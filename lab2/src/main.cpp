#include <iostream>
#include <string>
#include <vector>
#include <fstream>
#include <memory>
// for getopt
#include <unistd.h>
#include <stdio.h>
#include <iomanip>

#include "sched.h"

// TRACING
#ifndef DO_TRACE
#define DO_TRACE 0 //3
#define trace(fmt...)  do { if (DO_TRACE) {\
			printf("[%s: %s: %d] ", __FILE__, __PRETTY_FUNCTION__, __LINE__), \
			printf(fmt); fflush(stdout); }  } while(0)
#endif

using namespace std;

bool VERBOSE = false; // -v
bool SHOW_SCHED_READY_QUEUE = false; // -t
bool SHOW_EVENT_QUEUE = false; // -e
bool SHOW_PRIO_PREEMPT = false; // -p

int EVENT_COUNTER = 0;
int CURRENT_TIME = 0; 
bool CALL_SCHEDULER = false;
Process *CURRENT_RUNNING_PROCESS = nullptr;
int IO_USE = 0; // time at least one process is performing IO
int LAST_IO_END_TIME = 0;

class RandGenerator {
public:
	int total = 0;
	int ofs = -1;
	vector<int> randvals;
	RandGenerator(string rfile_name) {
	// read in all the random numbers
		ifstream rfile(rfile_name);
		if (rfile) {
			rfile >> total;
			trace("Initializing RandGenerator with %d numbers\n", total);
			int num;
			while (rfile >> num) {
				randvals.push_back(num);
			}
		} else {
			cerr << "Not a valid random file <" << rfile_name << ">" << endl;
			exit(1);
		}
	}
	
	int myrandom(int upper_bound) {
		ofs++;
		if (ofs == total) {
			trace("Reseting offset to 0\n");
			ofs = 0;
		}
		return 1 + (randvals[ofs] % upper_bound);
	}
};

void TraceEventExecution(Process *proc, Event *evt, int time_in_prev_state, int cpu_burst = 0, int io_burst = 0) {
	// time stamp | PID | Time stayed in its prev state
	cout << CURRENT_TIME << " " << proc->pid << " " << time_in_prev_state << ": ";
	// Transition
	cout << PROCESS_STATE_TO_STR[evt->old_state] << " -> "
         << PROCESS_STATE_TO_STR[evt->new_state] << " ";
	
	switch (evt->transition) {
		case TRANS_TO_READY:
			cout << endl;
			break;
		case TRANS_TO_PREEMPT:
			// (rem) cb | rem | (dynamic) prio    
			cout << " cb=" << proc->rem_cpu_burst << " rem=" << proc->rem_cpu_time << " prio=" << proc->dynamic_prio << endl;
			break;

		case TRANS_TO_RUN:
			// cb | rem | (dynamic) prio	
			cout << " cb=" << cpu_burst << " rem=" << proc->rem_cpu_time << " prio=" << proc->dynamic_prio << endl;
			break;

		case TRANS_TO_BLOCK:
			if (proc->rem_cpu_time) {
				// ib | rem
				cout << " ib=" << io_burst << " rem=" << proc->rem_cpu_time << endl;
			} else {
				cout << "Done" << endl;
			}
	}

}

void AddEventToEventQ(DES &des, Event *evt) {
	// Before insertion
	if (SHOW_EVENT_QUEUE) {
		cout << "  AddEvent(" << evt->time_stamp << ":"
			 << evt->process->pid << ":" 
			 << TRANSITION_TO_STR[evt->transition] << "):";
	  des.ShowEventQ();
	} 
	
	des.PutEvent(evt);
	
	// After insertion
	if (SHOW_EVENT_QUEUE) {
		cout << " ==>";
		des.ShowEventQ();
		cout << endl;
	} 
}

