#include <iostream>
#include <fstream>
#include <string>

// for strtok()
#include <cstring>
// for printf()
#include <cstdio>

using namespace std;

class Tokenizer {
	ifstream infile; 
	string line = "";

	public:
	int linenum = 0;
	int lineoffset = 0;
	int finalPosition = 0;
	
	Tokenizer(string filename) {
		infile.open(filename);
	}
	
	void  loadline() {
		if (getline(infile, line)) { 
			linenum++;
			
			while (line.empty() && !infile.eof()) {
           		if (getline(infile, line)) {
					linenum++;
				}
			}

			finalPosition = line.size() + 1;
		} else {
			infile.close();
		}
	    // cout << line << endl;
	}

	char* getToken() {
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
			lineoffset = finalPosition;
		}
		return token;	
	}	
};

int main(int argc, char *argv[]) {
	Tokenizer tokenizer(argv[1]);
	char* token;	
	while ((token = tokenizer.getToken()) != NULL) {
		printf("Token: %d:%d : %s\n", tokenizer.linenum, tokenizer.lineoffset, token);
	}
	printf("EOF position %d:%d\n", tokenizer.linenum, tokenizer.lineoffset);
}
