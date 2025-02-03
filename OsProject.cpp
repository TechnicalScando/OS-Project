#include <iostream>
#include <vector>

using namespace std;

struct Process {
    int processID;
    int maxMemoryNeeded;
    int numInstructions;
    vector<int> instructions;
};

// Function to parse all processes from input
vector<Process> parseProcesses(int& maxMemory, int& numProcesses) {
    cin >> maxMemory >> numProcesses;
    
    cout << "Max Memory: " << maxMemory << endl;
    cout << "Number of Processes: " << numProcesses << endl;

    vector<Process> processes;

    for (int p = 0; p < numProcesses; p++) {
        Process proc;
        cin >> proc.processID >> proc.maxMemoryNeeded >> proc.numInstructions;

        proc.instructions.clear();
        int opcode, operand;

        // Read numInstructions full operations
        for (int i = 0; i < proc.numInstructions; i++) {
            cin >> opcode;
            proc.instructions.push_back(opcode);

            int numOperands = 0;
            if (opcode == 1) numOperands = 2;  // Compute
            else if (opcode == 2) numOperands = 1;  // Print
            else if (opcode == 3) numOperands = 2;  // Store
            else if (opcode == 4) numOperands = 1;  // Load

            for (int j = 0; j < numOperands; j++) {
                cin >> operand;
                proc.instructions.push_back(operand);
            }
        }

        processes.push_back(proc);
    }

    return processes;
}


void allocateMemory(vector<int>& mainMemory, const vector<Process>& processes, int maxMemory, vector<int>& pcbLocations) {
    int currentAddress = 0;  // Start from the first available memory location

    for (const Process& proc : processes) {
        vector<int> instructionList;
        vector<int> operandList;

        // Separate opcodes and operands
        for (size_t i = 0; i < proc.instructions.size();) {
            int opcode = proc.instructions[i];
            instructionList.push_back(opcode);

            int numOperands = (opcode == 1) ? 2 : (opcode == 2) ? 1 : (opcode == 3) ? 2 : (opcode == 4) ? 1 : 0;

            // Store operands separately
            for (int j = 0; j < numOperands; j++) {
                if (i + 1 < proc.instructions.size()) {
                    operandList.push_back(proc.instructions[i + 1]);
                    i++;
                }
            }
            i++;
        }

        int instructionCount = instructionList.size();
        int operandCount = operandList.size();
        int remainingDataSpace = proc.maxMemoryNeeded - operandCount;
        int requiredMemory = 10 + instructionCount + proc.maxMemoryNeeded;

        // If not enough space, skip process
        if (currentAddress + requiredMemory > maxMemory) {
            cout << "Error: Not enough memory for process " << proc.processID << ". Skipping process.\n";
            continue;
        }

        // Store PCB at current address
        pcbLocations.push_back(currentAddress);  // Store PCB location
        mainMemory[currentAddress] = proc.processID;
        mainMemory[currentAddress + 1] = 0;  // State (NEW)
        mainMemory[currentAddress + 2] = 0;  // Program Counter
        mainMemory[currentAddress + 3] = currentAddress + 10;  // Instruction Base
        mainMemory[currentAddress + 4] = currentAddress + 10 + instructionCount;  // Data Base
        mainMemory[currentAddress + 5] = proc.maxMemoryNeeded;
        mainMemory[currentAddress + 6] = 0;  // CPU Cycles Used
        mainMemory[currentAddress + 7] = 0;  // Register Value
        mainMemory[currentAddress + 8] = proc.maxMemoryNeeded;
        mainMemory[currentAddress + 9] = currentAddress;  // Main Memory Base

        int instructionBase = currentAddress + 10;
        int dataBase = instructionBase + instructionCount;

        // Store Instructions
        int instrAddress = instructionBase;
        for (int instr : instructionList) {
            mainMemory[instrAddress++] = instr;
        }

        // Store Operands in Data Section
        int dataAddress = dataBase;
        for (int operand : operandList) {
            mainMemory[dataAddress++] = operand;
        }

        // Reserve remaining data section space
        for (int i = 0; i < remainingDataSpace; i++) {
            mainMemory[dataAddress + i] = -1;
        }

        // Move to next available address
        currentAddress += requiredMemory;
    }
}








#include <fstream>