void update_io_use(int io_burst) {
	int curr_end_time = CURRENT_TIME + io_burst;
	if (LAST_IO_END_TIME < curr_end_time) {
		IO_USE += curr_end_time - max(CURRENT_TIME, LAST_IO_END_TIME);
		LAST_IO_END_TIME = curr_end_time;
		trace("Update IO_USE: %d\n", IO_USE);
	}
}

void simulation(DES &des, Scheduler &sched, RandGenerator &rand) {
	trace("Simluation starts...\n");
	trace("Scheduler Type: %s\n", &sched.sched_type[0]);
	Event *evt;
	while (evt = des.GetEvent()) {
		trace("Get Event %d, time stamp: %d, pid: %d, old state: %s, new state: %s\n",
			   evt->eid,  
               evt->time_stamp, 
			   evt->process->pid,
			   &PROCESS_STATE_TO_STR[evt->old_state][0],
			   &PROCESS_STATE_TO_STR[evt->new_state][0]);
		Process *proc = evt->process; // this is the process the event works on
		CURRENT_TIME = evt->time_stamp;
		int transition = evt->transition;
		int old_state = evt->old_state;
		int new_state = evt->new_state;
		int time_in_prev_state = CURRENT_TIME - proc->state_time_stamp;
		proc->state_time_stamp = CURRENT_TIME;
		if (DO_TRACE > 3) {
			des.TraceEventQ();
		}
		
		int cpu_burst = 0;
		int io_burst = 0;
		switch(transition) {
		case TRANS_TO_READY:
			if (VERBOSE) {
				TraceEventExecution(proc, evt, time_in_prev_state);
			}			
			
			if (evt->old_state == STATE_BLOCKED) {		
				proc->dynamic_prio = proc->static_prio - 1;	
			}
			sched.AddProcess(proc);
			
			// check priority preemption
			if (CURRENT_RUNNING_PROCESS) {
				CURRENT_RUNNING_PROCESS->pending_evt = des.GetPendingEventByPID(CURRENT_RUNNING_PROCESS->pid);
			}
			
			if (sched.TestPreempt(proc, CURRENT_TIME, CURRENT_RUNNING_PROCESS)) {
			
				// remove future event for the current running process
				CURRENT_RUNNING_PROCESS->rem_cpu_time += CURRENT_RUNNING_PROCESS->time_to_pending_evt;
				CURRENT_RUNNING_PROCESS->rem_cpu_burst += CURRENT_RUNNING_PROCESS->time_to_pending_evt; 
				des.RemoveEvent(CURRENT_RUNNING_PROCESS->pending_evt->eid);				
				// add a new preemption event for the current time stamp	
				
				Event *evt = new Event(CURRENT_RUNNING_PROCESS,
							    	   CURRENT_TIME,
							    	   STATE_RUNNING,
							    	   STATE_READY,
							    	   TRANS_TO_PREEMPT);

				des.PutEvent(evt);
			}

			CALL_SCHEDULER = true;
			break;
		case TRANS_TO_PREEMPT:
			// must come from RUNNING
			// add to runqueue (no event is generated)
			if (VERBOSE) {
				TraceEventExecution(proc, evt, time_in_prev_state);
			}
			proc->dynamic_prio -= 1;
			sched.AddProcess(proc);
			
			CURRENT_RUNNING_PROCESS = nullptr;
			CALL_SCHEDULER = true;
			break;
		case TRANS_TO_RUN:
			// get cpu_burst
			if (CURRENT_RUNNING_PROCESS->rem_cpu_burst) {
				cpu_burst = CURRENT_RUNNING_PROCESS->rem_cpu_burst;
				trace("Remaining cpu_burst: %d\n", CURRENT_RUNNING_PROCESS->rem_cpu_burst);
			} else {
				cpu_burst = rand.myrandom(CURRENT_RUNNING_PROCESS->cpu_burst);			
				trace("Rand cpu_burst: %d\n", cpu_burst); 
			}
			
			// compare current cpu_burst with the remaning cpu execution time
			cpu_burst = min(cpu_burst, CURRENT_RUNNING_PROCESS->rem_cpu_time);
			
			if (VERBOSE) {
				TraceEventExecution(proc, evt, time_in_prev_state, cpu_burst);
			}
			
			//quantum preemption check
			Event *evt;
			trace("cpu_burst %d, scheduler quantum: %d\n", cpu_burst, sched.quantum);
			if (cpu_burst > sched.quantum) {		
				trace("Preempt current event!\n");
				// create an event for preemption
				proc->rem_cpu_time -= sched.quantum;
				proc->rem_cpu_burst = cpu_burst - sched.quantum;
				trace("Remaining cpu_burst: %d \n", proc->rem_cpu_burst);
				int end_time = CURRENT_TIME + sched.quantum;
				evt = new Event(proc,
								end_time,
								STATE_RUNNING,
								STATE_READY,
								TRANS_TO_PREEMPT);
			} else {
				// create an event for blocking
				proc->rem_cpu_time -= cpu_burst;
				proc->rem_cpu_burst = 0; // use up all the remaining cpu burst
				int end_time = CURRENT_TIME + cpu_burst;
				evt = new Event(proc,
							    end_time,
							    STATE_RUNNING,
							    STATE_BLOCKED,
							    TRANS_TO_BLOCK);
			}  
			AddEventToEventQ(des, evt);			
			break;

		case TRANS_TO_BLOCK:

			// generate io_busrt
			if (proc->rem_cpu_time) {
				io_burst = rand.myrandom(proc->io_burst);
				trace("Rand io_burst: %d\n", io_burst);
			}			
			proc->io_time += io_burst;
			update_io_use(io_burst);
			
			if (VERBOSE) {
				TraceEventExecution(proc, evt, time_in_prev_state, cpu_burst, io_burst);
			}	

			if (proc->rem_cpu_time) {
			// create an event for when the process becomes READY again
				int end_time = CURRENT_TIME + io_burst;
				Event *evt = new Event(proc,
									   end_time,
									   STATE_BLOCKED,
						               STATE_READY,
									   TRANS_TO_READY);
				AddEventToEventQ(des, evt);			
			} else {
				// process is done
				trace("Process is done. Mark finish time for the process.\n");
				proc->finish_time = CURRENT_TIME;
			}	
			CURRENT_RUNNING_PROCESS = nullptr;
			CALL_SCHEDULER = true;
			break;
		}
		delete evt;
		evt = nullptr;
	
		if (CALL_SCHEDULER) {
			if (des.GetNextEventTime() == CURRENT_TIME) {
				continue; // process next event from Event queue
			}						
			CALL_SCHEDULER = false;
			if (CURRENT_RUNNING_PROCESS == nullptr) {
				if (SHOW_SCHED_READY_QUEUE) {
                    sched.ShowReadyQueue();
                }
				CURRENT_RUNNING_PROCESS = sched.GetNextProcess();
				if (CURRENT_RUNNING_PROCESS == nullptr) {
					continue;
				}
				trace("Process %d: CPU Waiting Time (time in ready state): %d\n", CURRENT_RUNNING_PROCESS->pid, CURRENT_TIME - CURRENT_RUNNING_PROCESS->state_time_stamp);
				CURRENT_RUNNING_PROCESS->wait_time += CURRENT_TIME - CURRENT_RUNNING_PROCESS->state_time_stamp;
				trace("Process %d: Total CPU Waiting Time: %d\n", CURRENT_RUNNING_PROCESS->pid, CURRENT_RUNNING_PROCESS->wait_time);
				// create event tom make this process runnable for same time
				Event *evt = new Event(CURRENT_RUNNING_PROCESS,
                                     CURRENT_TIME,
                                     STATE_READY,
                                     STATE_RUNNING,
                                     TRANS_TO_RUN);
               	AddEventToEventQ(des, evt);
			}
		}
	}
}

