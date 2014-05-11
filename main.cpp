#include <stdio.h>
#include <stdlib.h>
#include <signal.h>

#include <string>
#include <iostream>

#include <gasc.hpp>

using namespace mesos;

using std::cerr;
using std::cout;
using std::endl;
using std::string;

bool debug = true;

void usage(string program) {
  cerr << "Usage: " << program << " -n <# instances> -c <# cpus> "
       << "-m <# memory> <master> -- <mpirun arguments...>"
       << endl;
  cerr << endl;
  cerr << "-n <# instances>\tNumber of instances / tasks (long)" << endl;
  cerr << "-c <# cpus>\t\tCPU fraction per instance (float)" << endl;
  cerr << "-m <# memory>\t\tMemory in megabytes per instance (long)" << endl;
  cerr << "<master>\t\tThe address of the Mesos master" << endl;
}

int main(int argc, char** argv) {
  // TODO(nnielsen): Output node list through environment variable or
  // argument replacement.
  // TODO(nnielsen): Enable framework fail-over!
  // TODO(nnielsen): Accept zookeeper string.
  // TODO(nnielsen): Enable 'exclusive' instances i.e. only run one instance per
  // node.
  // usage(argv[0]);

  int instances = 5;
  int cpus = 1;
  int memory = 128;

  string master = "localhost:5050";
  string command = "mpirun -f hosts.txt -n 64 ./mpi_hello";

  FrameworkInfo framework;
  framework.set_user("root");
  framework.set_name("GASC: " + command);

  GascScheduler scheduler(cpus, memory, instances, command);
  MesosSchedulerDriver* driver = new MesosSchedulerDriver(
      &scheduler, framework, master);
  int status = driver->run() == DRIVER_STOPPED ? 0 : 1;

  // Ensure that the driver process terminates.
  driver->stop();

  delete driver;
  return status;
}
