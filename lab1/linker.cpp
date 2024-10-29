#include <iostream>
#include <fstream>
#include <string>

// for strtok()
#include <cstring>

// for regular expressio
#include <regex>

// for module base table
#include <vector>

// for output formatting
#include <iomanip>

#include <tuple>

using namespace std;

class Linker {
public:
	static const int LIST_SIZE = 16;
	static const int MACHINE_SIZE = 512;
	Linker(string filename): infilename(filename) {}
	void pass1(); 
	void pass2();	
	
private:
	enum PARSE_ERROR {
		NUM_EXPECTED,
		SYM_EXPECTED,
		MARIE_EXPECTED,
		SYM_TOO_LONG,
		TOO_MANY_DEF_IN_MODULE,
		TOO_MANY_USE_IN_MODULE,
		TOO_MANY_INSTR
	};
	
	class Tokenizer;
	struct Symbol {
		string name = "";
		int absAddr = 0;
		int moduleNum = 0;
		bool multipleTimesDefined = false;
		bool used = false;
	};

	void createSymbol(Symbol sym, int val);
	void checkSymbolAbsAddress(int defcount, int module_size);
	void printSymbolTable();
	void checkModuleSymbolUsed(vector<tuple<Symbol, bool>> uselist);
	void checkAllSymbolUsed();
	string infilename = "";
	int curr_module_num = 0;
	vector<int> module_base_table;
	vector<Symbol> symbol_table;
};

class Linker::Tokenizer {
public:
	Tokenizer(string filename);
	void loadline();
	char* getToken();
	int readInt();
	Symbol readSymbol();
	string readMARIE();	
	void parseError(int errCode); 
	bool eof = false;
	int linenum = 0;
	int lineoffset = 0;
	int endOfLinePosition = 0;

private:
	ifstream infile; 
	string line = "";
};

Linker::Tokenizer::Tokenizer(string filename) {
	infile.open(filename);
	if (!infile) {
		cout << "Not a valid inputfile <" << filename << ">" << endl;
		exit(0);
	}
}

void Linker::Tokenizer::loadline() {
	if (getline(infile, line)) { 
		linenum++;
		
		while (line.empty() && !infile.eof()) {
			if (getline(infile, line)) {
				linenum++;
			}
		}

		endOfLinePosition = line.size() + 1;
	}
	
	if (infile.peek() == EOF) {
		eof = true;
	} 
}

char* Linker::Tokenizer::getToken() {
	char* token = nullptr;
	const char delimiters[] = {' ', '\t'};
	// load the first line and get the first token
	if (linenum == 0) {
		loadline();
		token = strtok(&line[0], delimiters);
	} else {	
		token = strtok(NULL, delimiters);
	}
	
	// if it reaches the end of a line but 
	// not end of the file, load a new line
	if (!token && !infile.eof()) {
		loadline(); 
		token = strtok(&line[0], delimiters);
	}
	
	// set line offset
	if (token) {
		lineoffset = token - &line[0] + 1;
	} else {
		lineoffset = endOfLinePosition;
	}
	return token;	
}

int Linker::Tokenizer::readInt() {
	char* token = getToken();
	
	if (token == NULL) {
		throw PARSE_ERROR::NUM_EXPECTED;
	}
	string tok(token);
	regex pattern("^[0-9]+$");
	if (!regex_match(tok, pattern)) { 
		throw PARSE_ERROR::NUM_EXPECTED;		
	}	
	
	return atoi(token);
}	


Linker::Symbol Linker::Tokenizer::readSymbol() {
	char* token = getToken();
	if (token == NULL) {
		throw PARSE_ERROR::SYM_EXPECTED;
	}
	string name(token);	
	regex pattern("^[a-zA-Z][a-zA-Z0-9]*$");
	if (!regex_match(name, pattern)) {
		throw PARSE_ERROR::SYM_EXPECTED;
	}	
	
	if (name.size() > 16) {
		throw PARSE_ERROR::SYM_TOO_LONG;
	}
	
	Symbol symbol = {name};
	return symbol;
}

string Linker::Tokenizer::readMARIE() {
	char* token = getToken();
	if (token == NULL) {
		throw PARSE_ERROR::MARIE_EXPECTED;
	}	
	string addrmode(token);
	regex pattern("^[MARIE]$");
	if (!regex_match(addrmode, pattern)) {
		throw PARSE_ERROR::MARIE_EXPECTED;
	} 

	return addrmode;
}