void statistics(Scheduler *sched, vector<Process> &processes) { 
	int last_FT = 0; // Finish time of the last event
	double cpu_util, io_util, avg_TT, avg_cpu_wait, throughput;
	double count = processes.size();
	
	cout << sched->sched_type;
	if (sched->sched_type == "RR" || sched->sched_type == "PRIO" || sched->sched_type == "PREPRIO") {
		cout << " " << sched->quantum;
	}
	cout << endl;

	for (auto proc: processes) { 
		cout << setw(4) << setfill('0') << proc.pid << ": "
			 << setw(4) << setfill(' ') << proc.arrival_time << " "
			 << setw(4) << proc.total_cpu_time << " "
			 << setw(4) << proc.cpu_burst << " "
			 << setw(4) << proc.io_burst << " "
			 << setw(1) << proc.static_prio << " | ";

		cout << setw(5) << proc.finish_time << " "
			 << setw(5) << proc.finish_time - proc.arrival_time << " "
			 << setw(5) << proc.io_time << " "
			 << setw(5) << proc.wait_time << endl;
		last_FT = max(last_FT, proc.finish_time);
		cpu_util += proc.total_cpu_time;
	    avg_TT += proc.finish_time - proc.arrival_time;
		avg_cpu_wait += proc.wait_time;
	}    
	
	cpu_util = cpu_util / last_FT * 100;
	io_util += (double)IO_USE / last_FT * 100;
	trace("io_use: %d, io_util: %f\n", IO_USE, io_util);
	avg_TT /= count;
	avg_cpu_wait /= count;
	throughput = 100 *  count / last_FT;
	cout << "SUM: " 
		 << last_FT << " "
		 << fixed << setprecision(2) << cpu_util << " "
		 << fixed << setprecision(2) << io_util << " "
	     // Average turnaround time among processes
		 << fixed << setprecision(2) << avg_TT  << " "
		 << fixed << setprecision(2) << avg_cpu_wait << " "
		 << fixed << setprecision(3) << throughput << endl;
}

