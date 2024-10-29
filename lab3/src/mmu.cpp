#include "mmu.h"
#include <iomanip>
#include <sstream> // for stringstream

using namespace std;

extern bool OPTION_O;

unsigned int NUM_FRAMES = 4;
unsigned int MAX_VPAGES = 64;
unsigned long CURRENT_PID = 0;
vector<Process> process_pool;
vector<Frame> frame_table;
deque<unsigned int> free_frames;

Pager *THE_PAGER = nullptr;

unsigned long long COST = 0;

ostream &operator<<(ostream &os, const vector<PTE> &page_table) {
	for (auto i = 0; i < page_table.size(); i++) {
		const PTE &pte = page_table[i];
		if (pte.present) {
			os << " " << i << ":"
			   << (pte.referenced ? "R" : "-")
			   << (pte.modified ? "M" : "-")
			   << (pte.paged_out ? "S" : "-");
		} else {
			os << (pte.paged_out ? " #" : " *");
		}
	}
	return os;
}

ostream &operator<<(ostream &os, const vector<Frame> &frame_table) {
	for (auto frame: frame_table) {
		if (frame.mapped) {
			os << " " << frame.pid << ":" << frame.vpage;
		} else {
			os << " *";
		}
	}
	return os;	
}

ostream& operator << (ostream& os, const Process& process) {
    os << " U=" << process.unmaps
       << " M=" << process.maps
       << " I=" << process.ins
       << " O=" << process.outs
       << " FI=" << process.fins
       << " FO=" << process.fouts
       << " Z=" << process.zeros
       << " SV=" << process.segv
       << " SP=" << process.segprot;
    return os;
}

/*
 * Fifo Pager
 */
unsigned int FifoPager::SelectVictimFrame() {
	hand = hand % NUM_FRAMES; // wraparound
	if (OPTION_a) {
		cout << "ASELECT " << hand << endl;
	}
	return hand++;
}

/*
 * Reverse Mapping Frame -> PTE
 */

inline PTE& FrameToPTE(unsigned int i) {
	unsigned int pid = frame_table[i].pid;
    unsigned int vpage = frame_table[i].vpage;
    return process_pool[pid].page_table[vpage];
}


/*
 * Clock Pager
 */
unsigned int ClockPager:: SelectVictimFrame() {
		unsigned int start = hand;
		unsigned int counter = 0;

		while (true) {
			counter++;
            PTE& pte = FrameToPTE(hand);
            if (pte.referenced) {
				// clear R and advance hand 
            	pte.referenced = false;
                hand = (hand+1) % NUM_FRAMES;
            }
            else break;
		}

		if (OPTION_a) {
            cout << "ASELECT " << start
                 << " " << counter << endl;
        }
		unsigned int victim = hand;
 		hand = (hand+1) % NUM_FRAMES;
		return victim;       
}

/*
 * ESC Pager
 *
 * 			 R	 M
 * Class 0:  0   0
 * Class 1:  0   1
 * Class 2:  1   0
 * Class 3:  1   1
 *
 * class_index = 2*R + M
 */

unsigned int ESCPager::SelectVictimFrame() {
	vector<int> classes(4, -1); // to store the first frame that falls into each class
	unsigned int start = hand;
	traceM("start: %d\n", start);
    unsigned int counter  = 0;	
	int victim = -1;
	bool reset = INSTR_COUNT-last_R_reset >= reset_cycle;
	if (reset) last_R_reset = INSTR_COUNT;
	traceM("Reset: %d\n", last_R_reset);
	do {
		counter++;
		PTE &pte = FrameToPTE(hand);
		int	class_index = pte.referenced*2 + pte.modified;
		traceM("class index: %d\n", class_index);
		if (classes[class_index] == -1) {
			classes[class_index] = hand;
			traceM("store class_index %d, hand %d\n", class_index, hand);
		}

		if (reset) {
			pte.referenced = false;
		} else {
			if (class_index == 0) {
				victim = hand;
				traceM("Encounter class 0, hand %d\n", hand);
				break;
			}
		}
		hand = (hand+1) % NUM_FRAMES;
	} while (hand != start);

	auto iter = classes.begin();
	if (victim == -1) {
		// search first non-empty class
		while (*iter == -1) {
			iter++;
			traceM("frame %d\n", *iter);
		}
		victim = *iter;
	}

    if (OPTION_a) {
		cout << "ASELECT: " << setw(2) << start
			 << " " << setw(1) << reset
			 << " | " << setw(1) << (iter - classes.begin())
			 << " " << setw(2) << victim
			 << " " << setw(2) << counter << setw(0) << endl;
	}
	hand = (victim+1) % NUM_FRAMES;
	traceM("victim: %d\n", victim);
	return victim;
}

