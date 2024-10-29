#include "sched.h"
#include <iostream>
using namespace std;


Process::Process(int pid, int at, int tc, int cb, int io, int prio) : 
	pid(pid), 
	arrival_time(at),
	total_cpu_time(tc),
	cpu_burst(cb),
	io_burst(io),
	static_prio(prio),
	dynamic_prio(prio - 1),
	state_time_stamp(at),
	rem_cpu_time(tc) {

	traceSched("Creating proces %d: %d, %d, %d, %d, static_prioity: %d\n",
				pid,
				arrival_time,
				total_cpu_time,
				cpu_burst,
				io_burst,
				static_prio
				);
}

Event::Event(Process *proc, int ts, ProcessState os, ProcessState ns, Transition t) :
	eid(EVENT_COUNTER++),
	process(proc),
	time_stamp(ts),
	old_state(os),
	new_state(ns),
	transition(t) {

	traceDES("Created event %d:  time stamp(%d), process(%d), old state(%s), new_state(%s), transition: %s\n",
		 eid,
		 time_stamp,
		 process->pid,
		 &PROCESS_STATE_TO_STR[old_state][0],
		 &PROCESS_STATE_TO_STR[new_state][0],
		 &TRANSITION_TO_STR[transition][0]
	);				 		
}	

DES::DES(vector<Process> &processes) {
	traceDES("Initializing DES Event Queue...\n");
	for (auto &proc: processes) {
	
		Event *evt = new Event(&proc,
	                   proc.arrival_time,
                   	   STATE_CREATED,
                       STATE_READY,
                      TRANS_TO_READY);
		
		// sort by time stamp
		auto iter = eventQ.begin();
		while (iter != eventQ.end()) {
			if (evt->time_stamp <= (*iter)->time_stamp) {
				if (TRACE_DES > 3) {
					traceDES("Compare event time stamp: %d, curr iter time stamp: %d\n", evt->time_stamp, (*iter)->time_stamp);
				}
				break;
			} 		
			iter++;
		}
		
		// if process arrives at the same time (same time stamps), order by pid
		while (iter != eventQ.end() && evt->time_stamp == (*iter)->time_stamp) {
			if (TRACE_DES > 3) {
				traceDES("Event process id %d, Current process id %d\n", evt->process->pid, (*iter)->process->pid);
			}
			if (evt->process->pid <= (*iter)->process->pid) {
				break;
			}
			iter++;
		}
		traceDES("Insert Event %d to EventQ\n", evt->eid);	
		eventQ.insert(iter, evt);
	}
} 


void DES::PutEvent(Event *evt) {
	traceDES("Put Event %d\n", evt->eid);
	auto iter = eventQ.begin();
	while (iter != eventQ.end()) {
		if (evt->time_stamp < (*iter)->time_stamp) {
			if (TRACE_DES > 3) {
				traceDES("Compare event time stamp: %d, curr iter time stamp: %d\n", 
							evt->time_stamp, 
							(*iter)->time_stamp);
			}
			break;
		} 		
		iter++;
	}
	/*	
	// if process arrives at the same time (same time stamps), order by pid
	while (iter != eventQ.end() && evt->time_stamp == (*iter)->time_stamp) {
		if (TRACE_DES > 3) {
			traceDES("Event process id %d, Current process id %d\n", evt->process->pid, (*iter)->process->pid);
		}
		if (evt->process->pid <= (*iter)->process->pid) {
			break;
		}
		iter++;
	}
	*/	
	eventQ.insert(iter, evt);
	if (TRACE_DES > 2) {
		this->TraceEventQ();
	}
}

/* Get the first event in the queue
 */


Event* DES::GetEvent() {
	if (eventQ.empty()) {
		return nullptr;
	}
	Event *evt = eventQ.front();
	eventQ.pop_front();
	return evt;
}

void DES::RemoveEvent(int eid) {
	auto iter = eventQ.begin();
	bool found = false;
	while (iter != eventQ.end()) {
    	if ((*iter)->eid == eid) {
        	traceDES("Removing event: %d\n", (*iter)->eid);
        	eventQ.erase(iter);
			found = true;
			break;
        }   
        iter++;
	}   
	if (not found) {
		traceDES("Event %d does not exist.\n", eid);
	}
}

void DES::ShowEventQ() {
	for (auto &e: eventQ) {
		// Timestamp:PID:State
		cout << "  " 
			 << e->time_stamp << ":" 
			 << e->process->pid << ":"
			 << TRANSITION_TO_STR[e->transition];
	}
	 
}

void DES::TraceEventQ() {
	traceDES("Trace EventQ ...\n");
	if (eventQ.empty()) {
		traceDES("No events left in EventQ.\n");
	} else  {
		for (auto &i: eventQ) {
			traceDES("Event %d: process: %d, time stamp: %d, old state: %d, new state: %d, transition: %d \n", 
				i->eid, 
				i->process->pid, 
				i->time_stamp,
				i->old_state,
				i->new_state,
				i->transition
			); 
		}
	}
}