int main(int argc, char *argv[]){
	// parse option arguments
	char c;
	char sched_type[] = {'F'};
	int quantum = 0;
	int maxprio = 4; // default
	opterr = 0;
	while ((c = getopt(argc, argv, "vteps:")) != -1) {
		switch(c) {
			case 'v':
				VERBOSE = true;
				trace("v, Verbose: %d\n", VERBOSE);
				break;
			case 't':
				SHOW_SCHED_READY_QUEUE = true;
				trace("t, Trace event executation: %d\n", SHOW_SCHED_READY_QUEUE);
				break;
			case 'e':
				SHOW_EVENT_QUEUE = true;
				trace("e, Show event queue before and after an event is inserted: %d\n", SHOW_EVENT_QUEUE);
				break;	
			case 'p':
                SHOW_PRIO_PREEMPT = true;
				trace("%s\n", "p: Show the E scheduler's decision  when an unblacked process attempts to preempt the running process");
				break;
			case 's':
				trace("Optarg: %s\n", optarg);
				
				sscanf(optarg, "%1s%d:%d\n", sched_type, &quantum, &maxprio);
				if ((sched_type[0] == 'R' || sched_type[0] == 'P' || sched_type[0] == 'E') && (quantum < 1)) {
					cout << "Invalid scheduler param <" << optarg << ">" << endl;
					return 1; 
				}
				if ((sched_type[0] == 'P' || sched_type[0] == 'E') && (maxprio < 1)) {
					cout << "Invalid scheduler param <" << optarg << ">" << endl;
                    return 1;
				}
				trace("Scheduler: %s\n", sched_type);	
				trace("quantum: %d, maxpprio:%d\n", quantum, maxprio);
				break;
			case '?':
				trace("%s: %c \n", "?", optopt);
				cerr << "invalid option -- \'" << char(optopt) << "\'\n";		
				return 1; 
		}
	}
	
	// parse non-option arguments	
	argv += optind;
	string infile_name;
	string rfile_name;
	if (argv[0] != NULL) {
		infile_name = argv[0];
	} else {
		cerr << "Not a valid inputfile <(null)>" << endl;
		return 1;
	}
	
	if (argv[1] != NULL) {
		rfile_name = argv[1];
	} else {
		cerr << "Not a valid random file <(null)>" << endl;
		return 1;
	}
	trace("Input file: %s, Rand File: %s\n", &infile_name[0], &rfile_name[0]);


	// Initializing scheduler
	Scheduler *sched;
	bool prio_preempt = false;
	switch (sched_type[0]) {
		case 'F':
			trace("%s\n", "Scheduler Type: FCFS"); 
			sched = new FCFS_scheduler();
			break;
		case 'L':
			trace("%s\n", "Initializing LCFS"); 
			sched = new LCFS_scheduler();
			break;
		case 'S':
			trace("%s\n", "Initializing SRTF"); 
			sched = new SRTF_scheduler();
			break;
		case 'R':
			
			trace("Initializing RR (Round Robin) with quantum %d\n", quantum); 
			sched = new FCFS_scheduler("RR", quantum);
			break;
		case 'P':
			trace("Initializing PRIO (Priority Scheduler) with quantum %d, maxprio %d\n", quantum, maxprio); 
			sched = new PRIO_scheduler(quantum, maxprio);
			break;
		case 'E':
			trace("%s\n", "Initializing PREPRIIO (Preemptive Priority Scheduler)"); 
			prio_preempt = true;
			sched = new PRIO_scheduler(quantum, maxprio, prio_preempt);
			break;
		default:
			cerr << "Unknown Scheduler spec: -v {FLSRPE}" << endl;
			return 1; 
	}	
   	
	// Read the input file and create Process objects
	vector<Process> processes;
	ifstream input(infile_name);
	RandGenerator rand(rfile_name);
	int pid = 0;
	int arrival_time = 0;
	int total_cpu_time = 0;
	int cpu_burst = 0;
	int io_burst = 0;
	int static_prio = 0;

	if (input) {
		trace("Reading Processes...\n");	
		while(input >> arrival_time >> total_cpu_time >> cpu_burst >> io_burst) {
			static_prio = rand.myrandom(maxprio);
			Process *proc = new Process(pid, arrival_time, total_cpu_time, cpu_burst, io_burst, static_prio);
			processes.push_back(*proc); 
			pid++;
			delete proc;
		}
	} else {
		cerr << "Not a valid inputfile <"<< infile_name << ">" << endl;
		return 1;
	}

	trace("Show Processes:\n");	
	for (auto i: processes) {
		trace("process %d, AT: %d, TC: %d, CB: %d, IO: %d\n",
				i.pid,
				i.arrival_time,
				i.total_cpu_time,
				i.cpu_burst,
				i.io_burst);
	}	

	// Initialize DES layer 
	DES des(processes);

	/*
	for (auto iter = processes.begin(); iter != processes.end(); iter++) {
		Event *evt = new Event(&*iter,
				  iter->arrival_time,
				  STATE_CREATED,
				  STATE_READY,
				  TRANS_TO_READY);			  
		des.PutEvent(evt);
	}*/
	
	if (SHOW_EVENT_QUEUE) {
		cout << "ShowEventQ:";
		for (auto &e: des.eventQ) {
          cout << "  " << e->time_stamp << ":" << e->process->pid;
      	}
		cout << endl; 
	}

	// testing des functions	
	/*
	des.TraceEventQ();	
		
	if (DO_TRACE > 3) {
		des.RemoveEvent(8);
		des.RemoveEvent(2);
		des.TraceEventQ();
		Event *evt = des.GetEvent();
		trace("Get Event %d\n", evt->eid);
		des.RemoveEvent(evt->eid);
		des.TraceEventQ();
	}
	*/

	simulation(des, *sched, rand);
	statistics(sched, processes);

	delete sched;	
	return 0;
}
