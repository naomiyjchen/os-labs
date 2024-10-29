#include <vector>
#include <cstdio> // for printf()
#include <iostream>
#include <deque>
#include <fstream>

// TRACING
#ifndef TRACE_MMU
#define TRACE_MMU 0
#define traceM(fmt...)  do { if (TRACE_MMU) {\
 			printf("[%s: %s: %d] ", __FILE__, __PRETTY_FUNCTION__, __LINE__), \
	 		printf(fmt); fflush(stdout); }  } while(0)
#endif

using namespace std;

#ifndef mmu_h
#define mmu_h

// CONSTANTS
extern unsigned int NUM_FRAMES;
extern unsigned int MAX_VPAGES;
extern unsigned long CURRENT_PID; 
extern unsigned long INSTR_COUNT;

struct Process;
struct Frame;
class Pager;
extern vector<Process> process_pool;
extern vector<Frame> frame_table;
extern std::deque<unsigned int> free_frames;
extern Pager *THE_PAGER;

extern unsigned long long COST; 

enum COST_TABLE {
	READ_WRITE         = 1,
    CTX_SWITCHES_COST  = 130,
    PROC_EXITS_COST    = 1230,
    MAPS               = 350,
    UNMAPS             = 410,
    INS                = 3200,
    OUTS               = 2750,
    FINS               = 2350,
    FOUTS              = 2800,
    ZEROS              = 150,
    SEGV               = 440,
    SEGPROT            = 410
};

/*
 * Segment / Virtual Memory Area (VMA)
 */
struct VMA {
	// total 64 virtual pages
	unsigned int start_vpage:6;
	unsigned int end_vpage:6;
	unsigned int write_protected:1;
	unsigned int file_mapped:1;
};

/*
 * Page Table Entry
 */

// can only be a total of 32 bits
struct PTE {
    unsigned int present:1;
    unsigned int referenced:1;
    unsigned int modified:1;
	unsigned int write_protected:1;
    unsigned int paged_out:1;
    unsigned int frame:7; // Assuming that the maximum number of frame is 128
    // for your own usage
    unsigned int file_mapped:1; // depends on the mapped physical frame
    unsigned int vma_checked:1;
    unsigned int vma_valid:1;
    unsigned int zeros:17;

	PTE() : present(0),
			referenced(0),
			modified(0),
			write_protected(0),
			paged_out(0),
			frame(0),
			vma_checked(0),
			vma_valid(0),
			file_mapped(0),
			zeros(0)
			{}
};

struct Process {
	vector<VMA> vmas;	
	vector<PTE> page_table;
    
	unsigned long unmaps    = 0;
    unsigned long maps      = 0;
    unsigned long ins       = 0;
    unsigned long fins      = 0;
    unsigned long outs      = 0;
    unsigned long fouts     = 0;
    unsigned long zeros     = 0;
    unsigned long segv      = 0;
    unsigned long segprot   = 0;
	
	Process() { 
		traceM("Initializing a Process with PTE entry size: %d (bytes)\n", sizeof(page_table[0]));
		for (int i = 0; i < MAX_VPAGES; i++) {
			page_table.push_back(PTE());
		} 
	}
};

/*
 * Physical Frame,
 * where you maintain reverse mapping to the process
 * and the vpage that maps a particular frame
 */
struct Frame {
	unsigned int pid;
	unsigned int vpage:6;
	unsigned int mapped:1;	 
	unsigned int age:32; // for Aging Pager
	unsigned int last_used; // for Working Set Pager
};


/*
 * frame_table output operator
 */
ostream &operator<<(ostream &os, const vector<Frame> &frame_table);

/*
 * page_table output operator
 */
ostream &operator<<(ostream &os, const vector<PTE> &page_table);

/*
 * process output operator
 */
ostream& operator << (ostream& os, const Process& process);

/*
 * Pager Abstract Class
 */ 

class Pager {
protected:
    const bool OPTION_a;
public:
	Pager(bool OPTION_a) : OPTION_a(OPTION_a) { 
		traceM( "Initializing Pager with OPTION_a: %d\n", this->OPTION_a); }
	virtual unsigned int SelectVictimFrame() = 0; // returns a frame number
	virtual void ResetActivePageField(unsigned int f_idx) {}
	virtual ~Pager() = default;
};

/*
 * FIFO Pager
 */

class FifoPager : public Pager {
protected:
	unsigned int hand;
public:
	FifoPager(bool OPTION_a) : Pager(OPTION_a), hand(0) { 
		traceM("Initializing FifoPager with hand %d\n", hand); 
	}
	unsigned int SelectVictimFrame();
};

inline Process& current_process() { return process_pool[CURRENT_PID]; }

/*
 * Reverse Mapping Frame -> PTE
 */
 
inline PTE& FrameToPTE(unsigned int i);

/*
 * Clock Pager
 */

class ClockPager : public FifoPager {
public:
	ClockPager(bool OPTION_a) : FifoPager(OPTION_a) {}
	unsigned int SelectVictimFrame();
};

/*
 * ESC Pager
 */

class ESCPager : public Pager {
private:
	static const unsigned int reset_cycle = 48;
	unsigned int hand = 0;
	unsigned long last_R_reset = 0;
public:
	ESCPager(bool OPTION_a) :  Pager(OPTION_a) {}
	unsigned int SelectVictimFrame(); 

};

/*
 * Random Number Generator
 */

class RandGenerator {
public:
	int total = 0;
	int ofs = -1;
	vector<int> randvals;
	RandGenerator(string rfile_name);
	unsigned int myrandom(unsigned int upper_bound);
};

/*
 * Random Pager
 */
class RandomPager : public Pager {
private:
	RandGenerator rand_generator;
public:
	RandomPager(bool OPTION_a, string rfile_name) :  
		Pager(OPTION_a), rand_generator(rfile_name) {}
	unsigned int SelectVictimFrame() { 
		return rand_generator.myrandom(NUM_FRAMES);
	} 

};

/*
 * Aging Pager
 */
class AgingPager : public Pager {
private:
	unsigned int hand = 0;
public:
	AgingPager(bool OPTION_a) : Pager(OPTION_a) {}
	unsigned int SelectVictimFrame();
	void ResetActivePageField(unsigned int f_idx) {
		frame_table[f_idx].age = 0;
	}

};

/*
 * Working Set Pager
 *
 * The working set of a process is the set of pages it has referenced
 * during the past TAU seconds of virtual time
 */

class WorkingSetPager : public Pager {
private:
	unsigned int hand = 0;
	static const unsigned int TAU = 49;
public:
	WorkingSetPager(bool OPTION_a) : Pager(OPTION_a) {}
	void ResetActivePageField(unsigned int f_idx) {
		frame_table[f_idx].age = 0;
		frame_table[f_idx].last_used = INSTR_COUNT;
	}
	unsigned int SelectVictimFrame();
};

/*
 * Page Fault Handler
 */
class PageFaultHandler {
private:
	int AllocateFrameFromFreeList();
public:
	PageFaultHandler() { traceM("Initializing PageFaultHandler\n"); }
	bool HandlePageFaultException(PTE &pte, unsigned int vpage);
	unsigned int GetFrame();
};


#endif
