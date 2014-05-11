all: mesos-gasc

SOURCES = main.cpp
INCLUDES =
CXXFLAGS += -pthread -std=c++11 -g -O2 -I. -I/usr/local/include/mesos

mesos-gasc: $(INCLUDES) $(SOURCES)
	$(CXX) $(CXXFLAGS) $(SOURCES) -lmesos -o mesos-gasc
