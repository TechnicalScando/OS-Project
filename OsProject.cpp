#include <iostream>
#include <vector>

using namespace std;


struct Process {
    int processID;
    int maxMemoryNeeded;
    int numInstructions;
    vector<int> instructions;
};

// New parseProcesses function which now reads global parameters:
// maxMemory, numProcesses, CPUAllocated, and contextSwitchTime.
vector<Process> parseProcesses(int &maxMemory, int &numProcesses, int &globalCPUAllocated, int &contextSwitchTime) {
    cin >> maxMemory >> numProcesses >> globalCPUAllocated >> contextSwitchTime;
    
    vector<Process> processes;
    for (int p = 0; p < numProcesses; p++) {
        Process proc;
        cin >> proc.processID >> proc.maxMemoryNeeded >> proc.numInstructions;
        
        int opcode, operand;
        // Read each instruction (opcode plus its operands)
        for (int i = 0; i < proc.numInstructions; i++) {
            cin >> opcode;
            proc.instructions.push_back(opcode);
            
            int numOperands = 0;
            if (opcode == 1) numOperands = 2;  // Compute: two operands
            else if (opcode == 2) numOperands = 1;  // Print: one operand
            else if (opcode == 3) numOperands = 2;  // Store: two operands
            else if (opcode == 4) numOperands = 1;  // Load: one operand
            
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
        int totalProcessMemory = proc.maxMemoryNeeded;
        int remainingDataSpace = totalProcessMemory - instructionCount - operandCount; // Updated formula

        // Ensure remaining data space isn't negative (shouldn't happen if inputs are valid)
        if (remainingDataSpace < 0) {
            cout << "Error: Process " << proc.processID << " requires more memory than allocated!" << endl;
            continue;
        }

        int requiredMemory = 10 + totalProcessMemory;  // PCB + Total Memory (instructions + data)

        // If not enough space, skip process
        if (currentAddress + requiredMemory > maxMemory) {
            cout << "Error: Not enough memory for process " << proc.processID << ". Skipping process.\n";
            continue;
        }

        // Store PCB at current address
        pcbLocations.push_back(currentAddress);  // Store PCB location
        mainMemory[currentAddress] = proc.processID;
        mainMemory[currentAddress + 1] = 1;  // State (NEW)
        mainMemory[currentAddress + 2] = 0;  // Program Counter
        mainMemory[currentAddress + 3] = currentAddress + 10;  // Instruction Base
        mainMemory[currentAddress + 4] = currentAddress + 10 + instructionCount;  // Data Base
        mainMemory[currentAddress + 5] = totalProcessMemory;
        mainMemory[currentAddress + 6] = 0;  // CPU Cycles Used
        mainMemory[currentAddress + 7] = 0;  // Register Value
        mainMemory[currentAddress + 8] = totalProcessMemory;
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

void dumpMemoryToFile(const vector<int>& mainMemory, const string& filename) {
    ofstream outFile(filename);
    
    if (!outFile) {
        cerr << "Error: Could not open file for writing: " << filename << endl;
        return;
    }

    outFile << "======= FULL MEMORY DUMP =======\n";
    
    for (size_t i = 0; i < mainMemory.size(); i++) {
        outFile << i << ": " << mainMemory[i] << "\n";
    }

    outFile << "================================\n";
    
    outFile.close();
   
}







// Function to dump main memory to a file
void dumpMemoryToString(const vector<int>& mainMemory) {
   
    

    
    for (size_t i = 0; i < mainMemory.size(); i++) {
        cout <<  i << ": " << mainMemory[i] << "\n";
    }


    
}

void executeProcess(vector<int>& mainMemory, int pcbLocation, int& totalCpuCycles) {
   int processID = mainMemory[pcbLocation];  
    int& state = mainMemory[pcbLocation + 1];  
    int& programCounter = mainMemory[pcbLocation + 2];  
    int instructionBase = mainMemory[pcbLocation + 3];  
    int dataBase = mainMemory[pcbLocation + 4];  
    int memoryLimit = mainMemory[pcbLocation + 5];  
    int& cpuCyclesUsed = mainMemory[pcbLocation + 6];  
    int& registerValue = mainMemory[pcbLocation + 7];  
    int maxMemoryNeeded = mainMemory[pcbLocation + 8];  
    int mainMemoryBase = mainMemory[pcbLocation + 9];  

    state = 1; // Set process state to RUNNING

    int operandPointer = dataBase; //  Independent operand tracking


    while (instructionBase + programCounter < dataBase) {
        int instructionAddress = instructionBase + programCounter;
        int opcode = mainMemory[instructionAddress];


        vector<int> operands;
        int numOperands = (opcode == 1) ? 2 : (opcode == 2) ? 1 : (opcode == 3) ? 2 : (opcode == 4) ? 1 : 0;


        for (int i = 0; i < numOperands; i++) {
            operands.push_back(mainMemory[operandPointer]);
            operandPointer++;  //  Correctly move operand pointer
        }

    

        // Execute instruction & track CPU usage
        if (opcode == 1) {  // Compute
            int iterations = operands[0];
            int cpuCycles = operands[1];
            cpuCyclesUsed += cpuCycles;
            totalCpuCycles += cpuCycles;
            cout << "compute" << endl;
        } 
        else if (opcode == 2) {  // Print
            int cpuCycles = operands[0];
            cpuCyclesUsed += cpuCycles;
            totalCpuCycles += cpuCycles;
            cout << "print" << endl;
        } 
        else if (opcode == 3) {  // Store
            int value = operands[0];
            int logicalAddress = operands[1];
            int physicalAddress = instructionBase + logicalAddress;

            registerValue = value;
            if (logicalAddress < memoryLimit) {
                mainMemory[physicalAddress] = value;
                
                cout << "stored "  << endl;
            } else {
                
                cout << "store error!" << endl;
            }
            cpuCyclesUsed++;  //  Store takes 1 cycle
            totalCpuCycles++;
        } 
        else if (opcode == 4) {  // Load
            int logicalAddress = operands[0];
            int physicalAddress = instructionBase + logicalAddress;

            if (logicalAddress < memoryLimit) {
                registerValue = mainMemory[physicalAddress];
                cout << "loaded" <<  endl;
            } else {
                cout << "load error!" << endl;
            }
            cpuCyclesUsed++;  //  Load takes 1 cycle
            totalCpuCycles++;
        }

        programCounter++;

       
    }

    state = 3; // Set process state to TERMINATED
     // Print final PCB state
    cout << "Process ID: " << processID << "\n";
    cout << "State: TERMINATED\n";
    cout << "Program Counter: " << instructionBase - 1 << "\n";
    cout << "Instruction Base: " << instructionBase << "\n";
    cout << "Data Base: " << dataBase << "\n";
    cout << "Memory Limit: " << memoryLimit << "\n";
    cout << "CPU Cycles Used: " << cpuCyclesUsed << "\n";
    cout << "Register Value: " << registerValue << "\n";
    cout << "Max Memory Needed: " << maxMemoryNeeded << "\n";
    cout << "Main Memory Base: " << mainMemoryBase << "\n";
    cout << "Total CPU Cycles Consumed: " << cpuCyclesUsed << "\n";
    
}











// Simple test routine to verify parsing works correctly.
void printProcesses(const vector<Process>& processes, int globalCPUAllocated, int contextSwitchTime) {
    cout << "Global CPUAllocated (timeout limit): " << globalCPUAllocated << "\n";
    cout << "Context Switch Time: " << contextSwitchTime << "\n\n";
    for (const Process &p : processes) {
        cout << "Process ID: " << p.processID << "\n";
        cout << "Max Memory Needed: " << p.maxMemoryNeeded << "\n";
        cout << "Num Instructions: " << p.numInstructions << "\n";
        cout << "Instructions: ";
        for (int inst : p.instructions) {
            cout << inst << " ";
        }
        cout << "\n\n";
    }
}

int main() {
    int maxMemory, numProcesses, globalCPUAllocated, contextSwitchTime;
    
    // For testing, provide input in the following order:
    // maxMemory numProcesses CPUAllocated contextSwitchTime
    // Then, for each process:
    // processID maxMemoryNeeded numInstructions followed by the instructions (opcodes and operands)
    //
    // For example, you might use:
    // 1000 2 50 5
    // 1 200 2 1 10 3 2
    // 2 300 1 2 4
    //
    // You can enter these values manually or pipe them from a file.
    
    vector<Process> processes = parseProcesses(maxMemory, numProcesses, globalCPUAllocated, contextSwitchTime);
    printProcesses(processes, globalCPUAllocated, contextSwitchTime);
    
    return 0;
}











