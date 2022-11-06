#include <vector>
#include <string>
#include <climits>
#include <cstdint>

class Sample
{
public:
    Sample(float);
    Sample(uint8_t *data, unsigned channels, unsigned bitsPerChannel);
    float GetMonoValue() const;
protected:
    float value;
};

class Reader {

public:
    Reader() {};
    virtual std::vector<Sample> GetData (unsigned quantity, bool &stop) = 0;

    virtual std::string GetDetails () = 0;
    virtual int GetSampleRate() = 0;
};