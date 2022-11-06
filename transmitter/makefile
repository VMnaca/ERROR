EXECUTABLE = fm_transmitter
VERSION = 0.9.6
FLAGS = -Wall -O3 -std=c++11
TRANSMITTER = -fno-strict-aliasing -I/opt/vc/include
ifeq ($(GPIO21), 1)
	TRANSMITTER += -DGPIO21
endif

all: fm_transmitter.o mailbox.o reader.o file_reader.o wave_reader.o transmitter.o
	g++ -o $(EXECUTABLE) fm_transmitter.o mailbox.o reader.o file_reader.o wave_reader.o transmitter.o -L/opt/vc/lib -lm -lpthread -lbcm_host

mailbox.o: mailbox.c mailbox.h
	g++ $(FLAGS) -c mailbox.c

reader.o: reader.cpp reader.hpp
	g++ $(FLAGS) -c reader.cpp

wave_reader.o: wave_reader.cpp wave_reader.hpp
	g++ $(FLAGS) -c wave_reader.cpp

transmitter.o: transmitter.cpp transmitter.hpp
	g++ $(FLAGS) $(TRANSMITTER) -c transmitter.cpp

file_reader.o: file_reader.cpp file_reader.hpp
	g++ $(FLAGS) -c file_reader.cpp

fm_transmitter.o: fm_transmitter.cpp
	g++ $(FLAGS) -DVERSION=\"$(VERSION)\" -DEXECUTABLE=\"$(EXECUTABLE)\" -c fm_transmitter.cpp

clean:
	rm *.o
