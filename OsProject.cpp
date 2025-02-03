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


void allocateMemory(vector<int>& mainMemory, const vector<Process>& processes, int maxMemory) {
    int currentAddress = 0;  // Start from the first available memory location

    for (const Process& proc : processes) {
        vector<int> instructionList;  // Separate storage for instructions
        vector<int> operandList;      // Separate storage for operands

        // Step 1: Separate opcodes and operands
        for (size_t i = 0; i < proc.instructions.size();) {
            int opcode = proc.instructions[i];
            instructionList.push_back(opcode);  // Store the opcode

            int numOperands = 0;
            if (opcode == 1) numOperands = 2;  // Compute (2 operands)
            else if (opcode == 2) numOperands = 1;  // Print (1 operand)
            else if (opcode == 3) numOperands = 2;  // Store (2 operands)
            else if (opcode == 4) numOperands = 1;  // Load (1 operand)

            // Move operands to the operandList
            for (int j = 0; j < numOperands; j++) {
                if (i + 1 < proc.instructions.size()) {
                    operandList.push_back(proc.instructions[i + 1]);  // Store operand in separate list
                    i++;
                }
            }

            i++;  // Move to next opcode
        }

        int instructionCount = instructionList.size();
        int operandCount = operandList.size();
        
        // The total required data space is maxMemoryNeeded, but some of that space is occupied by operands.
        int remainingDataSpace = proc.maxMemoryNeeded - operandCount;

        int requiredMemory = 10 + instructionCount + proc.maxMemoryNeeded;  // PCB + Instructions + Data

        // Step 2: Check if there is enough memory
        if (currentAddress + requiredMemory > maxMemory) {
            cout << "Error: Not enough memory for process " << proc.processID << ". Skipping process.\n";
            continue;
        }

        // Step 3: Store PCB
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

        // Step 4: Store Instructions
        int instrAddress = instructionBase;
        for (int instr : instructionList) {
            mainMemory[instrAddress++] = instr;
        }

        // Step 5: Store Operands in Data Section
        int dataAddress = dataBase;
        for (int operand : operandList) {
            mainMemory[dataAddress++] = operand;  // Directly store operands in data section
        }

        // Step 6: Reserve the **remaining** data section space (excluding stored operands)
        for (int i = 0; i < remainingDataSpace; i++) {
            mainMemory[dataAddress + i] = 0;  // Fill rest of data section with 0
        }

        // Move to next available address
        currentAddress += requiredMemory;

        // Print Key Memory Information for this Process
        cout << "\nProcess " << proc.processID << " Memory Allocation:";
        cout << "\n  PCB Location: " << currentAddress - requiredMemory;
        cout << "\n  Instruction Base: " << instructionBase;
        cout << "\n  Data Base: " << dataBase;
        cout << "\n  Instructions: ";
        for (int i = instructionBase; i < dataBase; i++) {
            cout << mainMemory[i] << " ";
        }
        cout << "\n  First 5 Data Addresses: ";
        for (int i = dataBase; i < dataBase + 5 && i < maxMemory; i++) {
            cout << mainMemory[i] << " ";
        }
        cout << "\n---------------------------";
    }
}




int main() {
    int maxMemory, numProcesses;

    // Parse processes
    vector<Process> processes = parseProcesses(maxMemory, numProcesses);

    // Initialize main memory
    vector<int> mainMemory(maxMemory, -1);  // Initialize all memory to -1

    // Call memory allocation
    allocateMemory(mainMemory, processes, maxMemory);

    return 0;
}










