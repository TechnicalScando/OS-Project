#include <iostream>
#include <vector>
#include <sstream>
using namespace std;

// Enum for process states
enum ProcessState { NEW, READY, RUNNING, TERMINATED };

// Structure to store Process Control Block (PCB)
struct PCB {
    int processID;
    ProcessState state;
    int programCounter;
    int instructionBase;
    int dataBase;
    int memoryLimit;
    int cpuCyclesUsed;
    int registerValue;
    int maxMemoryNeeded;
    int mainMemoryBase;
};

// Function to parse input from standard input
bool parseInput(int& maxMemorySize, vector<PCB>& pcbs) {
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
        PCB pcb;
        
        if (!getline(cin, line)) {
            cerr << "Error: Missing process data!" << endl;
            return false;
        }
        
        istringstream ss(line);
        int numInstructions;
        if (!(ss >> pcb.processID >> pcb.maxMemoryNeeded >> numInstructions)) {
            cerr << "Error: Invalid process format!" << endl;
            return false;
        }
        
        // Initialize PCB fields
        pcb.state = NEW;
        pcb.programCounter = 0;
        pcb.instructionBase = 0; // Will be set later during memory allocation
        pcb.dataBase = 0;        // Will be set later
        pcb.memoryLimit = pcb.maxMemoryNeeded;
        pcb.cpuCyclesUsed = 0;
        pcb.registerValue = 0;
        pcb.mainMemoryBase = 0;  // Will be set later
        
        pcbs.push_back(pcb);
    }
    
    return true;
}

// Function to display PCB data
void displayPCBs(const vector<PCB>& pcbs) {
    cout << "\nProcess Control Blocks:\n";
    for (const auto& pcb : pcbs) {
        cout << "Process ID: " << pcb.processID 
             << ", State: " << pcb.state 
             << ", Program Counter: " << pcb.programCounter 
             << ", Memory Needed: " << pcb.maxMemoryNeeded 
             << ", Memory Limit: " << pcb.memoryLimit 
             << ", CPU Cycles Used: " << pcb.cpuCyclesUsed 
             << "\n";
    }
}

int main() {
    int maxMemorySize;
    vector<PCB> pcbs;
    
    if (parseInput(maxMemorySize, pcbs)) {
        cout << "Max Memory Size: " << maxMemorySize << endl;
        displayPCBs(pcbs);
    } else {
        cerr << "Parsing failed due to errors." << endl;
    }
    
    return 0;
}
