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

// Function to allocate memory for processes
bool allocateMemory(int maxMemorySize, vector<PCB>& pcbs, vector<int>& memory) {
    memory.resize(maxMemorySize, -1); // Initialize memory with -1 (empty space)
    int currentAddress = 0;

    for (auto& pcb : pcbs) {
        if (currentAddress + pcb.maxMemoryNeeded > maxMemorySize) {
            cerr << "Error: Not enough memory to allocate process " << pcb.processID << endl;
            return false;
        }

        pcb.mainMemoryBase = currentAddress;
        pcb.instructionBase = pcb.mainMemoryBase + 10; // Reserve 10 slots for PCB metadata
        pcb.dataBase = pcb.instructionBase + pcb.memoryLimit; // Instructions first, then data

        // Mark memory allocation
        for (int i = 0; i < 10; i++) {
            memory[pcb.mainMemoryBase + i] = pcb.processID; // PCB storage
        }
        for (int i = pcb.instructionBase; i < pcb.dataBase; i++) {
            memory[i] = 99; // Simulating instruction memory allocation
        }
        for (int i = pcb.dataBase; i < pcb.mainMemoryBase + pcb.maxMemoryNeeded; i++) {
            memory[i] = 88; // Simulating data memory allocation
        }

        currentAddress += pcb.maxMemoryNeeded; // Move to next available memory slot
    }
    return true;
}

// Function to display memory layout
void displayMemory(const vector<int>& memory, int maxDisplay = 20) {
    cout << "\nMemory Layout (Showing Used Memory):\n";
    cout << "Address\tContent" << endl;

    int printed = 0;
    bool skipping = false;

    for (size_t i = 0; i < memory.size(); i++) {
        if (memory[i] != -1) {  // Print only allocated memory
            if (skipping) {
                cout << "...\n";  // Indicate skipped empty regions
                skipping = false;
            }
            cout << i << "\t" << memory[i] << endl;
            printed++;
        } else {
            if (!skipping && printed > maxDisplay) {
                cout << "...\n";  // Start skipping
                skipping = true;
            }
        }
    }
}

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
        pcb.memoryLimit = pcb.maxMemoryNeeded;
        pcb.cpuCyclesUsed = 0;
        pcb.registerValue = 0;
        pcb.mainMemoryBase = 0;  // Will be set during allocation
        
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
             << ", Memory Base: " << pcb.mainMemoryBase 
             << ", Memory Needed: " << pcb.maxMemoryNeeded 
             << "\n";
    }
}

int main() {
    int maxMemorySize;
    vector<PCB> pcbs;
    vector<int> memory;
    
    if (parseInput(maxMemorySize, pcbs)) {
        cout << "Max Memory Size: " << maxMemorySize << endl;
        
        if (allocateMemory(maxMemorySize, pcbs, memory)) {
            displayPCBs(pcbs);
            displayMemory(memory);
        } else {
            cerr << "Memory allocation failed." << endl;
        }
    } else {
        cerr << "Parsing failed due to errors." << endl;
    }
    
    return 0;
}
