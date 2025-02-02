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
    vector<int> instructions; // Store instructions for loading into memory
};

// Function to display PCB data
void displayPCBs(const vector<PCB>& pcbs) {
    cout << "\nProcess Control Blocks:\n";
    for (const auto& pcb : pcbs) {
        cout << "Process ID: " << pcb.processID 
             << ", State: " << pcb.state 
             << ", Program Counter: " << pcb.programCounter 
             << ", CPU Cycles Used: " << pcb.cpuCyclesUsed 
             << ", Register Value: " << pcb.registerValue << "\n";
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

        // Read instructions
        int instr;
        for (int j = 0; j < numInstructions; j++) {
            if (!(ss >> instr)) {
                cerr << "Error: Missing instruction data for process " << pcb.processID << endl;
                return false;
            }
            pcb.instructions.push_back(instr);
        }
        
        pcbs.push_back(pcb);
    }
    
    return true;
}

// Function to execute a process
void executeProcess(PCB& pcb, vector<int>& memory) {
    pcb.state = RUNNING;
    int instructionPointer = pcb.instructionBase;
    
    while (instructionPointer < pcb.dataBase) {
        int opcode = memory[instructionPointer];
        instructionPointer++;

        switch (opcode) {
            case 1: { // Compute (simulated CPU cycles)
                if (instructionPointer + 1 >= pcb.dataBase) break;
                int iterations = memory[instructionPointer++];
                int cycles = memory[instructionPointer++];
                pcb.cpuCyclesUsed += (iterations * cycles);
                break;
            }
            case 2: { // Print (Output a value)
                if (instructionPointer >= pcb.dataBase) break;
                int value = memory[instructionPointer++];
                cout << "Process " << pcb.processID << " Output: " << value << endl;
                break;
            }
            case 3: { // Store (Save value to memory)
                if (instructionPointer + 1 >= pcb.dataBase) break;
                int value = memory[instructionPointer++];
                int address = memory[instructionPointer++];
                if (address >= 0 && address < memory.size()) {
                    memory[address] = value;
                }
                break;
            }
            case 4: { // Load (Retrieve value from memory)
                if (instructionPointer >= pcb.dataBase) break;
                int address = memory[instructionPointer++];
                if (address >= 0 && address < memory.size()) {
                    pcb.registerValue = memory[address];
                }
                break;
            }
            default:
                cout << "Process " << pcb.processID << " encountered an unknown opcode: " << opcode << endl;
                break;
        }
    }
    
    pcb.state = TERMINATED;
}

// Function to allocate memory for processes and load instructions
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
        pcb.dataBase = pcb.instructionBase + pcb.instructions.size(); // Instructions first, then data

        // Mark PCB storage in memory
        for (int i = 0; i < 10; i++) {
            memory[pcb.mainMemoryBase + i] = pcb.processID;
        }

        // Load instructions into memory
        int instrAddress = pcb.instructionBase;
        for (int instr : pcb.instructions) {
            if (instrAddress >= maxMemorySize) {
                cerr << "Error: Instruction memory overflow for process " << pcb.processID << endl;
                return false;
            }
            memory[instrAddress++] = instr;
        }

        currentAddress += pcb.maxMemoryNeeded;
    }
    return true;
}

int main() {
    int maxMemorySize;
    vector<PCB> pcbs;
    vector<int> memory;
    
    if (parseInput(maxMemorySize, pcbs)) {
        cout << "Max Memory Size: " << maxMemorySize << endl;
        
        if (allocateMemory(maxMemorySize, pcbs, memory)) {
            displayPCBs(pcbs);
            
            if (!pcbs.empty()) {
                executeProcess(pcbs[0], memory);
                cout << "\nAfter Execution:\n";
                displayPCBs(pcbs);
            }
        } else {
            cerr << "Memory allocation failed." << endl;
        }
    } else {
        cerr << "Parsing failed due to errors." << endl;
    }
    
    return 0;
}