int DES::GetNextEventTime() {
	if (eventQ.empty()) {
		return -1;
	}
	traceDES("Next Event: %d,Time Stamp: %d\n", eventQ.front()->eid, eventQ.front()->time_stamp);
	return eventQ.front()->time_stamp;	
}

Event* DES::GetPendingEventByPID(int pid) {
	auto iter = eventQ.begin();
	while (iter != eventQ.end()) {
    	if ((*iter)->process->pid == pid) {
        	traceDES("Found pending event: %d\n", (*iter)->eid);
			return *iter;
        }   
        iter++;
	} 
	
	traceDES("No pending event for process %d.\n", pid);
	return nullptr;
}
/*
 * Base Scheduler
 */

Scheduler::Scheduler(string type) : sched_type(type) {
	traceSched("Initializing Base Scheduler for %s with quantum %d\n", &sched_type[0], quantum);
}


Scheduler::Scheduler(string type, int quantum) :  sched_type(type), quantum(quantum) {
	traceSched("Initializing Base Scheduler for %s with quantum %d\n", &sched_type[0], quantum);
}

/*
 * FCFS Scheduler / Round Robin (FCFS + preemption) Scheduler
 */

FCFS_scheduler::FCFS_scheduler() : Scheduler("FCFS") {
	traceSched("Initializing FCFS scheduler\n");
}

FCFS_scheduler::FCFS_scheduler(string type, int quantum) : Scheduler(type, quantum) {
	traceSched("Initializing FCFS scheduler\n");
}

void FCFS_scheduler::AddProcess(Process *p) {
	readyQ.push_back(p);	
	// this->ShowReadyQueue();
}

Process* FCFS_scheduler::GetNextProcess() {
	if (readyQ.empty()) {
		return nullptr;
	}
	Process *proc = readyQ.front();
	readyQ.pop_front();
	return proc;
}

void FCFS_scheduler::ShowReadyQueue() {
	if (TRACE_SCHED > 2) {
		traceSched("Show ReadyQ...\n");
		for (auto &p: readyQ) {
			traceSched("Process %d, Entry Time: %d\n", p->pid, p->state_time_stamp); 
		}
	}
	
	cout << "SCHED (" << readyQ.size() << "):";
	for (auto &p: readyQ) {
		cout << "  " << p->pid << ":" << p->state_time_stamp; 
	}	
	cout << endl;
}

/*
 * LCFS Scheduler
 */

LCFS_scheduler::LCFS_scheduler() : Scheduler("LCFS") {
	traceSched("Initializing LCFS scheduler\n");
}

void LCFS_scheduler::AddProcess(Process *p) {
	readyQ.push_back(p);	
	// this->ShowReadyQueue();
}

Process* LCFS_scheduler::GetNextProcess() {
	if (readyQ.empty()) {
		return nullptr;
	}
	Process *proc = readyQ.back();
	readyQ.pop_back();
	return proc;
}

void LCFS_scheduler::ShowReadyQueue() {
	if (TRACE_SCHED > 2) {
		traceSched("Show ReadyQ...\n");
		for (auto &p: readyQ) {
			traceSched("Process %d, Entry Time: %d\n", p->pid, p->state_time_stamp); 
		}
	}
	
	cout << "SCHED (" << readyQ.size() << "):";
	for (auto &p: readyQ) {
		cout << "  " << p->pid << ":" << p->state_time_stamp; 
	}	
	cout << endl;
}

/*
 * SRTF Scheduler
 */

SRTF_scheduler::SRTF_scheduler() : Scheduler("SRTF") {
	traceSched("Initializing %s scheduler\n", &sched_type[0]);
}

void SRTF_scheduler::AddProcess(Process *p) {
	traceSched("Add Process %d, Remaining Execution Time %d\n", p->pid, p->rem_cpu_time);
	// sort by the remaining cpu time
	
	auto iter = readyQ.begin();
	while (iter != readyQ.end()) {
		if (p->rem_cpu_time < (*iter)->rem_cpu_time) {
			break;
		}
		iter++; 
	}
	
	readyQ.insert(iter, p);	
	// this->ShowReadyQueue();
}

Process* SRTF_scheduler::GetNextProcess() {
	if (readyQ.empty()) {
		return nullptr;
	}
	
	Process *proc = readyQ.front();
	readyQ.pop_front();
	return proc;
}

void SRTF_scheduler::ShowReadyQueue() {
	if (TRACE_SCHED > 2) {
		traceSched("Show ReadyQ...\n");
		for (auto &p: readyQ) {
			traceSched("Process %d, Entry Time: %d, Remain CPU Time: %d\n", 
				p->pid, 
				p->state_time_stamp,
				p->rem_cpu_time); 
		}
	}
	
	cout << "SCHED (" << readyQ.size() << "):";
	for (auto &p: readyQ) {
		cout << "  " << p->pid << ":" << p->state_time_stamp; 
	}	
	cout << endl;
}


/*
 * PRIO Scheduler
 */

