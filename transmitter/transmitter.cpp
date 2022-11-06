/*
    FM Transmitter - use Raspberry Pi as FM transmitter

    Copyright (c) 2022, Marcin Kondej
    All rights reserved.

    See https://github.com/markondej/fm_transmitter

    Redistribution and use in source and binary forms, with or without modification, are
    permitted provided that the following conditions are met:

    1. Redistributions of source code must retain the above copyright notice, this list
    of conditions and the following disclaimer.

    2. Redistributions in binary form must reproduce the above copyright notice, this
    list of conditions and the following disclaimer in the documentation and/or other
    materials provided with the distribution.

    3. Neither the name of the copyright holder nor the names of its contributors may be
    used to endorse or promote products derived from this software without specific
    prior written permission.

    THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY
    EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
    OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT
    SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
    INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED
    TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
    BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
    CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY
    WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#include "transmitter.hpp"
#include "mailbox.h"
#include <bcm_host.h>
#include <thread>
#include <chrono>
#include <cmath>
#include <fcntl.h>
#include <sys/mman.h>
#include <iostream>

#include "rpi_constants.hpp"

struct ClockRegisters {
    uint32_t ctl;
    uint32_t div;
};

struct PWMRegisters {
    uint32_t ctl;
    uint32_t status;
    uint32_t dmaConf;
    uint32_t reserved0;
    uint32_t chn1Range;
    uint32_t chn1Data;
    uint32_t fifoIn;
    uint32_t reserved1;
    uint32_t chn2Range;
    uint32_t chn2Data;
};

struct DMAControllBlock {
    uint32_t transferInfo;
    uint32_t srcAddress;
    uint32_t dstAddress;
    uint32_t transferLen;
    uint32_t stride;
    uint32_t nextCbAddress;
    uint32_t reserved0;
    uint32_t reserved1;
};

struct DMARegisters {
    uint32_t ctlStatus;
    uint32_t cbAddress;
    uint32_t transferInfo;
    uint32_t srcAddress;
    uint32_t dstAddress;
    uint32_t transferLen;
    uint32_t stride;
    uint32_t nextCbAddress;
    uint32_t debug;
};

class Peripherals
{
    public:
        virtual ~Peripherals() {
            munmap(peripherals, GetSize());
        }
        Peripherals(const Peripherals &) = delete;
        Peripherals(Peripherals &&) = delete;
        Peripherals &operator=(const Peripherals &) = delete;
        static Peripherals &GetInstance() {
            static Peripherals instance;
            return instance;
        }
        uintptr_t GetPhysicalAddress(volatile void *object) const {
            return PERIPHERALS_PHYS_BASE + (reinterpret_cast<uintptr_t>(object) - reinterpret_cast<uintptr_t>(peripherals));
        }
        uintptr_t GetVirtualAddress(uintptr_t offset) const {
            return reinterpret_cast<uintptr_t>(peripherals) + offset;
        }
        static uintptr_t GetVirtualBaseAddress() {
            return (bcm_host_get_peripheral_size() == BCM2711_PERI_VIRT_BASE) ? BCM2711_PERI_VIRT_BASE : bcm_host_get_peripheral_address();
        }
        static float GetClockFrequency() {
            return (Peripherals::GetVirtualBaseAddress() == BCM2711_PERI_VIRT_BASE) ? BCM2711_PLLD_FREQ : BCM2835_PLLD_FREQ;
        }
    private:
        Peripherals() {
            int memFd;
            if ((memFd = open("/dev/mem", O_RDWR | O_SYNC)) < 0) {
                throw std::runtime_error("Cannot open /dev/mem file (permission denied)");
            }

            peripherals = mmap(nullptr, GetSize(), PROT_READ | PROT_WRITE, MAP_SHARED, memFd, GetVirtualBaseAddress());
            close(memFd);
            if (peripherals == MAP_FAILED) {
                throw std::runtime_error("Cannot obtain access to peripherals (mmap error)");
            }
        }
        unsigned GetSize() {
            unsigned size = bcm_host_get_peripheral_size();
            if (size == BCM2711_PERI_VIRT_BASE) {
                size = 0x01000000;
            }
            return size;
        }

        void *peripherals;
};

class AllocatedMemory
{
    public:
        AllocatedMemory() = delete;
        AllocatedMemory(unsigned size) {
            mBoxFd = mbox_open();
            memSize = size;
            if (memSize % PAGE_SIZE) {
                memSize = (memSize / PAGE_SIZE + 1) * PAGE_SIZE;
            }
            memHandle = mem_alloc(mBoxFd, size, PAGE_SIZE, (Peripherals::GetVirtualBaseAddress() == BCM2835_PERI_VIRT_BASE) ? BCM2835_MEM_FLAG : BCM2711_MEM_FLAG);
            if (!memHandle) {
                mbox_close(mBoxFd);
                memSize = 0;
                throw std::runtime_error("Cannot allocate memory (" + std::to_string(size) + " bytes)");
            }
            memAddress = mem_lock(mBoxFd, memHandle);
            memAllocated = mapmem(memAddress & ~0xc0000000, memSize);
        }
        virtual ~AllocatedMemory() {
            unmapmem(memAllocated, memSize);
            mem_unlock(mBoxFd, memHandle);
            mem_free(mBoxFd, memHandle);
            mbox_close(mBoxFd);
            memSize = 0;
        }
        AllocatedMemory(const AllocatedMemory &) = delete;
        AllocatedMemory(AllocatedMemory &&) = delete;
        AllocatedMemory &operator=(const AllocatedMemory &) = delete;
        uintptr_t GetPhysicalAddress(volatile void *object) const {
            return (memSize) ? memAddress + (reinterpret_cast<uintptr_t>(object) - reinterpret_cast<uintptr_t>(memAllocated)) : 0x00000000;
        }
        uintptr_t GetBaseAddress() const {
            return reinterpret_cast<uintptr_t>(memAllocated);
        }
    private:
        unsigned memSize, memHandle;
        uintptr_t memAddress;
        void *memAllocated;
        int mBoxFd;
};

class Device
{
    public:
        Device() {
            peripherals = &Peripherals::GetInstance();
        }
    protected:
        Peripherals *peripherals;
};

class ClockDevice : public Device
{
    public:
        ClockDevice(uintptr_t address, unsigned divisor) {
            clock = reinterpret_cast<ClockRegisters *>(peripherals->GetVirtualAddress(address));
            clock->ctl = CLK_PASSWORD | CLK_CTL_SRC_PLLD;
            std::this_thread::sleep_for(std::chrono::microseconds(100));
            clock->div = CLK_PASSWORD | divisor;
            clock->ctl = CLK_PASSWORD | CLK_CTL_MASH(0x1) | CLK_CTL_ENAB | CLK_CTL_SRC_PLLD;
        }
        virtual ~ClockDevice() {
            clock->ctl = CLK_PASSWORD | CLK_CTL_SRC_PLLD;
        }
    protected:
        volatile ClockRegisters *clock;
};

class ClockOutput : public ClockDevice
{
    public:
        ClockOutput(unsigned divisor) : ClockDevice(CLK0_BASE_OFFSET, divisor) {
            output = reinterpret_cast<uint32_t *>(peripherals->GetVirtualAddress(GPIO_BASE_OFFSET));
            *output = (*output & 0xffff8fff) | (0x04 << 12);
        }
        virtual ~ClockOutput() {
            *output = (*output & 0xffff8fff) | (0x01 << 12);
        }
        void SetDivisor(unsigned divisor) {
            clock->div = CLK_PASSWORD | (0xffffff & divisor);
        }
        volatile uint32_t &GetDivisor() {
            return clock->div;
        }
    private:
        volatile uint32_t *output;
};

class PWMController : public ClockDevice
{
    public:
        //PWMController() = delete;
        PWMController(unsigned sampleRate) : ClockDevice(PWMCLK_BASE_OFFSET, static_cast<unsigned>(Peripherals::GetClockFrequency() * 1000000.f * (0x01 << 12) / (PWM_WRITES_PER_SAMPLE * PWM_CHANNEL_RANGE * sampleRate))) {
            pwm = reinterpret_cast<PWMRegisters *>(peripherals->GetVirtualAddress(PWM_BASE_OFFSET));
            pwm->ctl = 0x00000000;
            std::this_thread::sleep_for(std::chrono::microseconds(100));
            pwm->status = PWM_STA_BERR | PWM_STA_GAPO1 | PWM_STA_RERR1 | PWM_STA_WERR1;
            pwm->ctl = PWM_CTL_CLRF1;
            std::this_thread::sleep_for(std::chrono::microseconds(100));
            pwm->chn1Range = PWM_CHANNEL_RANGE;
            pwm->dmaConf = PWM_DMAC_ENAB | PWM_DMAC_PANIC(0x7) | PWM_DMAC_DREQ(0x7);
            pwm->ctl = PWM_CTL_USEF1 | PWM_CTL_RPTL1 | PWM_CTL_MODE1 | PWM_CTL_PWEN1;
        }
        virtual ~PWMController() {
            pwm->ctl = 0x00000000;
        }
        volatile uint32_t &GetFifoIn() {
            return pwm->fifoIn;
        }
    private:
        volatile PWMRegisters *pwm;
};

class DMAController : public Device
{
    public:
        DMAController(uint32_t address) {
            dma = (DMARegisters *)(peripherals->GetVirtualAddress(DMA0_BASE_OFFSET));
            dma->ctlStatus = DMA_CS_RESET;
            std::this_thread::sleep_for(std::chrono::microseconds(100));
            dma->ctlStatus = DMA_CS_INT | DMA_CS_END;
            dma->cbAddress = address;
            dma->ctlStatus = DMA_CS_PANIC_PRIORITY(0xf) | DMA_CS_PRIORITY(0xf) | DMA_CS_ACTIVE;
        }
        virtual ~DMAController() {
            dma->ctlStatus = DMA_CS_RESET;
        }
        void SetControllBlockAddress(uint32_t address) {
            dma->cbAddress = address;
        }
        volatile uint32_t &GetControllBlockAddress() {
            return dma->cbAddress;
        }
    private:
        volatile DMARegisters *dma;
};

Transmitter::Transmitter()
    : output(nullptr), stop(true)
{
}

Transmitter::~Transmitter() {
    if (output != nullptr) {
        delete output;
    }
}

void Transmitter::Transmit(Reader &reader, float frequency, float bandwidth, bool preserveCarrier)
{
    stop = false;

    unsigned bufferSize = static_cast<unsigned>(reader.GetSampleRate());

    unsigned clockDivisor = static_cast<unsigned>(round(Peripherals::GetClockFrequency() * (0x01 << 12) / frequency));
    unsigned divisorRange = clockDivisor - static_cast<unsigned>(round(Peripherals::GetClockFrequency() * (0x01 << 12) / (frequency + 0.0005f * bandwidth)));

    if (output == nullptr) {
        output = new ClockOutput(clockDivisor);
    }

    auto finally = [&]() {
        if (!preserveCarrier) {
            delete output;
        }
    };
    try {
        TransmitViaDma(reader, *output, reader.GetSampleRate(), bufferSize, clockDivisor, divisorRange);
    } catch (...) {
        finally();
        throw;
    }
    finally();
}

void Transmitter::Stop()
{
    stop = true;
}

void Transmitter::TransmitViaDma(Reader &reader, ClockOutput &output, unsigned sampleRate, unsigned bufferSize, unsigned clockDivisor, unsigned divisorRange)
{
    AllocatedMemory allocated(sizeof(uint32_t) * bufferSize + sizeof(DMAControllBlock) * (2 * bufferSize) + sizeof(uint32_t));

    std::vector<Sample> data = reader.GetData(bufferSize, stop);
    if (data.empty()) {
        return;
    }

    bool eof = false;
    if (data.size() < bufferSize) {
        bufferSize = data.size();
        eof = true;
    }

    PWMController pwm(sampleRate);
    Peripherals &peripherals = Peripherals::GetInstance();

    unsigned cbOffset = 0;

    volatile DMAControllBlock *dmaCb = reinterpret_cast<DMAControllBlock *>(allocated.GetBaseAddress());
    volatile uint32_t *clkDiv = reinterpret_cast<uint32_t *>(reinterpret_cast<uintptr_t>(dmaCb) + 2 * sizeof(DMAControllBlock) * bufferSize);
    volatile uint32_t *pwmFifoData = reinterpret_cast<uint32_t *>(reinterpret_cast<uintptr_t>(clkDiv) + sizeof(uint32_t) * bufferSize);
    for (unsigned i = 0; i < bufferSize; i++) {
        float value = data[i].GetMonoValue();
        clkDiv[i] = CLK_PASSWORD | (clockDivisor - static_cast<int32_t>(round(value * divisorRange)));
        dmaCb[cbOffset].transferInfo = DMA_TI_NO_WIDE_BURST | DMA_TI_WAIT_RESP;;
        dmaCb[cbOffset].srcAddress = allocated.GetPhysicalAddress(&clkDiv[i]);
        dmaCb[cbOffset].dstAddress = peripherals.GetPhysicalAddress(&output.GetDivisor());
        dmaCb[cbOffset].transferLen = sizeof(uint32_t);
        dmaCb[cbOffset].stride = 0;
        dmaCb[cbOffset].nextCbAddress = allocated.GetPhysicalAddress(&dmaCb[cbOffset + 1]);
        cbOffset++;

        std::cout << value << std::endl;

        dmaCb[cbOffset].transferInfo = DMA_TI_NO_WIDE_BURST | DMA_TI_PERMAP(0x5) | DMA_TI_DEST_DREQ | DMA_TI_WAIT_RESP;
        dmaCb[cbOffset].srcAddress = allocated.GetPhysicalAddress(pwmFifoData);
        dmaCb[cbOffset].dstAddress = peripherals.GetPhysicalAddress(&pwm.GetFifoIn());
        dmaCb[cbOffset].transferLen = sizeof(uint32_t) * PWM_WRITES_PER_SAMPLE;
        dmaCb[cbOffset].stride = 0;
        dmaCb[cbOffset].nextCbAddress = allocated.GetPhysicalAddress((i < bufferSize - 1) ? &dmaCb[cbOffset + 1] : dmaCb);
        cbOffset++;


    }
    *pwmFifoData = 0x00000000;

    DMAController dma(allocated.GetPhysicalAddress(dmaCb));

    std::this_thread::sleep_for(std::chrono::microseconds(BUFFER_TIME / 10));

    auto finally = [&]() {
        dmaCb[(cbOffset < 2 * bufferSize) ? cbOffset : 0].nextCbAddress = 0x00000000;
        while (dma.GetControllBlockAddress() != 0x00000000) {
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
        data.clear();
        stop = true;
    };
    try {
        while (!eof && !stop) {
            data = reader.GetData(bufferSize, stop);
            if (!data.size()) {
                break;
            }
            cbOffset = 0;
            eof = data.size() < bufferSize;
            for (std::size_t i = 0; i < data.size(); i++) {
                float value = data[i].GetMonoValue();
                std::cout << value << std::endl;
                while (i == ((dma.GetControllBlockAddress() - allocated.GetPhysicalAddress(dmaCb)) / (2 * sizeof(DMAControllBlock)))) {
                    std::this_thread::sleep_for(std::chrono::microseconds(BUFFER_TIME / 10));
                }
                clkDiv[i] = CLK_PASSWORD | (0xffffff & (clockDivisor - static_cast<int>(round(value * divisorRange))));
                cbOffset += 2;
            }
        }
    } catch (...) {
        finally();
        throw;
    }
    finally();
}