// Function to dump main memory to a file
void dumpMemoryToFile(const vector<int>& mainMemory, const string& filename) {
    ofstream outFile(filename);
    
    if (!outFile) {
        cerr << "Error: Could not open file for writing: " << filename << endl;
        return;
    }

    outFile << "======= FULL MEMORY DUMP =======\n";
    
    for (size_t i = 0; i < mainMemory.size(); i++) {
        outFile << "Address " << i << ": " << mainMemory[i] << "\n";
    }

    outFile << "================================\n";
    
    outFile.close();
    cout << "Memory dump written to " << filename << endl;
}

void executeProcess(vector<int>& mainMemory, int pcbLocation) {
    int processID = mainMemory[pcbLocation];  
    int instructionBase = mainMemory[pcbLocation + 3];  
    int dataBase = mainMemory[pcbLocation + 4];  
    int memoryLimit = mainMemory[pcbLocation + 5];
    int& registerValue = mainMemory[pcbLocation + 7];  // Simulated CPU Register
    int& cpuCyclesUsed = mainMemory[pcbLocation + 6];  // Tracks total CPU cycles

    cout << "\nExecuting Process: " << processID;
    cout << "\nInstruction Base: " << instructionBase << endl;

    int programCounter = 0;  // Tracks instruction execution
    int operandPointer = dataBase;  // Tracks operand position

    while (instructionBase + programCounter < dataBase) {  
        int instructionAddress = instructionBase + programCounter;
        int opcode = mainMemory[instructionAddress];

        cout << "Instruction at address " << instructionAddress << ": " << opcode << endl;
        
        vector<int> operands;
        int numOperands = 0;

        if (opcode == 1) numOperands = 2;  // Compute
        else if (opcode == 2) numOperands = 1;  // Print
        else if (opcode == 3) numOperands = 2;  // Store
        else if (opcode == 4) numOperands = 1;  // Load

        for (int i = 0; i < numOperands; i++) {
            operands.push_back(mainMemory[operandPointer]);  
            operandPointer++;  
        }

        cout << "Operands: ";
        for (int op : operands) {
            cout << op << " ";
        }
        cout << endl;

        // **Compute (Opcode = 1)**
        if (opcode == 1) {
            int iterations = operands[0];
            int cpuCycles = operands[1];
            cpuCyclesUsed += cpuCycles;  // Increase total CPU cycles
            cout << "compute" << endl;
        }

        // **Print (Opcode = 2)**
        else if (opcode == 2) {
            int cpuCycles = operands[0];
            cpuCyclesUsed += cpuCycles;  // Increase total CPU cycles
            cout << "print" << endl;
        }

        // **Store (Opcode = 3)**
        else if (opcode == 3) {
            int value = operands[0];  // Value to store
            int logicalAddress = operands[1];  // Logical address within data section

            // Translate logical address to physical
            int physicalAddress = dataBase + logicalAddress;

            if (logicalAddress < memoryLimit) {
                mainMemory[physicalAddress] = value;
                cout << "stored" << endl;
            } else {
                cout << "store error!" << endl;
            }
        }

        // **Load (Opcode = 4)**
        else if (opcode == 4) {
            int logicalAddress = operands[0];  // Logical address within data section
            int physicalAddress = dataBase + logicalAddress;

            if (logicalAddress < memoryLimit) {
                registerValue = mainMemory[physicalAddress];  // Load value into register
                cout << "loaded" << endl;
            } else {
                cout << "load error!" << endl;
            }
        }

        programCounter++;  // Move to next instruction

        cout << "Updated Program Counter: " << programCounter << endl;
        cout << "-------------------------" << endl;
    }

    cout << "Process " << processID << " completed execution.\n";
}







int main() {
     int maxMemory, numProcesses;
    vector<int> pcbLocations;  // Store PCB locations for execution

    // Parse processes
    vector<Process> processes = parseProcesses(maxMemory, numProcesses);

    // Initialize main memory
    vector<int> mainMemory(maxMemory, -1);  // Initialize all memory to -1

    // Call memory allocation
    allocateMemory(mainMemory, processes, maxMemory, pcbLocations);

    // Dump memory to file for verification
    dumpMemoryToFile(mainMemory, "memory_dump.txt");

    // Execute each process (debugging operand fetching)
    cout << "\nNumber of Processes: " << numProcesses << endl;
    for (int pcbLocation : pcbLocations) {
        cout << "\nExecuting Process at PCB Location: " << pcbLocation << endl;
        executeProcess(mainMemory, pcbLocation);
    }
    

    return 0;
}











