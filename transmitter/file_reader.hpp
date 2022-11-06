#include "wave_reader.hpp"


class FileReader : public Reader {

public:
    FileReader(std::string filename, int sampleRate);

    std::vector<Sample> GetData (unsigned quantity, bool &stop);
    std::string GetDetails ();
    int GetSampleRate();

private:
    std::string filename;
    int sampleRate;
    int fd;

};