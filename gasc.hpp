#ifndef GASC_H
#define GASC_H

#include <vector>
#include <map>

#include <mesos/mesos.hpp>
#include <mesos/scheduler.hpp>

extern bool debug;

class GascDaemon
{
public:
  GascDaemon(int state, std::string hostname, int port);

  void changeState(int state);

  std::string hostnamePort() const;

private:
  int state_;
  std::string hostname;
  int port;
};


class GascScheduler : public mesos::Scheduler
{
public:
  GascScheduler(
      double cpusPerInstance,
      long memoryPerInstance,
      long instances,
      std::string command);

  virtual ~GascScheduler() {}

  virtual void registered(mesos::SchedulerDriver*,
      const mesos::FrameworkID&,
      const mesos::MasterInfo&);

  virtual void reregistered(
      mesos::SchedulerDriver*,
      const mesos::MasterInfo& masterInfo);

  virtual void disconnected(mesos::SchedulerDriver* driver);

  virtual void resourceOffers(
      mesos::SchedulerDriver* driver,
      const std::vector<mesos::Offer>& offers);

  virtual void offerRescinded(
      mesos::SchedulerDriver* driver,
      const mesos::OfferID& offerId);

  virtual void statusUpdate(
      mesos::SchedulerDriver* driver,
      const mesos::TaskStatus& status);

  virtual void frameworkMessage(
      mesos::SchedulerDriver* driver,
      const mesos::ExecutorID& executorId,
      const mesos::SlaveID& slaveId,
      const std::string& data);

  virtual void slaveLost(
      mesos::SchedulerDriver* driver,
      const mesos::SlaveID& sid);

  virtual void executorLost(
      mesos::SchedulerDriver* driver,
      const mesos::ExecutorID& executorID,
      const mesos::SlaveID& slaveID,
      int status);

  virtual void error(
      mesos::SchedulerDriver* driver,
      const std::string& message);

private:
  std::string role;
  int tasksToLaunch;
  int tasksRunning;
  int tasksFinished;

  double cpusPerInstance;
  long memoryPerInstance;
  int instances;

  std::string command;

  int nextTaskId;

  std::map<int, GascDaemon> daemons;

  void runTool(mesos::SchedulerDriver* driver);
};

#endif
