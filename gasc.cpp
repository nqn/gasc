#include <iostream>
#include <sstream>
#include <fstream>

#include <gasc.hpp>

using std::cerr;
using std::cout;
using std::endl;
using std::map;
using std::string;
using std::vector;

using namespace mesos;

GascDaemon::GascDaemon(int state, string hostname, int port)
  : state_(state),
    hostname(hostname),
    port(port) {
}

void GascDaemon::changeState(int state) {
  if (debug) {
    cout << "Daemon transitioned to " << state << endl;
  }
  state_ = state;
}

string GascDaemon::hostnamePort() const {
  string portString = std::to_string(port);
  return hostname + ":" + portString;
}

GascScheduler::GascScheduler(
    double cpusPerInstance,
    long memoryPerInstance,
    long instances,
    string command = "")
  : tasksToLaunch(instances),
    tasksFinished(0),
    cpusPerInstance(cpusPerInstance),
    memoryPerInstance(memoryPerInstance),
    instances(instances),
    command(command),
    nextTaskId(0) {}

void GascScheduler::registered(SchedulerDriver*,
    const FrameworkID&,
    const MasterInfo&)
{
  if (debug) {
    cout << "GASC registered" << endl;
  }
}

void GascScheduler::reregistered(SchedulerDriver*, const MasterInfo& masterInfo) {
  cerr << "GASC reregistered" << endl;
}

void GascScheduler::disconnected(SchedulerDriver* driver) {
  cerr << "GASC disconnected" << endl;
}

void GascScheduler::resourceOffers(SchedulerDriver* driver,
    const vector<Offer>& offers)
{
  for (size_t i = 0; i < offers.size(); i++) {
    const Offer& offer = offers[i];

    double availableCpus = 0;
    double availableMemory = 0;

    for (size_t j = 0; j < offer.resources_size(); j++) {
      const Resource& resource = offer.resources(j);

      if (resource.has_name()) {
        if (resource.has_scalar()) {
          double value = resource.scalar().value();
          if (resource.name() == "cpus") {
            availableCpus = value;
          } else if (resource.name() == "mem") {
            availableMemory = value;
          }
        }

        // TODO(nnielsen): Parse port range and disk.
      }
    }

    if (availableCpus < cpusPerInstance) {
      if (debug) {
        cerr << "Declining offer: need " << cpusPerInstance << " cpus"
             << " but was offered " << availableCpus << endl;
      }

      driver->declineOffer(offer.id());
      continue;
    }

    if (availableMemory < memoryPerInstance) {
      if (debug) {
        cerr << "Declining offer: need " << memoryPerInstance << " mb"
             << " memory but was offered "
             << availableMemory << " mb " << endl;
      }
      driver->declineOffer(offer.id());
      continue;
    }

    Resource cpuResource;
    cpuResource.set_name("cpus");
    cpuResource.set_type(Value_Type_SCALAR);
    cpuResource.mutable_scalar()->set_value(cpusPerInstance);

    Resource memoryResource;
    memoryResource.set_name("mem");
    memoryResource.set_type(Value_Type_SCALAR);
    memoryResource.mutable_scalar()->set_value(memoryPerInstance);

    // Launch tasks.
    vector<TaskInfo> tasks;

    while ((availableCpus >= cpusPerInstance) &&
        (availableMemory >= memoryPerInstance) &&
        (tasksToLaunch > 0)) {
      int taskId = nextTaskId++;
      tasksToLaunch--;

      availableCpus -= cpusPerInstance;
      availableMemory -= memoryPerInstance;

      string taskIdString = std::to_string(taskId);
      int port = 8000 + taskId;
      string portString = std::to_string(port);

      TaskInfo task;
      task.set_name("GSAC daemon #" + taskIdString);
      task.mutable_task_id()->set_value(taskIdString);
      task.mutable_slave_id()->CopyFrom(offer.slave_id());

      task.mutable_command()->set_value(
          "/usr/sbin/sshd -p " + portString +
          " -D -f /etc/ssh/sshd_config");

      task.add_resources()->MergeFrom(cpuResource);
      task.add_resources()->MergeFrom(memoryResource);

      if (debug) {
        cout << "Starting daemon #" << taskId << " on "
             << offer.hostname() << ":" << portString << endl;
      }

      daemons.insert(std::pair<int, GascDaemon>(
          taskId,
          GascDaemon(TASK_STAGING, offer.hostname(), port)));

      tasks.push_back(task);
    }

    driver->launchTasks(offer.id(), tasks);
  }
}