PRIO_scheduler::PRIO_scheduler(int quantum, int maxprio) : 
	Scheduler("PRIO", quantum), 
	maxprio(maxprio),
	priority_preempt(false) {
	traceSched("Initializing %s scheduler with maxprio %d, prio preempt %d\n", &sched_type[0], maxprio, priority_preempt);
	activeQ.resize(maxprio);
	expiredQ.resize(maxprio);
	traceSched("ActiveQ Size: %d, ExpiredQ Size: %d\n", activeQ.size(), expiredQ.size());	
}

PRIO_scheduler::PRIO_scheduler(int quantum, int maxprio, bool priority_preempt) : 
	Scheduler("PREPRIO", quantum), 
	maxprio(maxprio),
	priority_preempt(priority_preempt) {
	traceSched("Initializing %s scheduler with maxprio %d, prio preempt %d\n", &sched_type[0], maxprio, priority_preempt);
	activeQ.resize(maxprio);
	expiredQ.resize(maxprio);
	traceSched("ActiveQ Size: %d, ExpiredQ Size: %d\n", activeQ.size(), expiredQ.size());	
}


void PRIO_scheduler::TraceQueue(vector<deque<Process*>> &ml_queue) {
	int priority = 4;
	for (auto q = ml_queue.rbegin(); q != ml_queue.rend(); q++) {
		auto p = q->begin();
		while (p != q->end()) {
			traceSched("Prority %d: pid %d\n", priority, (*p)->pid);
			p++;	
		}
		priority--;
	}

}


void PRIO_scheduler::AddProcess(Process *p) {
	traceSched("Add Process %d, Remaining Execution Time %d, Dynamic Prio %d\n", p->pid, p->rem_cpu_time, p->dynamic_prio);

	if (p->dynamic_prio < 0) {
		// reset and enter p into expireQ
		p->dynamic_prio = p->static_prio - 1;
	    expiredQ[p->dynamic_prio].push_back(p);
		traceSched("Reset dynamic prio to: %d, Add p to expiredQ\n", p->dynamic_prio);
	} else {
		traceSched("Add p to activeQ\n");
		activeQ[p->dynamic_prio].push_back(p);
	}
	TraceQueue(activeQ);
	TraceQueue(expiredQ);
	
}

bool PRIO_scheduler::IsEmptyQueue_(vector<deque<Process*>> &Q) {
	int count = 0;
	for (auto q: Q){
		count += q.size();
	}
	
	traceSched("Process Count: %d\n", count);
	if (count == 0) {
		return true;
	}
	return false;
}


Process* PRIO_scheduler::GetNextProcess() {
	bool switched = false;
	for (int i = 0; i < 2; i++) {
		if (!this->IsEmptyQueue_(activeQ)) {
			for (auto q = activeQ.rbegin(); q != activeQ.rend(); q++) {
				if (!q->empty()) {
					Process *proc = q->front();
					q->pop_front();
					return proc;				
				}
			}
		} else if (!switched) {
			activeQ.swap(expiredQ);
			if (SHOW_SCHED_READY_QUEUE) {
				cout << "switched queues" << endl;
			}
			switched = true;
		}
	}
	return nullptr;
}

void PRIO_scheduler::PrintMLQueue(vector<deque<Process*>> &ml_queue) {
	cout << "{ ";
	for (auto q = ml_queue.rbegin(); q != ml_queue.rend(); q++) {
		cout << "[";
		
		auto p = q->begin();
		if (p != q->end()) {
			cout << (*p)->pid;
			p++;
		}
		while (p != q->end()) {
			cout << "," << (*p)->pid;
			p++;	
		}
		cout << "]";
	}
	cout << "} : ";

}

void PRIO_scheduler::ShowReadyQueue() {

	PrintMLQueue(activeQ);
	PrintMLQueue(expiredQ);
	cout << endl;	
}

bool PRIO_scheduler::TestPreempt(Process *proc, int current_time, Process *curr_running_proc) {
	traceSched("TestPreempt\n");
	if (!priority_preempt) {
		return false;
	}

	if (!curr_running_proc) {//|| !proc->pending_evt) {
		traceSched("No current running process\n");
		return false;
	}
	
	if (curr_running_proc->pending_evt) {
		traceSched("Pending Event: %d %d %s\n",
			curr_running_proc->pending_evt->time_stamp,
			curr_running_proc->pid,
			&TRANSITION_TO_STR[curr_running_proc->pending_evt->transition][0]);
	} else {
		traceSched("No pending event for process %d\n", proc->pid);
		return false;
	}
	
	bool cond1 = proc->dynamic_prio > curr_running_proc->dynamic_prio;
	bool cond2 = curr_running_proc->pending_evt->time_stamp > current_time; 
	curr_running_proc->time_to_pending_evt = curr_running_proc->pending_evt->time_stamp - current_time;
	if (SHOW_PRIO_PREEMPT) {
		cout << "    --> Preempt Cond1=" << cond1 << " Cond2=" << cond2 << " (" << curr_running_proc->time_to_pending_evt  << ") --> ";
		if (cond1 && cond2) {
			cout << "YES" << endl;
		} else {
			cout << "NO" << endl;
		}
	}
	return cond1 && cond2;
}


