#include <cstdio> // for printf()
#include <list>
#include <sstream> // for ostringstream
#include <iostream>
#include <algorithm> // for find()
// TRACING
#ifndef TRACE_IO
#define TRACE_IO 0
#define traceIO(fmt...)  do { if (TRACE_IO) {\
  				printf("[%s: %s: %d] ", __FILE__, __PRETTY_FUNCTION__, __LINE__), \
    			printf(fmt); fflush(stdout); }  } while(0)
#endif

using namespace std;

#ifndef iosched_h
#define iosched_h

extern int CURRENT_TIME;

struct IO_Operation {
	int op_id;
	int arr_time;
	int req_track;
	int distance = 0;
	int start_time = -1;
	int end_time = -1;
	bool completed = false;

	IO_Operation(int id, int at, int track): op_id(id), arr_time(at), req_track(track) {
		traceIO("Initialize ID: %d, AT: %d, RT: %d\n",
				op_id, arr_time, req_track); 
	}
};

/*
 * IO Scheduler Base Class
 */
class IOScheduler {
protected:
	bool OPTION_q = false;
	list<IO_Operation*> IO_queue;
	
	
public:
	int head = 0;
	int direction = 0;
	IOScheduler(bool OPTION_q) : OPTION_q(OPTION_q) { 
		traceIO("Initialize base iosched, OPTION_q: %d\n", this->OPTION_q);
	}
	IOScheduler(bool OPTION_q, int dir) : OPTION_q(OPTION_q), direction(dir) { 
		traceIO("Initialize base iosched, OPTION_q: %d, dir: %d\n",
				this->OPTION_q,
				this->direction);
	}
	virtual void AddIORequest(IO_Operation *op) { IO_queue.push_back(op); }
	virtual bool HasPendingRequests() { return (!IO_queue.empty()); }
	virtual IO_Operation* Strategy() = 0;
	
	virtual	void ComputeDistance() {
		for (auto op: IO_queue) {
			op->distance = op->req_track - head; 
		} 
	}
	
	virtual string ShowIOQueue() {
		ostringstream queue;
		if (!IO_queue.empty()) {
			for (auto op: IO_queue) {
				queue << op->op_id << ":" << abs(op->distance) << " ";
			}
		}
		return queue.str();
	}
	
	void Seek(IO_Operation *op) { 
		head += direction; 
		if (head == op->req_track) {
			op->completed = true;
		}
	}

	virtual ~IOScheduler() = default;
};

/*
 * FIFO IO Scheduler
 */
class FIFOScheduler: public IOScheduler {
public:
	FIFOScheduler(bool OPTION_q) : IOScheduler(OPTION_q) {
		traceIO("Initialize FIFO iosched\n");
	}
	
	IO_Operation* Strategy() {
		if (IO_queue.empty()) {
			return nullptr;
		}
		
		ComputeDistance();
		if (OPTION_q) {
        	cout << "   Get: (" << ShowIOQueue() << ") --> ";
		}

		IO_Operation *op = IO_queue.front();
		IO_queue.pop_front();
	
		if (OPTION_q) {
			cout << op->op_id << endl;
		}
			
		if (op->distance > 0) {
			direction = 1;
		} else if (op->distance < 0) {
			direction = -1;
		} else {
			direction = 0;
		}
		return op;
	}
};

/*
 * SSTF Scheduler
 */
class SSTFScheduler: public IOScheduler {
public:
	SSTFScheduler(bool OPTION_q) : IOScheduler(OPTION_q) {
		traceIO("Initialize SSTF iosched\n");
	}

	IO_Operation* Strategy() {
		if (IO_queue.empty()) {
			return nullptr;
		}
		ComputeDistance();
		if (OPTION_q) {
        	cout << "   Get: (" << ShowIOQueue() << ") --> ";
		}
		
		// find the shortest seek time
		IO_Operation *shortest_seek = IO_queue.front();
		auto remove = IO_queue.begin();
		for (auto iter = IO_queue.begin(); iter != IO_queue.end(); iter++){
			if (abs(shortest_seek->distance) > abs((*iter)->distance)) {
				shortest_seek = *iter;
				remove = iter;
			}
		} 
		IO_queue.erase(remove);		
		
		if (OPTION_q) {
			cout << shortest_seek->op_id << endl;
		}
			
		if (shortest_seek->req_track > head) {
			direction = 1;
		} else if (shortest_seek->req_track < head) {
			direction = -1;
		} else {
			//edge case: op->req_track == head	
			//shortest_seek->completed = true;
			direction = 0;
		}
		return shortest_seek;
	}
};