void Linker::Tokenizer::parseError(int errCode) {
	static string errStr[] = {
		"NUM_EXPECTED",
		"SYM_EXPECTED",
		"MARIE_EXPECTED",
		"SYM_TOO_LONG",
		"TOO_MANY_DEF_IN_MODULE",
		"TOO_MANY_USE_IN_MODULE",
		"TOO_MANY_INSTR"
	};
	cout << "Parse Error line " << linenum<< " offset " << lineoffset << ": "
		 << errStr[errCode] << endl;
}	

void Linker::pass1() {
	Tokenizer tokenizer(infilename);
	int curr_base_addr = 0;
	
	try {
		while (!tokenizer.eof) {
			// this is a new model with base address at curr_base_addr 
			curr_module_num += 1;
			module_base_table.push_back(curr_base_addr);
			//cout << "module " << curr_module_num << endl;
			
			// parse definition list
			int defcount = tokenizer.readInt();
			if (defcount > LIST_SIZE) {
				throw PARSE_ERROR::TOO_MANY_DEF_IN_MODULE;
			}
			//cout << defcount << " ";
			for (int i = 0; i < defcount; i++) {
				auto symbol = tokenizer.readSymbol();
				int	val = tokenizer.readInt();
				//cout << symbol.name << " " << val << " ";
				createSymbol(symbol, val);
			}
			//cout << endl;	
			// parse use list
			int usecount = tokenizer.readInt();
			if (usecount > LIST_SIZE) {
				throw PARSE_ERROR::TOO_MANY_USE_IN_MODULE;
			}
			//cout << usecount << " ";
			for (int i = 0; i < usecount; i++) {
				auto symbol = tokenizer.readSymbol();
				//cout  << symbol.name << " ";
			}
			//cout << endl;
			// parse program text		
			int instcount = tokenizer.readInt();
			if ((curr_base_addr + instcount) > MACHINE_SIZE) {
				throw PARSE_ERROR::TOO_MANY_INSTR;
			}
			//cout << instcount << " ";
			for (int i = 0; i < instcount; i++) {  
				string addrmode = tokenizer.readMARIE();
				int operand = tokenizer.readInt();
				//cout << addrmode << " " << operand << endl;
			}
			
			// if new symbols are defined, check if they are within the size of the module	
			if (defcount) {
				checkSymbolAbsAddress(defcount, instcount);			
			}
			curr_base_addr += instcount;
		}
		printSymbolTable();

	} catch (PARSE_ERROR errCode) {
		tokenizer.parseError(errCode);
		exit(0);
	}	
}


void Linker::checkSymbolAbsAddress(int defcount, int module_size) {
	int module_base = module_base_table[curr_module_num - 1];	
	int last_module_address = module_base + module_size - 1;
	
	int count = defcount;
	auto symbol = symbol_table.end();
	while (count) {
		symbol--;	
		if (symbol->absAddr > last_module_address) {
			cout << "Warning: Module " << curr_module_num << ": " << symbol->name 
				<< " too big " << symbol->absAddr - module_base
				<< " (max=" << module_size - 1
				<< ") assume zero relative" <<  endl; 	
			symbol->absAddr = module_base;
		}
		count -= 1;
	}
}

void Linker::createSymbol(Symbol sym, int val) {
	int curr_module_base = module_base_table[curr_module_num - 1];	
	bool exist = false;
	// if symbol is in the table
	for (auto &s: symbol_table) {
		if (s.name == sym.name && val != -1) {
			s.multipleTimesDefined = true;
			cout << "Warning: Module " << curr_module_num << ": " << s.  name
				<< " redefinition ignored" << endl;
			exist = true;
		}
	}
	
	// if symbol is not in the table
	if (!exist) {
		if (val == -1) {
			sym.absAddr = val;
		} else {
			sym.absAddr = curr_module_base + val;
		}
		sym.moduleNum = curr_module_num;
		symbol_table.push_back(sym);
	}
}

void Linker::printSymbolTable() {
	// print the symbol table, and add error message
	// to the multiple defined symbols
	cout << "Symbol Table" << endl;	
	for (auto sym: symbol_table) {
		if (sym.absAddr == -1) {
			continue;
		}

		cout << sym.name << "=" << sym.absAddr;
		if (sym.multipleTimesDefined) {
			cout << " Error: This variable is multiple times defined; first value used" << endl;
		} else { 
			cout << endl;
		}
	}	
	cout << endl;	
}


