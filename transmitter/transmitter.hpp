

#pragma once

// #include "wave_reader.hpp"
#include "file_reader.hpp"
#include <mutex>

class ClockOutput;

class Transmitter
{
    public:
        Transmitter();
        virtual ~Transmitter();
        void Transmit(Reader &reader, float frequency, float bandwidth, bool preserveCarrier);
        void Stop();
    private:
        void TransmitViaDma(Reader &reader, ClockOutput &output, unsigned sampleRate, unsigned bufferSize, unsigned clockDivisor, unsigned divisorRange);

        ClockOutput *output;
        std::mutex access;
        bool stop;
};
