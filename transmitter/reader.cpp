#include "reader.hpp"

Sample::Sample(float val) {
    value = val;
}

Sample::Sample(uint8_t *data, unsigned channels, unsigned bitsPerChannel)
    : value(0.f)
{
    int sum = 0;
    int16_t *channelValues = new int16_t[channels];
    for (unsigned i = 0; i < channels; i++) {
        switch (bitsPerChannel >> 3) {
        case 2:
            channelValues[i] = (data[((i + 1) << 1) - 1] << 8) | data[((i + 1) << 1) - 2];
            break;
        case 1:
            channelValues[i] = (static_cast<int16_t>(data[i]) - 0x80) << 8;
            break;
        }
        sum += channelValues[i];
    }
    value = 2 * sum / (static_cast<float>(USHRT_MAX) * channels);
    delete[] channelValues;
}

float Sample::GetMonoValue() const
{
    return value;
}