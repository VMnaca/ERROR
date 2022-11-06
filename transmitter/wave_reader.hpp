

#pragma once

#include <cstdint>
#include <string>
#include <vector>
#include "reader.hpp"

#define WAVE_FORMAT_PCM 0x0001

struct WaveHeader
{
    uint8_t chunkID[4];
    uint32_t chunkSize;
    uint8_t format[4];
    uint8_t subchunk1ID[4];
    uint32_t subchunk1Size;
    uint16_t audioFormat;
    uint16_t channels;
    uint32_t sampleRate;
    uint32_t byteRate;
    uint16_t blockAlign;
    uint16_t bitsPerSample;
    uint8_t subchunk2ID[4];
    uint32_t subchunk2Size;
};



class WaveReader : public Reader
{
    public:
        WaveReader(const std::string &filename, bool &stop);
        virtual ~WaveReader();
        std::string GetFilename() const;
        const WaveHeader &GetHeader() const;
        std::vector<Sample> GetSamples(unsigned quantity, bool &stop);
        bool SetSampleOffset(unsigned offset);


        std::vector<Sample> GetData (unsigned quantity, bool &stop) {
            return this->GetSamples(quantity, stop);
        }
        std::string GetDetails () {
            WaveHeader header = this->GetHeader();
            return std::string("Playing: " + this->GetFilename() + ", " \
                + std::to_string(header.sampleRate) + " Hz, "           \
                + std::to_string(header.bitsPerSample) + " bits, "      \
                + ((header.channels > 0x01) ? "stereo" : "mono"));
        }
        int GetSampleRate () {
            return this->GetHeader().sampleRate;
        }
    private:
        std::vector<uint8_t> ReadData(unsigned bytesToRead, bool headerBytes, bool &stop);

        std::string filename;
        WaveHeader header;
        unsigned dataOffset, headerOffset, currentDataOffset;
        int fileDescriptor;
};
