#include <iostream>
#include <unistd.h> // for getopt
#include <cstdlib> // for exit(1)
#include <cstdio> // for sscanf, printf
#include <fstream>
#include <sstream> // for stringstream
#include <deque>
#include "mmu.h"

// TRACE
#ifndef DO_TRACE
#define DO_TRACE 0
#define trace(fmt...)  do { if (DO_TRACE) {\
 			printf("[%s: %s: %d] ", __FILE__, __PRETTY_FUNCTION__, __LINE__), \
 			printf(fmt); fflush(stdout); }  } while(0)
#endif

using namespace std;
/*
 * GLOBAL VARIABLES
 */

// Stats
unsigned long INSTR_COUNT = 0; 
unsigned long CTX_SWITCHES = 0;
unsigned long PROCESS_EXITS = 0;
extern unsigned long long COST;

// EXTERNAL
extern unsigned int NUM_FRAMES;
extern unsigned long CURRENT_PID;
extern vector<Process> process_pool;
extern vector<Frame> frame_table;
extern deque<unsigned int> free_frames;
extern Pager *THE_PAGER;

// Options
bool OPTION_O = false;
bool OPTION_P = false;
bool OPTION_F = false;
bool OPTION_S = false; // Print process statss "PROC[i]" and summary "TOTALCOST..."
bool OPTION_a = false;
bool OPTION_f = false;
bool OPTION_x = false;
bool OPTION_y = false;

void ParseOptArgs(int &argc, char *argv[], char &algo) {
	trace("Reading arguments...\n");
	char c;
	char *option = nullptr;
	while ((c = getopt(argc, argv, "f:a:o:xy")) != -1) {
		trace("argc: %d\n", argc);
		switch(c) {
			case 'f':
				trace("f optarg: %s\n", optarg);
				OPTION_f = true;
				NUM_FRAMES = atoi(optarg);
				trace("NUM_FRAMES: %d\n", NUM_FRAMES);
				if (NUM_FRAMES == 0) {
					cerr << "Really funny .. you need at least one frame\n";
					exit(1);
				}
				break;
			case 'a':
				trace("a optarg: %s\n", optarg);
				algo = *optarg;
				break;
			case 'o':
				trace("o optarg: %s\n", optarg);
				option = optarg;
				while (*option) {
					trace("option: %c\n", *option);
					
					switch (*option) {
						case 'O':
							OPTION_O = true;
							break;
						case 'P':
							OPTION_P = true;
							break;
						case 'F':
							OPTION_F = true;
							break;
						case 'S':
							OPTION_S = true;
							break;
						case 'a':
							OPTION_a = true;
							break;
						case 'f':
							OPTION_f = true;
							break;
						case 'x':
							OPTION_x = true;
							break;
						case 'y':
							OPTION_y = true;
							break;
						default:
							cerr << "Unknown output option: <" << *option << ">\n";
							exit(1);
					}
					
					++option;
				}
				break;
			case '?':
				trace("?: %c \n", "?", optopt);
				cerr << "illegal option\n"; 	
				exit(1);
		}
	}	
}

void ParseNonOptArgs(char algo, char *argv[], string &infile_name, string &rfile_name) {
	trace("Reading non-option arguments with optind: %d\n", optind);
	argv += optind;
	if (argv[0] != NULL) {
		infile_name = argv[0];
		trace("Input file: %s\n", &infile_name[0]);
	} else {
		cerr << "inputfile name not supplied" << endl;
		exit(1);
	}
	// random file is required for the Random Algorithm
	if (algo == 'r') {
		if (argv[1] != NULL) {
			rfile_name = argv[1];
			trace("Rand file: %s\n", &rfile_name[0]);
		} else {
			cerr << "randfile name not supplied" << endl;
			exit(1);
		}
	}
}

// Assume input files are all well formed
class InputReader {
private:
	ifstream infile_;
	stringstream line_;
public:
	InputReader(string infile_name): infile_(infile_name) {
		trace("Initializing the Input Reader and open the file...\n"); 
		if (!infile_) {
			cerr << "Cannot open inputfile <" << infile_name << ">" << endl;
	 		exit(1);
		} 	
	}

	bool GetNextValidLine() {
		string line;
		while (getline(infile_, line)) {
			if (line[0] == '#') {
				trace("Encounter a comment: %s\n", &line[0]);
				continue;
			}
			line_.clear();
			line_.str(line);
			// trace("Line: %s\n", &line_.str()[0]);
			return true;
		}
		return false;		
	}
	
	void CreateProcess() {
		if (!GetNextValidLine()) {
			trace("Number of Processes expected\n");
			exit(1);
		}

		int num_process = 0;
		line_ >> num_process;
		trace("Number of Processes: %d\n", num_process);
		int pid = 0;
		while (num_process) {
			// Create a process
			process_pool.push_back(Process());
			
			stringstream buffer;
			buffer << process_pool.back().page_table;
			trace("PT[%d]: %s\n", pid++, &buffer.str()[0]); 	
			
			// Create process' vmas
			if (!GetNextValidLine()) {
				trace("Number of VMAs expected\n");
				exit(1);
			}
			int num_vma = 0;
			line_ >> num_vma;
			trace("Number of VMAs: %d\n", num_vma);
			while (num_vma) {
				if (!GetNextValidLine()) {
					trace("VMA expected\n");
					exit(1);
				}
				
				unsigned int start_vpage, end_vpage;
				bool write_protected, file_mapped;
				line_ >> start_vpage >> end_vpage >> write_protected >> file_mapped;
				VMA vma{start_vpage, end_vpage, write_protected, file_mapped};
				trace("VMA: %d %d %d %d\n", vma.start_vpage,
											vma.end_vpage,
											vma.write_protected,
											vma.file_mapped);
			
				process_pool.back().vmas.push_back(vma);
				num_vma--;
			}
			num_process--;
		}
	}
	
