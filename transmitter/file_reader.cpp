#include "file_reader.hpp"

#include <cstdint>
#include <stdexcept>
#include <cstring>
#include <thread>
#include <chrono>
#include <unistd.h>
#include <fcntl.h>
#include <climits>
#include <bitset>

FileReader::FileReader(std::string filename, int sampleRate) {

    this->filename = filename;

    this->sampleRate = sampleRate;

    fd = open(filename.c_str(), O_RDONLY);

    if (fd == -1) {
        throw std::runtime_error(std::string("Cannot open ") + filename + std::string(", file does not exist"));
    }
}

std::vector<Sample> FileReader::GetData (unsigned quantity, bool &stop) {

    void *data = malloc(quantity);
    std::vector<Sample> ret = std::vector<Sample>();

    int size = read(fd, data, quantity);

    char *chars = (char *)data;

    for (int i = 0; i < size; i++) {
        std::string bin_str = std::bitset<8>(chars[i]).to_string();
        for (unsigned int j = 0; j < bin_str.length(); j++) {
            ret.push_back(Sample((int)bin_str[j] - 48 ? -1.0f : 1.0f));
        }
    }
    return ret;
}

std::string FileReader::GetDetails () {
    return "Transfering: " + filename + " at " + std::to_string(sampleRate) + " Hz.\n";
}


int FileReader::GetSampleRate() {
    return sampleRate;
}