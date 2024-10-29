#include <iostream>
#include <unistd.h> // for getopt
#include <cstdlib> // for exit(1)
#include <cstdio> // for printf()
#include <fstream>
#include <sstream> // for istringstream
#include <vector>
#include <iomanip>
#include "iosched.h"

// TRACE
#ifndef DO_TRACE
#define DO_TRACE 0
#define trace(fmt...)  do { if (DO_TRACE) {\
  				printf("[%s: %s: %d] ", __FILE__, __PRETTY_FUNCTION__, __LINE__), \
		 		printf(fmt); fflush(stdout); }  } while(0)
#endif

using namespace std;

bool OPTION_v = false;
bool OPTION_q = false;
bool OPTION_f = false;

IOScheduler *IO_SCHEDULER = nullptr;
int CURRENT_TIME = 0;
int TOTAL_MOVEMENT = 0;
double IO_BUSY = 0;
void ParseOptArgs(int &argc, char *argv[], char &algo) {
	trace("Reading arguments with argc=%d..\n", argc);
	char c;
	char *option = nullptr;
	while ((c = getopt(argc, argv, "vfqs:")) != -1) {
		switch(c) {
			case 's':
				trace("s optarg: %s\n", optarg);
				algo = *optarg;
				break;
			case 'v':
				OPTION_v = true;
				break;
			case 'q':
				OPTION_q = true;
				break;
			case 'f':
				OPTION_f = true;
				break;
			case '?':
				trace("?: %c \n", optopt);	
				exit(1);
			default:
				exit(1);
		}
	}
}

string ParseNonOptArgs(char *argv[]) {
	trace("Reading non-option arguments with optind: %d\n", optind);
	if (argv[optind]) {
		return argv[optind];
	} else {
		cerr << "Not a valid inputfile <(null)>" << endl;
		exit(1);
	}	
}

void LoadIORequests(string infile_name, vector<IO_Operation> &io_requests) {
	trace("Reading IO requests from file\n");
	ifstream infile(infile_name);
	if (!infile) {
		cerr << "Not a valid inputfile <" << infile_name << ">" << endl;
		exit(1);
	}

	string line;
	int arr_time, req_track;
	int req_count = 0;
	while (getline(infile, line)) {
		if (line[0] == '#') {
			trace("Encounter a comment: %s\n", &line[0]);
			continue;
		}
		istringstream io_request(line);
		io_request >> arr_time >> req_track;
		io_requests.push_back(IO_Operation(req_count++, arr_time, req_track));
	}			
}



void InitializeIOScheduler(char sched_algo) {
	switch(sched_algo) {
		case 'N':
			IO_SCHEDULER = new FIFOScheduler(OPTION_q);
			break;
		case 'S':
			IO_SCHEDULER = new SSTFScheduler(OPTION_q);
			break;
		case 'L':
			IO_SCHEDULER = new LOOKScheduler(OPTION_q);
			break;
		case 'C':
			IO_SCHEDULER = new CLOOKScheduler(OPTION_q);
			break;
		case 'F':
			IO_SCHEDULER = new FLOOKScheduler(OPTION_q);
			break;
		default:
			cerr << "Unknown Scheduler spec: -s {NSLCF}" << endl;
			exit(1);
	}
}

void Simulation(vector<IO_Operation> &io_requests) {
	
	IO_Operation* active_IO_req = nullptr;
	auto incoming_IO = io_requests.begin();
	trace("First incoming io %d at time %d\n", incoming_IO->op_id, incoming_IO->arr_time);
	
	if (incoming_IO != io_requests.end()) {
		if (OPTION_v) {
			cout << "TRACE" << endl;
		}

		while (true) {
			trace("Current time:%d, Head: %d, Dir: %d\n", CURRENT_TIME, IO_SCHEDULER->head, IO_SCHEDULER->direction);	
			if (incoming_IO->arr_time == CURRENT_TIME) {
				if (OPTION_v) {
					cout << CURRENT_TIME << ":"
						 << right << setw(6) 
						 << incoming_IO->op_id << " add " << incoming_IO->req_track << endl;
				}
		
				IO_SCHEDULER->AddIORequest(&(*incoming_IO));
				incoming_IO++;
			}

			if (active_IO_req != NULL && active_IO_req->completed) {
				active_IO_req->end_time = CURRENT_TIME;
				if (OPTION_v) { 
	            	 cout << CURRENT_TIME << ":"
						  << right << setw(6)
						  << active_IO_req->op_id << " finish "
						  << active_IO_req->end_time - active_IO_req->arr_time << endl;                 
             	}
				active_IO_req = NULL;	
			} 

			if (active_IO_req == NULL) {
				if (IO_SCHEDULER->HasPendingRequests()) { 
					active_IO_req = IO_SCHEDULER->Strategy();
					if (active_IO_req == NULL) continue;
						
					if (OPTION_v) {
						cout << CURRENT_TIME << ":"
							 << right << setw(6)
							 << active_IO_req->op_id << " issue " << active_IO_req->req_track
							 << " " << IO_SCHEDULER->head << endl;
					}
					active_IO_req->start_time = CURRENT_TIME;
					
					if (active_IO_req->req_track == IO_SCHEDULER->head) {
						active_IO_req->completed = true;
						continue;
					}	
			} else if (incoming_IO == io_requests.end()) { // all IO from input file processed
					break;
				}
			}
			
			if (active_IO_req != NULL) {
				IO_SCHEDULER->Seek(active_IO_req);
				TOTAL_MOVEMENT++;
				IO_BUSY++; 
			}
			CURRENT_TIME++;
		}
	}
}

void PrintRequestsInfoAndStats(vector<IO_Operation> io_requests) {
	double total_waittime = 0;
	double total_turnaround = 0;
	int max_waittime = 0;
	for (auto req: io_requests) {
		printf("%5d: %5d %5d %5d\n", req.op_id, req.arr_time, req.start_time, req.end_time);
		int waittime = req.start_time - req.arr_time; 
		total_waittime += waittime; 
		max_waittime = (max_waittime < waittime ? waittime : max_waittime);
		total_turnaround += req.end_time - req.arr_time;
	}
	
	cout << "SUM: "	
		 << CURRENT_TIME << " "
		 << TOTAL_MOVEMENT << " "
		 << fixed << setprecision(4) << IO_BUSY / CURRENT_TIME << " "
		 << fixed << setprecision(2) << total_turnaround / io_requests.size() << " "
		 << fixed << setprecision(2) << total_waittime / io_requests.size() << " "
		 << max_waittime << endl;
}
int main(int argc, char *argv[]) {
	char sched_algo = 'N';
	ParseOptArgs(argc, argv, sched_algo);
	trace("Sched algorithm: %c\n", sched_algo);
	
	string infile_name = ParseNonOptArgs(argv);
	trace("Input file: %s\n", &infile_name[0]);
	
	vector<IO_Operation> io_requests;	
	LoadIORequests(infile_name, io_requests);

	InitializeIOScheduler(sched_algo);
	Simulation(io_requests);
	delete IO_SCHEDULER;
	
	PrintRequestsInfoAndStats(io_requests);	
	return 0;
} 
