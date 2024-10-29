// TRACING
#include <cstdio>
#ifndef TRACE_SCHED
#define TRACE_SCHED 0
#define traceSched(fmt...)  do { if (TRACE_SCHED) {\
			printf("[%s: %s: %d] ", __FILE__, __PRETTY_FUNCTION__, __LINE__), \
			printf(fmt); fflush(stdout); }  } while(0)
#endif

#ifndef TRACE_DES 
#define TRACE_DES 0 
#define traceDES(fmt...)  do { if (TRACE_DES) {\
			printf("[%s: %s: %d] ", __FILE__, __PRETTY_FUNCTION__, __LINE__), \
			printf(fmt); fflush(stdout); }  } while(0) 
#endif 

#ifndef SCHED_H
#define SCHED_H

#include <string>
#include <deque>
#include <list>
#include <vector>

extern bool SHOW_SCHED_READY_QUEUE;
extern int EVENT_COUNTER;
extern bool SHOW_EVENT_QUEUE;
extern bool SHOW_PRIO_PREEMPT;
typedef enum { 
	STATE_CREATED,
	STATE_READY,
	STATE_RUNNING,
	STATE_BLOCKED
} ProcessState;

typedef enum {
	TRANS_TO_READY,
	TRANS_TO_RUN,
	TRANS_TO_PREEMPT,
	TRANS_TO_BLOCK
} Transition;

const std::string PROCESS_STATE_TO_STR[] {"CREATED", "READY", "RUNNG", "BLOCK"};
const std::string TRANSITION_TO_STR[] {"READY", "RUNNG", "PREEMPT", "BLOCK"};

struct Event;
struct Process {
	const int pid;
	const int arrival_time;
	const int total_cpu_time;
	const int cpu_burst;
	const int io_burst;
	const int static_prio;

	Event *pending_evt = nullptr;
	int time_to_pending_evt = 0;
	int dynamic_prio;
	int state_time_stamp;
	int rem_cpu_time;
	int rem_cpu_burst = 0; // unused cpu_burst due to preemption
	int wait_time = 0; // time in ready state	
	int io_time = 0; // time performing IO
	int finish_time = 0;
	Process(int pid, int at, int tc, int cb, int io, int prio); 
};

struct Event {
	const int eid;
	Process *process = nullptr;
	const int time_stamp;
	const ProcessState old_state;
	const ProcessState new_state;
	const Transition transition;
	Event(Process *proc, int ts, ProcessState os, ProcessState ns, Transition t);	
};

class DES {
public:
	std::list<Event*> eventQ;
	DES(std::vector<Process> &processes); 
	void PutEvent(Event *evt);
	Event* GetEvent();
	void RemoveEvent(int eid);
	void ShowEventQ();
	void TraceEventQ();	
	int GetNextEventTime();
	Event* GetPendingEventByPID(int pid);
};

/*
 * Base Clase for all schedulers
 */
class Scheduler {
public:	
	const std::string sched_type = "";
	int quantum = 10 * 1000;
	Scheduler(std::string type);
	Scheduler(std::string type, int quantum);
	virtual void AddProcess(Process *p) = 0;
	virtual Process* GetNextProcess() = 0;
	virtual void ShowReadyQueue() = 0;
	virtual bool TestPreempt(Process *proc, int current_time, Process *curr_running_proc) = 0;
	virtual ~Scheduler() = default;
};

/*
 * FCFS Scheduler / Round Robin (FCFS + preemption) Scheduler
 */
class FCFS_scheduler: public Scheduler {
private:
	std::deque<Process*> readyQ;
public:
	FCFS_scheduler();
	FCFS_scheduler(std::string type, int quantum);
	void AddProcess(Process *p);
	Process* GetNextProcess();
	bool TestPreempt(Process *proc, int current_time, Process *curr_running_proc) { return false; }
	void ShowReadyQueue();
};

/*
 * LCFS Scheduler
 */

class LCFS_scheduler: public Scheduler {
private:
	std::deque<Process*> readyQ;
public:
	LCFS_scheduler();
	void AddProcess(Process *p);
	Process* GetNextProcess();
	bool TestPreempt(Process *proc, int current_time, Process *curr_running_proc) { return false; }
	void ShowReadyQueue();
};

/*
 * SRTF Scheduler
 */

class SRTF_scheduler: public Scheduler {
private:
	std::list<Process*> readyQ;
public:
	SRTF_scheduler();
	void AddProcess(Process *p);
	Process* GetNextProcess();
	bool TestPreempt(Process *proc, int current_time, Process *curr_running_proc) { return false; }
	void ShowReadyQueue();
};

/*
 * PRIO (Priority) / PREPRIO (Preemption Priority) Scheduler
 */

class PRIO_scheduler: public Scheduler {
private:
	std::vector<std::deque<Process*>> activeQ;
    std::vector<std::deque<Process*>> expiredQ;
	void PrintMLQueue(std::vector<std::deque<Process*>> &ml_queue);
	bool IsEmptyQueue_(std::vector<std::deque<Process*>> &Q); 
	void TraceQueue(std::vector<std::deque<Process*>> &ml_queue);
public:
	const int maxprio;
	const bool priority_preempt;
	PRIO_scheduler(int quantum, int maxprio);
	PRIO_scheduler(int quantum, int maxprio, bool priority_preempt);
	void AddProcess(Process *p);
	Process* GetNextProcess();
	bool TestPreempt(Process *proc, int current_time, Process *curr_running_proc);
	void ShowReadyQueue();
};



#endif	