void GascScheduler::offerRescinded(SchedulerDriver* driver,
    const OfferID& offerId) {
  // We don't care as we don't keep offers around; we accept or
  // decline up frount.
}

void GascScheduler::statusUpdate(SchedulerDriver* driver, const TaskStatus& status)
{
  const char* statusString[7] = {
    "TASK_STARTING",
    "TASK_RUNNING",
    "TASK_FINISHED",
    "TASK_FAILED",
    "TASK_KILLED",
    "TASK_LOST",
    "TASK_STAGING"};

  int taskId;
  std::istringstream(status.task_id().value()) >> taskId;

  if (debug) {
    cout << "Task " << taskId << " is in state "
         << statusString[status.state()] << endl;
  }

  map<int, GascDaemon>::iterator daemonIterator = daemons.find(taskId);
  if (daemonIterator != daemons.end()) {
    GascDaemon& daemon = daemonIterator->second;
    daemon.changeState(status.state());
  }

  if ((status.state() == TASK_FAILED) &&
      (status.state() == TASK_LOST) &&
      (status.state() == TASK_KILLED)) {
    tasksToLaunch++;

    if (daemonIterator != daemons.end()) {
      daemons.erase(daemonIterator);
    }

  } else if (status.state() == TASK_RUNNING) {
    tasksRunning++;
    if (tasksRunning == instances) {
      runTool(driver);
    }
  } else if (status.state() == TASK_FINISHED) {
    tasksFinished++;
    if (tasksFinished == instances)
      driver->stop();
  }
}

void GascScheduler::frameworkMessage(SchedulerDriver* driver,
    const ExecutorID& executorId,
    const SlaveID& slaveId,
    const string& data) {
  // This should not happen as we are using the mesos-executor.
}

void GascScheduler::slaveLost(SchedulerDriver* driver, const SlaveID& sid) {
  cerr << "Lost slave " << sid.value() << endl
       << "Aborting run: GASC can't recover!" << endl;
  driver->stop();
}

void GascScheduler::executorLost(SchedulerDriver* driver,
    const ExecutorID& executorID,
    const SlaveID& slaveID,
    int status) {
  cerr << "Lost executor " << executorID.value() << endl
       << "Aborting run: GASC can't recover!" << endl;
  driver->stop();
}

void GascScheduler::error(SchedulerDriver* driver, const string& message)
{
  cerr << "Detected framework error: " << message << endl;
}

void GascScheduler::runTool(SchedulerDriver* driver) {
  // TODO(nnielsen): Make host name file configurable or
  // auto-generated.
  std::ofstream hosts("hosts.txt");

  map<int, GascDaemon>::iterator daemonIterator = daemons.begin();
  for (; daemonIterator != daemons.end(); daemonIterator++) {
    const GascDaemon& daemon = daemonIterator->second;

    hosts << daemon.hostnamePort() << endl;
  }
  hosts.close();

  if (debug) {
    cout << "Host file written in 'hosts.txt'" << endl
         << "" << endl;
  }

  FILE* toolStream = popen(command.c_str(), "w");

  // TODO(nnielsen): Do this in thread instead of blocking callback!
  pclose(toolStream);

  // TODO(nnielsen): close pipe.
  driver->stop();
}