/*
 * LOOK Scheduler
 */
class LOOKScheduler: public IOScheduler {
public:
	LOOKScheduler(bool OPTION_q) : IOScheduler(OPTION_q, 1) {
		traceIO("Initialize LOOK iosched\n");
	}
	
	string ShowIOQueue() {
		ostringstream queue;
        if (!IO_queue.empty()) {
			for (auto op: IO_queue) {		
				if (op->distance * this->direction >= 0) {
					queue << op->op_id << ":" << abs(op->distance) << " ";				
				}
			}
		}

		return queue.str();
	}

	IO_Operation* Strategy() {
		if (IO_queue.empty()) {
			return nullptr;
		}
		ComputeDistance();
		if (OPTION_q) {
        	cout << "   Get: (" << ShowIOQueue() << ") --> ";
		}
		
		if (ShowIOQueue() == "") {
			this->direction *= -1;
			if (OPTION_q) cout << "change direction to " << this->direction << endl;
			return nullptr;
		}

		// find the first request along the same direction
		auto temp = IO_queue.begin();
		while ( (temp != IO_queue.end())  
				&& ((*temp)->distance * this->direction < 0) ) {
				temp++; // it's guaranteed that we'll find one
		}
		 
		// find the closest request along the same direction
		for (auto iter = IO_queue.begin(); iter != IO_queue.end(); iter++){
			traceIO("%d:%d\n", (*iter)->op_id, (*iter)->distance);
			if (((*iter)->distance * this->direction >= 0) 
				&& (abs((*temp)->distance) > abs((*iter)->distance))) {
				temp = iter;
			}
		} 

		IO_Operation *closest = *temp;
		IO_queue.erase(temp);		
		
		if (OPTION_q) {
			cout << closest->op_id << " dir=" << this->direction << endl;
		}
	
		return closest;	
	}
};

/*
 * CLOOK Scheduler
 */
class CLOOKScheduler: public IOScheduler {
public:
	CLOOKScheduler(bool OPTION_q) : IOScheduler(OPTION_q, 1) {
    	traceIO("Initialize CLOOK iosched\n");
	}

	string ShowIOQueue() {
		ostringstream queue;
        if (!IO_queue.empty()) {
			for (auto op: IO_queue) {		
				queue << op->op_id << ":" << op->distance << " ";				
			}
		}

		return queue.str();
	}

	IO_Operation* Strategy() {
		if (IO_queue.empty()) {
			return nullptr;
		}
		ComputeDistance();
		if (OPTION_q) {
        	cout << "   Get: (" << ShowIOQueue() << ") --> ";
		}
	
		// Check if there are pending requests in upward direction
		int upward_pending = false;		
		
		for (auto op: IO_queue) {
			if (op->distance >= 0) {
				upward_pending = true;
				break;
			}
		}
		
		if (!upward_pending) {
			traceIO("No higher pending io requests\n");
			direction = -1;
			// find the farest track at the opposite direction
			auto temp = IO_queue.begin();
			for (auto iter = IO_queue.begin(); iter != IO_queue.end(); iter++) {
				if ((*iter)->distance < (*temp)->distance) {
					temp = iter;
				}
			}

			IO_Operation *bottom = *temp;
			IO_queue.erase(temp);
			if (OPTION_q) cout << "go to bottom and pick " << bottom->op_id << endl;
			return bottom;
		} else {
			direction = 1;
		}

		// find the first request along the same direction
		auto temp = IO_queue.begin();
		while ( (temp != IO_queue.end())  
				&& ((*temp)->distance * this->direction < 0) ) {
				temp++; // it's guaranteed that we'll find one
		}
		 
		// find the closest request along the same direction
		for (auto iter = IO_queue.begin(); iter != IO_queue.end(); iter++){
			traceIO("Current Op-id %d: distance %d\n", (*iter)->op_id, (*iter)->distance);
			if (((*iter)->distance * this->direction >= 0) 
				&& (abs((*temp)->distance) > abs((*iter)->distance))) {
				temp = iter;
			}
		} 

		IO_Operation *closest = *temp;
		IO_queue.erase(temp);		
		
		if (OPTION_q) {
			cout << closest->op_id << endl;
		}
	
		return closest;	
	}
};

/*
 * FLOOK Scheduler 
 */