/*
 * Random Number Generator
 */

RandGenerator::RandGenerator(string rfile_name) {
	ifstream rfile(rfile_name);
	if (rfile) {
		rfile >> total;
		traceM("Initializing RandGenerator with %d numbers\n", total);
		int num;
		while (rfile >> num) {
			randvals.push_back(num);
		}
	} else {
		cerr << "Not a valid random file <" << rfile_name << ">" << endl;
		exit(1);
	}
}

/*
 * Aging Pager
 */

unsigned int AgingPager::SelectVictimFrame() {
	unsigned int start = hand;
	unsigned int min_f = hand;

	do {
		frame_table[hand].age >>= 1;
		PTE &pte = FrameToPTE(hand);
		if (pte.referenced) {
			// set the leading bit to 1 
			frame_table[hand].age |= 0x80000000;
			pte.referenced = false;
		}
		if (frame_table[hand].age < frame_table[min_f].age) {
			min_f = hand;
		}
		hand = (hand+1) % NUM_FRAMES;
	} while (hand != start);

	if (OPTION_a) {
		cout << "ASELECT " 
		     << start << "-" << (hand-1) % NUM_FRAMES << " | ";

		do {
			cout << start <<":";
			cout << hex << frame_table[start].age << dec << " ";
			start = (start+1) % NUM_FRAMES;
		} while (start != hand);
		// Frame selected
		cout << "| " << min_f << endl;
	}

	hand = (min_f+1) % NUM_FRAMES;
	return min_f;
}


unsigned int RandGenerator::myrandom(unsigned int upper_bound) {
	ofs++;
	traceM("ofs: %d, total %d\n", ofs, total);
	if (ofs == total) {
		traceM("Reseting offset to 0\n");
		ofs = 0;
	}
	traceM("Rand Val: %d\n", randvals[ofs]);
	return randvals[ofs] % upper_bound;
}

/*
 * Working Set Pager
 */
unsigned int WorkingSetPager::SelectVictimFrame() {
		unsigned int start = hand;
        unsigned int oldest = hand;
		unsigned int count = 0;
		stringstream buffer;
		do {
			PTE &pte = FrameToPTE(hand);
			count++;
			if (OPTION_a) {
            	buffer << hand << "(" << pte.referenced
                       << " " << frame_table[hand].pid << ":" << frame_table[hand].vpage
                       << " " << frame_table[hand].last_used << ") ";
            }
			if (pte.referenced) {
				frame_table[hand].last_used = INSTR_COUNT;
				pte.referenced = false;
			} else if (INSTR_COUNT - frame_table[hand].last_used > TAU) {
				if (OPTION_a) {
                    buffer << "STOP(" << count  << ") ";
                }	
				oldest = hand;
				break;
			}
	
			if (frame_table[hand].last_used < frame_table[oldest].last_used) {
				oldest = hand;
			}
			hand = (hand+1) % NUM_FRAMES;
		} while (hand != start);
		
		if (OPTION_a) {
            cout << "ASELECT " 
				 << start << "-" << (start-1) % NUM_FRAMES << " | "
                 << buffer.str() << "| " << oldest << endl;
        }
		hand = (oldest+1) % NUM_FRAMES;
		return oldest;
}