void Linker::pass2() {
	Tokenizer tokenizer(infilename);
	curr_module_num = 0;
	int curr_base_addr = 0;
	cout << "Memory Map\n";	
	
	while(!tokenizer.eof) {
		// this is a new module
		curr_module_num += 1;

		// parse definition list
		int defcount = tokenizer.readInt();
		for (int i = 0; i < defcount; i++) {
			auto symbol = tokenizer.readSymbol();
			int	val = tokenizer.readInt();
		}
		
		// parse use list
		int usecount = tokenizer.readInt();
		vector<tuple<Symbol, bool>> uselist;
		for (int i = 0; i < usecount; i++) {
			auto symbol = tokenizer.readSymbol();
			uselist.push_back(make_tuple(symbol, false));
		}
		
		// parse program text		
		int instcount = tokenizer.readInt();
		int module_base = module_base_table[curr_module_num - 1];
		for (int i = 0; i < instcount; i++) {  
			string addrmode = tokenizer.readMARIE();
			int instcode = tokenizer.readInt();
		//	cout << addrmode << " " << instcode << endl;
						
			// print out absolute address
			cout << setfill('0') << setw(3) << curr_base_addr << ": "; 
			
			// process instruction code	
			int opcode = instcode / 1000;
			int operand = instcode % 1000;
			if (opcode >= 10) {
				opcode = 9;
				operand = 999;
				cout << opcode << setfill('0') << setw(3) << operand << " ";
				cout << "Error: Illegal opcode; treated as 9999" << endl;
				curr_base_addr += 1;
				continue;
			} else {
				cout << opcode << setfill('0') << setw(3);
			}
			switch (addrmode[0]) {
				case 'M':
					// out of bound
					if (operand > module_base_table.size() - 1) {
						cout << "000 ";
						cout << "Error: Illegal module operand ; treated as module=0" << endl;
					} else {
						cout << module_base_table[operand] << endl;
					}
					break;

				case 'A':
					if (operand >= 512) {
						cout << "000 "; 
						cout << "Error: Absolute address exceeds machine size; zero used" << endl;
					} else {
						cout << operand << endl;
					}
					break;

				case 'R':
					if (operand > instcount - 1) {
						cout << module_base << " ";
						cout << "Error: Relative address exceeds module size; relative zero used" << endl; 
					} else {
						cout << operand + module_base << endl;
					}
					break;

				case 'I': 
					if (operand >= 900) {
						cout << "999 ";
						cout << "Error: Illegal immediate operand; treated as 999" << endl; 
					} else {
						cout << operand << endl;
					}
					break;

				case 'E': // replace the operand by symbol absolute address
					if (operand > usecount - 1) {
						cout << module_base << " ";
						cout << "Error: External operand exceeds length of uselist; treated as relative=0" << endl;
						break;
					}
					// valid operand
					bool defined = false;
					auto symbol = get<0>(uselist[operand]);
					
					for (auto &s: symbol_table) {
						if (s.name == symbol.name) {
							cout << s.absAddr << endl;
							s.used = true;
							defined = true;;
							break;
						}
					}
					
					if (!defined) {
						cout << "000 ";
						cout << "Error: " << symbol.name << " is not defined; zero used" << endl;
					}
					
					get<1>(uselist[operand]) = true;
					break;
			}
			curr_base_addr += 1;	
		}
		checkModuleSymbolUsed(uselist);	
	}
	checkAllSymbolUsed();
}

void Linker::checkModuleSymbolUsed(vector<tuple<Symbol, bool>> uselist) {
	for (int i = 0; i < uselist.size(); i ++ ) {
		if (!get<1>(uselist[i])) {
			cout << "Warning: Module " << curr_module_num << ": uselist[" << i << "]="
				<< (get<0>(uselist[i])).name << " was not used" << endl;
		}
	}
}

void Linker::checkAllSymbolUsed() {
	cout << endl;
	for (auto sym: symbol_table){
		if (!sym.used) {
			cout << "Warning: " << "Module " << sym.moduleNum 
				<< ": " << sym.name << " was defined but never used" << endl;
		}
	}
	cout << endl;
}

int main(int argc, char *argv[]) {
	Linker linker(argv[1]);
	
	linker.pass1();
	linker.pass2();
}
