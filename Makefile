all: example mesos-gasc

SOURCES = main.cpp gasc.cpp
INCLUDES = gasc.hpp
CXXFLAGS += -std=c++11 -g -O2 -I. -I/usr/local/include/mesos
MPICXX = mpic++

example: mpi_hello.cpp
	$(MPICXX) mpi_hello.cpp -o mpi_hello

mesos-gasc: $(INCLUDES) $(SOURCES)
	$(CXX) $(CXXFLAGS) $(SOURCES) -lmesos -o mesos-gasc
