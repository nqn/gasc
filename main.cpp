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

bool expect(string found, string expect) {
  if (found != expect) {
    cerr << "Could not parse arguments: expected '" << expect
         << "' but found '" << found << "'" << endl;
    return false;
  }
  return true;
}

bool parse(
    int argc,
    char** argv,
    int& instances,
    double& cpus,
    long& memory,
    string& master,
    string& command) {
  
  enum {
    instancesFlag,
    instancesInt,
    cpusFlag,
    cpusDouble,
    memoryFlag,
    memoryLong,
    masterString,
    split,
    commandString
  } state = instancesFlag;

  const int expectedArguments = 10;
  if (argc < expectedArguments) {
    cerr << "Wrong number of arguments. "
         << "Expected " << expectedArguments << " but found "
         << argc << endl;
    return false;
  }

  // TODO(nnielsen): Dipatch on flag rather than enforcing order.
  for (int i = 1; i < argc; i++) {
    string argument = argv[i];

    if (state == instancesFlag) {
      if (!expect(argument, "-n")) {
        return false;
      }

      state = instancesInt;
    } else if (state == instancesInt) {
      instances = atoi(argument.c_str());
      if (instances <= 0) {
        cerr << "Number of instances must be greater than zero. "
             << "Found: '" << argument << "'" << endl;
        return false;
      }

      state = cpusFlag;
    } else if (state == cpusFlag) {
      if (!expect(argument, "-c")) {
        return false;
      }

      state = cpusDouble;
    } else if (state == cpusDouble) {
      cpus = atof(argument.c_str()); 
      if (cpus <= 0.0) {
        cerr << "CPU fraction must be greater than zero. "
             << "Found: '" << argument << "'" << endl;
        return false;
      }

      state = memoryFlag;
    } else if (state == memoryFlag) {
      if (!expect(argument, "-m")) {
        return false;
      }

      state = memoryLong;
    } else if (state == memoryLong) {
      memory = atol(argument.c_str());
      if (memory <= 0) {
        cerr << "Memory (MB) need to be greater than zero. "
             << "Found: '" << argument << "'" << endl;
        return false;
      }

      state = masterString;
    } else if (state == masterString) {
      master = argument;
      state = split;
    } else if (state == split) {{
      if (!expect(argument, "--")) {
        return false;
      }

      state = commandString;
      break;
    }
    }
  }

  if (state != commandString) {
    return false;
  }

  // TODO(nnielsen): Get argument start from "--" position.
  for (int i = 8; i < argc - 8; i++) {
    command += string(argv[0]) + " ";
  }

  if (debug) {
    cout << "Tool command: '" << command << "'";
  }

  return true;
}

int main(int argc, char** argv) {
  // TODO(nnielsen): Output node list through environment variable or
  // argument replacement.
  // TODO(nnielsen): Enable framework fail-over!
  // TODO(nnielsen): Accept zookeeper string.
  // TODO(nnielsen): Enable 'exclusive' instances i.e. only run one instance per
  // node.

  int instances = 5;
  double cpus = 1;
  long memory = 128;

  string master = "localhost:5050";
  string command = "mpirun -f hosts.txt -n 64 ./mpi_hello";

  if (!parse(argc, argv, instances, cpus, memory, master, command)) {
    usage(argv[0]);
    return EXIT_FAILURE;
  }

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