	bool GetNextInstruction(char &operation, int &operand) {
		if (GetNextValidLine()) {
			line_ >> operation >> operand;
			return true;
		}	
		return false;
	}	
};

void InitializeFrameTable() {
	for (auto i = 0; i < NUM_FRAMES; i++) {
		frame_table.push_back(Frame());
		// All frames initially are in a free pool
		free_frames.push_back(i);
	}
	stringstream buffer;
	buffer << frame_table;
	trace("FT: %s", &buffer.str()[0]);
}

void InitializePager(char algo, string rfile_name) {	
	switch(algo) {
		case 'f':
			THE_PAGER = new FifoPager(OPTION_a);
			break;
		case 'r':
			THE_PAGER = new RandomPager(OPTION_a, rfile_name);
			break;
		case 'c':
			THE_PAGER = new ClockPager(OPTION_a);
			break;
		case 'e':
			THE_PAGER = new ESCPager(OPTION_a);
			break;
		case 'a':
			THE_PAGER = new AgingPager(OPTION_a);
			break;
		case 'w':
			THE_PAGER = new WorkingSetPager(OPTION_a);
			break;
		default:
			cerr << "Unknown Replacement Algorithm: <" << algo << ">" << endl;
			exit(1);
	}
}




void Simulation(InputReader &reader) {
	PageFaultHandler pf_handler = PageFaultHandler();	
	char operation;
	int operand;
	while (reader.GetNextInstruction(operation, operand)) {
		if (OPTION_O) {
            cout << INSTR_COUNT << ": ==> "
                 << operation << " "
                 << operand << endl;
        }
		INSTR_COUNT++;
		
		// handle special case of 'c' and 'e' instruction
		if (operation == 'c') { // context switch i
			CTX_SWITCHES++;
			CURRENT_PID = operand;					
			COST += CTX_SWITCHES_COST;
			continue;
		}
		
		if (operation == 'e') {
			cout << "EXIT current process " << operand << endl;
			PROCESS_EXITS++;
			COST += PROC_EXITS_COST;
			
			// traverse through the process' page table and release the memory
			Process &proc = process_pool[operand];
			for (auto i = 0; i < proc.page_table.size(); i++) {
				PTE &pte = proc.page_table[i];
				if (pte.present) {
					// unmap the active page
					if (OPTION_O) {
						// proc id:vpage
						cout << " UNMAP " << operand << ":" << i << endl;
					}
					proc.unmaps++;
					COST += UNMAPS;
					frame_table[pte.frame].mapped = false;
					// store modified filemapped page to disk
					if (pte.file_mapped && pte.modified) {
                    				if (OPTION_O) {
                        				cout << " FOUT" << endl;
                        			}
                        			proc.fouts++;
                        			COST += FOUTS;
                    			}	
					pte.present = false;
					// return used frame to the free pool
					free_frames.push_back(pte.frame);
				}
				// Note that dirty non-filemapped pages are not written back to disk
				pte.paged_out = false;
			}
			continue;
		}			
		// handle instructions for read and write
		COST += READ_WRITE;	
		PTE *pte = &current_process().page_table[operand];		
		if (!pte->present) {
			trace("pte not present => page fault exeception\n");
			if (!pf_handler.HandlePageFaultException(*pte, operand)) continue;	
		}
		// now the page is present
		
		// simulate instruction execution by hardware 
		// by updaing the R/M PTE bits
		if (operation == 'w') {
			pte->referenced = true;
			// check write protection
			if (pte->write_protected) {
				if (OPTION_O) {
					cout << " SEGPROT" << endl;
				}
				current_process().segprot++;
				COST += SEGPROT;	
			} else {
				trace("Set wrtie modified");
				pte->modified = true;
			}
		}
		if (operation == 'r') {
			trace("Set Read Referenced\n");
			pte->referenced = true;
		}

		if (OPTION_x) {
        	cout << "PT[" << CURRENT_PID << "]: "
                 << current_process().page_table << endl;
        }	
	}
}


int main(int argc, char *argv[]) {
		
	char algo = 'f';
	ParseOptArgs(argc, argv, algo);
	trace("algorithm: %c\n", algo);
	
	string infile_name = "";
	string rfile_name = "";
	ParseNonOptArgs(algo, argv, infile_name, rfile_name);

	InitializeFrameTable();

	InputReader input_reader(infile_name); 
	input_reader.CreateProcess();
	
	InitializePager(algo, rfile_name);
	Simulation(input_reader);
	delete THE_PAGER;

	if (OPTION_P) {
        for (size_t i = 0; i < process_pool.size(); i++) {
            Process &proc = process_pool[i];
            cout << "PT[" << i << "]:" << proc.page_table << endl;
        }
    }
	
	if (OPTION_F) {
		cout << "FT:" << frame_table << endl;
	}

	if (OPTION_S) {
		for (auto i = 0; i < process_pool.size(); i++) {
			Process &proc = process_pool[i];
			cout << "PROC[" << i << "]:" << proc << endl; //TODO
		}
		
		cout << "TOTALCOST " << INSTR_COUNT << " "
                             << CTX_SWITCHES << " "
                             << PROCESS_EXITS << " "
                             << COST << " "
                             << sizeof(PTE) << endl;
	}
	
	return 0;	

}
