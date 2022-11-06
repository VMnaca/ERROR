
#include "transmitter.hpp"
#include <iostream>
#include <csignal>
#include <unistd.h>

bool stop = false;
Transmitter *transmitter = nullptr;

void sigIntHandler(int sigNum)
{
    if (transmitter != nullptr) {
        std::cout << "Stopping..." << std::endl;
        transmitter->Stop();
        stop = true;
    }
}

int main(int argc, char** argv)
{
    float frequency = 100.f, bandwidth = 200.f;
    int sampleRate = 60;
    bool showUsage = true, loop = false, wav = false;
    int opt, filesOffset;

    while ((opt = getopt(argc, argv, "rf:d:b:ws:")) != -1) {
        switch (opt) {
            case 'r':
                loop = true;
                break;
            case 'f':
                frequency = std::stof(optarg);
                break;
            case 'b':
                bandwidth = std::stof(optarg);
                break;
            case 'w':
                wav = true;
                break;
            case 's':
                sampleRate = std::stof(optarg);
                break;
        }
    }
    if (optind < argc) {
        filesOffset = optind;
        showUsage = false;
    }
    if (showUsage) {
        std::cout << "Usage: " << EXECUTABLE << " [-f <frequency>] [-b <bandwidth>] [-r] <file>" << std::endl;
        return 0;
    }

    int result = EXIT_SUCCESS;

    std::signal(SIGINT, sigIntHandler);
    std::signal(SIGTSTP, sigIntHandler);

    try {
        transmitter = new Transmitter();
        std::cout << "Broadcasting at " << frequency << " MHz with "
            << bandwidth << " kHz bandwidth" << std::endl;
        do {
            std::string filename = argv[optind++];
            if ((optind == argc) && loop) {
                optind = filesOffset;
            }
            
            

            if (wav) {
                WaveReader wr(filename != "-" ? filename : std::string(), stop);
                transmitter->Transmit(wr, frequency, bandwidth, optind < argc);
                wr.GetDetails();
            }
            else {
                FileReader fr(filename, sampleRate);
                transmitter->Transmit(fr, frequency, bandwidth, optind < argc);
                fr.GetDetails();
            }
            
        } while (!stop && (optind < argc));
    } catch (std::exception &catched) {
        std::cout << "Error: " << catched.what() << std::endl;
        result = EXIT_FAILURE;
    }
    if (transmitter != nullptr) {
        auto temp = transmitter;
        transmitter = nullptr;
        delete temp;
    }

    return result;
}
