gasc
====

A Generic Mesos Gang Scheduler (GaSc) for HPC tooling. Schedules in-process-tree ssh daemons, announce node lists and calls tool command.

### Build instructions

    $ make
    mpic++ mpi_hello.cpp -o mpi_hello
    g++ -std=c++11 -g -O2 -I. -I/usr/local/include/mesos main.cpp gasc.cpp -lmesos -o mesos-gasc

### Usage

    Usage: ./mesos-gasc -n <# instances> -c <# cpus> -m <# memory> <master> -- <mpirun arguments...>
    -n <# instances>        Number of instances / tasks (long)
    -c <# cpus>             CPU fraction per instance (float)
    -m <# memory>           Memory in megabytes per instance (long)
    <master>                The address of the Mesos master
    
### Example

    ./mesos-gasc -n 5 -c 0.5 -m 128 localhost:5050 -- ./mpi_hello -n 64 -f hosts.txt
    