bool PageFaultHandler::HandlePageFaultException(PTE &pte, unsigned int vpage) {
 	traceM("PTE: %d%d%d\n", pte.referenced, pte.modified, pte.paged_out);
	// check if this is actually a page in vma
 	if (!pte.vma_checked) {
		pte.vma_checked = true;
		// search the VMA list
		for (auto &vma : current_process().vmas) {
			if (vpage >= vma.start_vpage && vpage <= vma.end_vpage) {
				pte.vma_valid = true;
				pte.file_mapped = vma.file_mapped;
                pte.write_protected = vma.write_protected;
				break;
			}
		}
	}
	
	// if it is part of a VMA then the page must be instantiated
	if (pte.vma_valid) {
		// assign the allocated frame to the PTE belonging to the vpage of instruction
		pte.frame = GetFrame();


		// Fill frame with the proper content
		if (pte.paged_out) {
            if (OPTION_O) {
                // bring the page back from the swap space
				cout << " IN" << endl;
            }
            current_process().ins++;
            COST += INS;
			// Note that the pageout flag will never be reset
			// as it indicates there is content on the swap device
		} else if (pte.file_mapped) {
            if (OPTION_O) {
                cout << " FIN" << endl;
            }
            current_process().fins++;
            COST += FINS;
        } else { // page still has zero filled content
            if (OPTION_O) {
                cout << " ZERO" << endl;
            }
            current_process().zeros++;
            COST += ZEROS;
        } 
		// Map its new user
		frame_table[pte.frame].pid = CURRENT_PID;
    	frame_table[pte.frame].vpage = vpage;
    	frame_table[pte.frame].mapped = true;
		if (OPTION_O) {
            cout << " MAP " << pte.frame << endl;
        }
		current_process().maps++;
 		THE_PAGER->ResetActivePageField(pte.frame); // for Aging and WorkingSet Algorithm
        COST += MAPS;
 		pte.present = true;
 		return true;
	} else {
 	// if not, raise error 
    	if (OPTION_O) {
			cout << " SEGV" << endl; // Segmentation Violation
 		}
		current_process().segv++;
 		COST += SEGV;
		return false;
 	}
}

int PageFaultHandler::AllocateFrameFromFreeList() {
	int f_idx = NUM_FRAMES;
	if (!free_frames.empty()) {
		f_idx = free_frames.front();
		free_frames.pop_front();
	}
	return f_idx;
}

unsigned int PageFaultHandler::GetFrame() {
	traceM("Get frame...\n");
	unsigned int f_idx = AllocateFrameFromFreeList();
	// If running out of free frames, implement paging
	if (f_idx == NUM_FRAMES) {
		f_idx = THE_PAGER->SelectVictimFrame(); 
		
		// Unmap allocated frame's current user
		unsigned int pid = frame_table[f_idx].pid;
        unsigned int vpage = frame_table[f_idx].vpage;
        if (OPTION_O) {
            cout << " UNMAP " << pid << ":" << vpage << endl;
        }
		Process &process = process_pool[pid];
		process.unmaps++;
		COST += UNMAPS;

		// Save allocated frame's old page to disk if necessary	
		PTE &pte = process_pool[pid].page_table[vpage];
        traceM("%d:%d PTE.modified: %d\n", pid, vpage, pte.modified);
		if (pte.modified) {
            if (pte.file_mapped) {
				if (OPTION_O) {
					cout << " FOUT" << endl;
				}
				process.fouts++;
				COST += FOUTS;
			} else {
				pte.paged_out = true;
				if (OPTION_O) {
					cout << " OUT" << endl;
				}
				process.outs++;
				COST += OUTS;
			}
        }
		
		// Reset PTE
        pte.present = false;
		pte.modified = false;
		pte.referenced = false;
	}
	return f_idx;			
}
