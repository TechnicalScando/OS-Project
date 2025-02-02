#include <iostream>
#include <vector>
#include <sstream>
using namespace std;

// Structure to store individual instructions
struct Instruction {
    int type;
    vector<int> parameters;
};

// Structure to store process information
struct Process {
    int processID;
    int maxMemoryNeeded;
    int numInstructions;
    vector<Instruction> instructions;
};

// Function to parse input from standard input
bool parseInput(int& maxMemorySize, vector<Process>& processes) {
    string line;
    
    // Read the first line (maximum memory size)
    if (!getline(cin, line) || !(istringstream(line) >> maxMemorySize)) {
        cerr << "Error: Invalid max memory size format!" << endl;
        return false;
    }
    
    // Read the second line (number of processes)
    int numProcesses;
    if (!getline(cin, line) || !(istringstream(line) >> numProcesses)) {
        cerr << "Error: Invalid number of processes format!" << endl;
        return false;
    }
    
    // Read each process
    for (int i = 0; i < numProcesses; i++) {
        Process process;
        
        if (!getline(cin, line)) {
            cerr << "Error: Missing process data!" << endl;
            return false;
        }
        
        istringstream ss(line);
        if (!(ss >> process.processID >> process.maxMemoryNeeded >> process.numInstructions)) {
            cerr << "Error: Invalid process format!" << endl;
            return false;
        }
        
        // Read instructions from the same line
        for (int j = 0; j < process.numInstructions; j++) {
            Instruction instr;
            if (!(ss >> instr.type)) {
                cerr << "Error: Missing instruction type for process " << process.processID << endl;
                return false;
            }
            
            // Determine how many parameters are needed based on the instruction type
            int paramCount = 0;
            if (instr.type == 1) paramCount = 2; // Compute (iterations, cycles)
            else if (instr.type == 2) paramCount = 1; // Print (cycles)
            else if (instr.type == 3) paramCount = 2; // Store (value, address)
            else if (instr.type == 4) paramCount = 1; // Load (address)
            else {
                cerr << "Error: Invalid instruction type " << instr.type << " for process " << process.processID << endl;
                return false;
            }
            
            // Read required parameters
            for (int k = 0; k < paramCount; k++) {
                int param;
                if (!(ss >> param)) {
                    cerr << "Error: Missing parameter for instruction " << instr.type
                         << " in process " << process.processID << endl;
                    return false;
                }
                instr.parameters.push_back(param);
            }
            
            process.instructions.push_back(instr);
        }
        
        processes.push_back(process);
    }
    
    return true;
}

// Function to display parsed data
void displayProcesses(int maxMemorySize, const vector<Process>& processes) {
    cout << "Max Memory Size: " << maxMemorySize << endl;
    cout << "Processes Parsed: " << processes.size() << endl;
    for (const auto& process : processes) {
        cout << "Process ID: " << process.processID << ", Memory Needed: " << process.maxMemoryNeeded 
             << ", Instructions: " << process.numInstructions << endl;
        
        for (const auto& instr : process.instructions) {
            cout << "  Instruction Type: " << instr.type << " | Params: ";
            for (const auto& param : instr.parameters) {
                cout << param << " ";
            }
            cout << endl;
        }
    }
}

int main() {
    int maxMemorySize;
    vector<Process> processes;
    
    if (parseInput(maxMemorySize, processes)) {
        displayProcesses(maxMemorySize, processes);
    } else {
        cerr << "Parsing failed due to errors." << endl;
    }
    
    return 0;
}