class FLOOKScheduler: public IOScheduler {
private:
	bool swap = false;
	vector<list<IO_Operation*>> queues;
	list<IO_Operation*>* add_queue = nullptr;
	list<IO_Operation*>* active_queue = nullptr;

public:
	FLOOKScheduler(bool OPTION_q) : IOScheduler(OPTION_q, 1) {
    	traceIO("Initialize FLOOK iosched with dir=%d\n", direction);
    	for (int i = 0; i < 2; i++) {
			list<IO_Operation*> queue;
			queues.push_back(queue);
		}

		add_queue = &queues[0];
	}

	int GetIndex(list<IO_Operation*> queue) {
		auto it = find(queues.begin(), queues.end(), queue);	
		return it - queues.begin(); 	
	}

	void AddIORequest(IO_Operation *op) { 
		(*add_queue).push_back(op); 
		if (OPTION_q) {
			cout << "   Q=" << GetIndex(*add_queue) 
				 << " ( " << ShowIOQueue() << ")" << endl; 
		}
		swap = false;	
	}
	bool HasPendingRequests() {
		traceIO("Time %d, Check pending requests\n", CURRENT_TIME);	
	
		// Set up active queue	
		if (active_queue == NULL) {

			if (!add_queue->empty()) {
				traceIO("Set up active queue\n");
				active_queue = add_queue;
				add_queue = &queues[1];
			} else {
				traceIO("Add queue empty\n");
				return false;
			}
		}
		
		if (active_queue->empty() and swap == false) {
			// swap active_queue and add_queue
			traceIO("Swap queues\n");
			swap = true;
			auto temp = active_queue;
			active_queue = add_queue;
			add_queue = temp;
		}		
			
			
		if (!active_queue->empty()) {
			swap = false;
			return true;
		} else {
			return false;
		}

	}

	void ComputeDistance() {
		for(auto &IO_queue: queues) {
			for (auto op: IO_queue) {
				op->distance = op->req_track - head; 
			}
		} 
	}	
	
	// show queue during add
	string ShowIOQueue() {
		ostringstream queue;
        if (!(*add_queue).empty()) {
			for (auto op: *add_queue) {		
				queue << op->op_id << ":" << op->req_track << " ";				
			}
		}
		return queue.str();
	}

	// show queue during get
	string ShowGetDetails() {
		ostringstream info;
		for(int i = 0; i < 2; i++) {
			info << " Q[" << i << "] = ( "; 
			for (auto op: queues[i]) {
				info << op->op_id << ":" << op->req_track << ":" << op->distance
					 << " "; 
			}
			info << ") ";
		} 
		
		info << endl;
		info << "	Get: (";

		for (auto op: *active_queue) {
			info << op->op_id << ":" << op->req_track << ":" << direction * op->distance << " ";
		}
		info << ") --> ";
		return info.str();		
	}
	
	IO_Operation* Strategy() {
		if (queues[0].empty() && queues[1].empty()) {
	     	return nullptr;
	    }
		ComputeDistance();
		
		if (OPTION_q) {
        	cout << "AQ=" << GetIndex(*active_queue) 
			     << " dir=" << direction
				 << " curtrack=" << head << ": "
				 << ShowGetDetails();
		}
	
		// Check if there are pending requests in the same direction
		bool same_dir_pending = false;		
		
		for (auto op: *active_queue) {
			if (op->distance * direction >= 0) {
				same_dir_pending = true;
				break;
			}
		}
		
		// if not, switch direction
		if (!same_dir_pending) {
			direction *= -1;
			if (OPTION_q) {
				cout << "change direction to " << direction << endl;
			
				cout << "	Get: (";
				for (auto op: *active_queue) {
					cout << op->op_id << ":" << op->req_track << ":" << direction * op->distance << " ";
				}
				cout << ") --> ";
			}
		}	

		// find the first request along the same direction
		auto temp = active_queue->begin();
		while ( (temp != active_queue->end())  
				&& ((*temp)->distance * this->direction < 0) ) {
				temp++; // it's guaranteed that we'll find one
		}
		 
		// find the closest request along the same direction
		for (auto iter = active_queue->begin(); iter != active_queue->end(); iter++){
			traceIO("%d:%d\n", (*iter)->op_id, (*iter)->distance);
			if (((*iter)->distance * this->direction >= 0) 
				&& (abs((*temp)->distance) > abs((*iter)->distance))) {
				temp = iter;
			}
		} 

		IO_Operation *closest = *temp;
		active_queue->erase(temp);		
		
		if (OPTION_q) {
			cout << closest->op_id << " dir=" << this->direction << endl;
		}
		
		

		return closest;		
	}
};
#endif
