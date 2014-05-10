#include <stdio.h>
#include <stdlib.h>

#include <string>
#include <iostream>
#include <vector>
#include <sstream>
#include <map>

#include <mesos/mesos.hpp>
#include <mesos/scheduler.hpp>

using namespace mesos;

using std::cerr;
using std::cout;
using std::endl;
using std::string;
using std::vector;

static bool debug = true;

class GascScheduler : public Scheduler
{
	public:
		GascScheduler(
				double cpusPerInstance,
				long memoryPerInstance,
				long instances)
			: tasksToLaunch(instances),
			tasksFinished(0),
			cpusPerInstance(cpusPerInstance),
			memoryPerInstance(memoryPerInstance),
			instances(instances),
			nextTaskId(0) {}

		virtual ~GascScheduler() {}

		virtual void registered(SchedulerDriver*,
				const FrameworkID&,
				const MasterInfo&)
		{
			if (debug) {
				cout << "GASC registered" << endl;
			}
		}

		virtual void reregistered(SchedulerDriver*, const MasterInfo& masterInfo) {}

		virtual void disconnected(SchedulerDriver* driver) {
			cerr << "GASC disconnected" << endl;
		}

		virtual void resourceOffers(SchedulerDriver* driver,
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
							<< " but was offered " << availableCpus;
					}

					driver->declineOffer(offer.id());
					continue;
				}

				if (availableMemory < memoryPerInstance) {
					if (debug) {
						cerr << "Declining offer: need " << memoryPerInstance << " mb"
							<< " memory but was offered " << availableMemory;
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
					string portString = std::to_string(8000 + taskId);

					TaskInfo task;
					task.set_name("GSAC daemon #" + taskIdString);
					task.mutable_task_id()->set_value(taskIdString);
					task.mutable_slave_id()->CopyFrom(offer.slave_id());

					task.mutable_command()->set_value("sshd -p " + portString + " -D");

					task.add_resources()->MergeFrom(cpuResource);
					task.add_resources()->MergeFrom(memoryResource);

					if (debug) {
						cout << "Starting daemon #" << taskId << " on "
							<< offer.hostname() << endl;
					}

					tasks.push_back(task);
				}

				driver->launchTasks(offer.id(), tasks);
			}
		}

		virtual void offerRescinded(SchedulerDriver* driver,
				const OfferID& offerId) {}

		virtual void statusUpdate(SchedulerDriver* driver, const TaskStatus& status)
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

			if ((status.state() == TASK_FAILED) &&
					(status.state() == TASK_LOST) &&
					(status.state() == TASK_KILLED)) {
				tasksToLaunch++;
			} else if (status.state() == TASK_RUNNING) {
				tasksRunning++;
				if (tasksRunning == instances) {
					// TODO(nnielsen): launch mpirun.
				}
			} else if (status.state() == TASK_FINISHED) {
				tasksFinished++;
				if (tasksFinished == instances)
					driver->stop();
			}
		}

		virtual void frameworkMessage(SchedulerDriver* driver,
				const ExecutorID& executorId,
				const SlaveID& slaveId,
				const string& data) {}

		virtual void slaveLost(SchedulerDriver* driver, const SlaveID& sid) {
			cerr << "Lost slave " << sid.value() << endl;
		}

		virtual void executorLost(SchedulerDriver* driver,
				const ExecutorID& executorID,
				const SlaveID& slaveID,
				int status) {
			cerr << "Lost executor " << executorID.value() << endl;
		}

		virtual void error(SchedulerDriver* driver, const string& message)
		{
			cerr << "Detected framework error: " << message << endl;
		}

	private:
		string role;
		int tasksToLaunch;
		int tasksRunning;
		int tasksFinished;

		double cpusPerInstance;
		long memoryPerInstance;
		int instances;

		int nextTaskId;
};

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
	// usage(argv[0]);

	int instances = 5;
	int cpus = 1;
	int memory = 128;

	string master = "192.168.56.1:5050";
	string command = "mpirun -n 512 ./helloworld";

	FrameworkInfo framework;
	framework.set_user("");
	framework.set_name("GASC: " + command);

	GascScheduler scheduler(cpus, memory, instances);
	MesosSchedulerDriver* driver = new MesosSchedulerDriver(
			&scheduler, framework, master);
	int status = driver->run() == DRIVER_STOPPED ? 0 : 1;

	// Ensure that the driver process terminates.
	driver->stop();

	delete driver;
	return status;
}
