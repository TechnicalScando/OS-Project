#include <iostream>
#include <vector>
#include <queue>
#include <fstream>
#include <algorithm>
using namespace std;

// -----------------------------------------------------------------------------
// Process Structure and Constants
// -----------------------------------------------------------------------------
struct Process {
    int processID;
    int maxMemoryNeeded;   
    int numInstructions;
    vector<int> instructions;
    int startTime = -1;  
};

const int STATE_NEW = 0;
const int STATE_RUNNING = 1;
const int STATE_IO_WAITING = 2;
const int STATE_TERMINATED = 3;

int ioWaitTime = 0;

// -----------------------------------------------------------------------------
// CPU Clock and Context Switching
// -----------------------------------------------------------------------------
void updateClock(int &totalCpuCycles, int increment, const string &reason) {
    totalCpuCycles += increment;
}

void contextSwitch(int &totalCpuCycles, int contextSwitchTime, const string &reason) {
    updateClock(totalCpuCycles, contextSwitchTime, reason);
}

// -----------------------------------------------------------------------------
// Process Input Parsing
// -----------------------------------------------------------------------------
vector<Process> parseProcesses(int &maxMemory, int &numProcesses,
                               int &globalCPUAllocated, int &contextSwitchTime) 
{
    cin >> maxMemory >> globalCPUAllocated >> contextSwitchTime >> numProcesses;
    vector<Process> processes;
    processes.reserve(numProcesses);

    for (int p = 0; p < numProcesses; p++) {
        Process proc;
        cin >> proc.processID >> proc.maxMemoryNeeded >> proc.numInstructions;
        proc.instructions.clear();
        proc.instructions.reserve(proc.numInstructions * 3);

        int instructionsRead = 0;
        while (instructionsRead < proc.numInstructions) {
            int opcode;
            if (!(cin >> opcode)) break;
            proc.instructions.push_back(opcode);
            instructionsRead++;

            int numOperands = 0;
            switch (opcode) {
                case 1: numOperands = 2; break; // compute
                case 2: numOperands = 1; break; // print
                case 3: numOperands = 2; break; // store
                case 4: numOperands = 1; break; // load
                default: break;
            }
            for (int j = 0; j < numOperands; j++) {
                int operand;
                if (!(cin >> operand)) break;
                proc.instructions.push_back(operand);
            }
        }
        processes.push_back(proc);
    }
    return processes;
}

// -----------------------------------------------------------------------------
// Memory System
// -----------------------------------------------------------------------------
struct MemBlock {
    int processID;       
    int start;           
    int size;            
    vector<int> content; 
    MemBlock* next;      
};

MemBlock* initDynamicMemory(int maxMemory) {
    MemBlock* head = new MemBlock();
    head->processID = -1; 
    head->start = 0;
    head->size = maxMemory;
    head->next = nullptr;
    return head;
}

// Insert a free block in ascending 'start'
void insertFreeBlock(MemBlock* block, MemBlock*& freeList) {
    if (!freeList || block->start < freeList->start) {
        block->next = freeList;
        freeList = block;
        return;
    }
    MemBlock* curr = freeList;
    while (curr->next && curr->next->start < block->start) {
        curr = curr->next;
    }
    block->next = curr->next;
    curr->next = block;
}

// Coalesce adjacent free blocks
void coalesceFreeList(MemBlock*& freeList) {
    if (!freeList) return;
    MemBlock* curr = freeList;
    while (curr && curr->next) {
        if (curr->start + curr->size == curr->next->start) {
            MemBlock* temp = curr->next;
            curr->size += temp->size;
            curr->next = temp->next;
            delete temp;
        } else {
            curr = curr->next;
        }
    }
}

// Freed memory => insert into freeList WITHOUT coalescing
void freeMemoryBlock(MemBlock* block, MemBlock*& freeList) {
    int startAddr = block->start;
    int endAddr = block->start + block->size - 1;
    int procID = block->processID;
    block->processID = -1;
    insertFreeBlock(block, freeList);
    cout << "Process " << procID
         << " terminated and released memory from "
         << startAddr << " to " << endAddr << "." << endl;
}

// -----------------------------------------------------------------------------
// Global fakeMemory array for printing physical addresses
// -----------------------------------------------------------------------------
static vector<int> fakeMemory; // This will be resized to maxMemory in main()

// -----------------------------------------------------------------------------
// Allocation and Loading
// -----------------------------------------------------------------------------
MemBlock* allocateMemoryForJob(MemBlock*& freeList, Process &job) {
    int requiredSize = 10 + job.maxMemoryNeeded;
    MemBlock* current = freeList;
    MemBlock* prev = nullptr;

    while (current) {
        if (current->processID == -1 && current->size >= requiredSize) {
            if (current->size == requiredSize) {
                if (!prev) {
                    freeList = current->next;
                } else {
                    prev->next = current->next;
                }
                current->next = nullptr;
                current->processID = job.processID;
                current->content.resize(requiredSize, 0);
                return current;
            } else {
                MemBlock* allocated = new MemBlock();
                allocated->processID = job.processID;
                allocated->start = current->start;
                allocated->size = requiredSize;
                allocated->content.resize(requiredSize, 0);
                current->start += requiredSize;
                current->size -= requiredSize;
                return allocated;
            }
        }
        prev = current;
        current = current->next;
    }
    return nullptr;
}

bool loadJobIntoBlock(const Process &proc, MemBlock* block) {
    vector<int> instrList;
    vector<int> operandList;

    for (size_t i = 0; i < proc.instructions.size();) {
        int opcode = proc.instructions[i];
        instrList.push_back(opcode);
        int numOperands = 0;
        switch (opcode) {
            case 1: numOperands = 2; break;
            case 2: numOperands = 1; break;
            case 3: numOperands = 2; break;
            case 4: numOperands = 1; break;
            default: break;
        }
        for (int j = 0; j < numOperands; j++) {
            if ((i + 1) < proc.instructions.size()) {
                operandList.push_back(proc.instructions[i + 1]);
                i++;
            }
        }
        i++;
    }

    int instructionCount = (int)instrList.size();
    int operandCount = (int)operandList.size();
    int totalMem = proc.maxMemoryNeeded;
    int remainData = totalMem - (instructionCount + operandCount);

    if (remainData < 0) {
        cout << "Error: Process " << proc.processID
             << " requires more memory than allocated!" << endl;
        return false;
    }

    // Setup PCB in block->content (all stored as relative values)
    block->content[0] = proc.processID;         // ID
    block->content[1] = STATE_RUNNING;          // state
    block->content[2] = 0;                      // relative PC
    block->content[3] = 10;                     // relative instruction base
    block->content[4] = 10 + instructionCount;  // relative data base
    block->content[5] = totalMem;               // memory limit
    block->content[6] = 0;                      // CPU cycles used
    block->content[7] = 0;                      // register
    block->content[8] = totalMem;               // duplicate memory limit
    block->content[9] = block->start;           // physical base

    // Copy instructions and operands into block->content
    int instrIndex = 10;
    for (int instr : instrList) {
        if (instrIndex < (int)block->content.size())
            block->content[instrIndex++] = instr;
    }
    int dataIndex = 10 + instructionCount;
    for (int op : operandList) {
        if (dataIndex < (int)block->content.size())
            block->content[dataIndex++] = op;
    }
    for (int i = 0; i < remainData; i++) {
        if (dataIndex + i < (int)block->content.size())
            block->content[dataIndex + i] = -1;
    }

    // --- Here we update our fakeMemory array (which will be printed) ---
    // For each index in the allocated block, copy the block content.
    // For PCB fields at offsets 3 and 4, add the block's physical start.
    for (int i = 0; i < block->size; i++) {
        if (i == 3 || i == 4)
            fakeMemory[block->start + i] = block->content[i] + block->start;
        else
            fakeMemory[block->start + i] = block->content[i];
    }

    return true;
}

// -----------------------------------------------------------------------------
// loadWaitingJobs: Coalesce only if a job fails to fit on first try
// -----------------------------------------------------------------------------
void loadWaitingJobs(queue<int>& newJobQueue, 
                     vector<Process>& processes,
                     MemBlock*& freeList, 
                     queue<MemBlock*>& readyQueue) 
{
    if (newJobQueue.empty()) {
        return;
    }

    vector<int> waiting;
    while (!newJobQueue.empty()) {
        waiting.push_back(newJobQueue.front());
        newJobQueue.pop();
    }

    vector<int> stillWaiting;

    for (size_t i = 0; i < waiting.size(); i++) {
        int idx = waiting[i];
        Process &job = processes[idx];

        MemBlock* allocated = allocateMemoryForJob(freeList, job);
        if (!allocated) {
            cout << "Insufficient memory for Process " << job.processID 
                 << ". Attempting memory coalescing." << endl;
            coalesceFreeList(freeList);
            allocated = allocateMemoryForJob(freeList, job);
            if (!allocated) {
                cout << "Process " << job.processID 
                     << " waiting in NewJobQueue due to insufficient memory." << endl;
                stillWaiting.push_back(idx);
                for (size_t j = i+1; j < waiting.size(); j++) {
                    stillWaiting.push_back(waiting[j]);
                }
                break;
            } else {
                cout << "Memory coalesced. Process " << job.processID
                     << " can now be loaded." << endl;
                if (loadJobIntoBlock(job, allocated)) {
                    cout << "Process " << job.processID
                         << " loaded into memory at address "
                         << allocated->start
                         << " with size "
                         << allocated->size
                         << "." << endl;
                    readyQueue.push(allocated);
                }
            }
        } else {
            if (loadJobIntoBlock(job, allocated)) {
                cout << "Process " << job.processID
                     << " loaded into memory at address "
                     << allocated->start
                     << " with size "
                     << allocated->size
                     << "." << endl;
                readyQueue.push(allocated);
            }
        }
    }

    for (int idx : stillWaiting) {
        newJobQueue.push(idx);
    }
}

// -----------------------------------------------------------------------------
// Execution
// -----------------------------------------------------------------------------
int computeOperandPointerFromBlock(const vector<int>& mem,
                                   int instructionBase,
                                   int dataBase,
                                   int programCounter) 
{
    int operandCount = 0;
    for (int i = 0; i < programCounter; i++) {
        int opcode = mem[instructionBase + i];
        switch (opcode) {
            case 1: operandCount += 2; break;
            case 2: operandCount += 1; break;
            case 3: operandCount += 2; break;
            case 4: operandCount += 1; break;
            default: break;
        }
    }
    return dataBase + operandCount;
}

bool executeProcess(MemBlock* block,
                    int &totalCpuCycles,
                    int globalCPUAllocated,
                    int startTime) 
{
    vector<int> &mem = block->content;
    int processID = mem[0];
    int &state = mem[1];
    int &relProgramCounter = mem[2];
    int relInstructionBase = mem[3];
    int relDataBase       = mem[4];
    int memoryLimit       = mem[5];
    int &cpuCyclesUsed    = mem[6];
    int &registerValue    = mem[7];
    int maxMemoryNeeded   = mem[8];
    int mainMemoryBase    = mem[9];

    cout << "Process " << processID << " has moved to Running." << endl;
    state = STATE_RUNNING;

    int timeSliceCounter = 0;
    bool brokeEarly = false;

    int operandPointer = computeOperandPointerFromBlock(
        mem, relInstructionBase, relDataBase, relProgramCounter);

    while ((relInstructionBase + relProgramCounter) < relDataBase) {
        int opcode = mem[relInstructionBase + relProgramCounter];
        int numOperands = 0;
        switch (opcode) {
            case 1: numOperands = 2; break;
            case 2: numOperands = 1; break;
            case 3: numOperands = 2; break;
            case 4: numOperands = 1; break;
            default: break;
        }
        vector<int> operands(numOperands);
        for (int i = 0; i < numOperands; i++) {
            operands[i] = mem[operandPointer++];
        }
        if (opcode == 1) {
            int cpuCycles = operands[1];
            cpuCyclesUsed += cpuCycles;
            timeSliceCounter += cpuCycles;
            updateClock(totalCpuCycles, cpuCycles, "compute");
            cout << "compute" << endl;
        } else if (opcode == 2) {
            int cpuCycles = operands[0];
            cpuCyclesUsed += cpuCycles;
            timeSliceCounter += cpuCycles;
            relProgramCounter++;
            state = STATE_IO_WAITING;
            ioWaitTime = cpuCycles;
            cout << "Process " << processID 
                 << " issued an IOInterrupt and moved to the IOWaitingQueue." << endl;
            brokeEarly = true;
            break;
        } else if (opcode == 3) {
            int value = operands[0];
            int logicalAddr = operands[1];
            int physicalAddr = relInstructionBase + logicalAddr;
            registerValue = value;
            if (logicalAddr < memoryLimit) {
                mem[physicalAddr] = value;
                cout << "stored" << endl;
            } else {
                cout << "store error!" << endl;
            }
            cpuCyclesUsed++;
            timeSliceCounter++;
            updateClock(totalCpuCycles, 1, "store");
        } else if (opcode == 4) {
            int logicalAddr = operands[0];
            int physicalAddr = relInstructionBase + logicalAddr;
            if (logicalAddr < memoryLimit) {
                registerValue = mem[physicalAddr];
                cout << "loaded" << endl;
            } else {
                cout << "load error!" << endl;
            }
            cpuCyclesUsed++;
            timeSliceCounter++;
            updateClock(totalCpuCycles, 1, "load");
        }
        if (!brokeEarly) {
            relProgramCounter++;
        }
        bool endOfInstructions = ((relInstructionBase + relProgramCounter) >= relDataBase);
        if (!brokeEarly && timeSliceCounter >= globalCPUAllocated && !endOfInstructions) {
            cout << "Process " << processID
                 << " has a TimeOUT interrupt and is moved to the ReadyQueue." << endl;
            brokeEarly = true;
            break;
        }
    }
    bool finishedAll = ((relInstructionBase + relProgramCounter) >= relDataBase);
    if (!brokeEarly && finishedAll) {
        state = STATE_TERMINATED;
        int physicalPC = mainMemoryBase + relProgramCounter;
        int physicalInstructionBase = mainMemoryBase + relInstructionBase;
        int physicalDataBase = mainMemoryBase + relDataBase;
        cout << "Process ID: " << processID << "\n"
             << "State: TERMINATED\n"
             << "Program Counter: " << (physicalInstructionBase - 1) << "\n"
             << "Instruction Base: " << physicalInstructionBase << "\n"
             << "Data Base: " << physicalDataBase << "\n"
             << "Memory Limit: " << memoryLimit << "\n"
             << "CPU Cycles Used: " << cpuCyclesUsed << "\n"
             << "Register Value: " << registerValue << "\n"
             << "Max Memory Needed: " << maxMemoryNeeded << "\n"
             << "Main Memory Base: " << mainMemoryBase << "\n"
             << "Total CPU Cycles Consumed: " << (totalCpuCycles - startTime) << "\n";

        cout << "Process " << processID
             << " terminated. Entered running state at: " << startTime
             << ". Terminated at: " << totalCpuCycles
             << ". Total Execution Time: "
             << (totalCpuCycles - startTime)
             << "." << endl;

        return true;
    }
    return false;
}

// -----------------------------------------------------------------------------
// I/O Handling
// -----------------------------------------------------------------------------
void checkIOQueueSimultaneously(queue<pair<MemBlock*, int>> &ioQueue,
                                queue<MemBlock*> &readyQueue,
                                int &totalCpuCycles) 
{
    int originalSize = (int)ioQueue.size();
    for (int i = 0; i < originalSize; i++) {
        auto ioEntry = ioQueue.front();
        ioQueue.pop();
        MemBlock* block = ioEntry.first;
        int readyTime = ioEntry.second;
        if (totalCpuCycles >= readyTime) {
            block->content[1] = STATE_NEW; // done waiting
            int processID = block->content[0];
            cout << "print" << endl;
            cout << "Process " << processID
                 << " completed I/O and is moved to the ReadyQueue." << endl;
            readyQueue.push(block);
        } else {
            ioQueue.push({block, readyTime});
        }
    }
}

// -----------------------------------------------------------------------------
// Scheduler
// -----------------------------------------------------------------------------
void schedulerLoop(queue<MemBlock*>& readyQueue,
                   queue<pair<MemBlock*, int>>& ioQueue,
                   queue<int>& newJobQueue,
                   vector<Process>& processes,
                   int globalCPUAllocated,
                   int contextSwitchTime,
                   MemBlock*& freeList) 
{
    int totalCpuCycles = 0;
    MemBlock* runningBlock = nullptr;
    bool firstProcessPicked = false;

    // Initial load
    loadWaitingJobs(newJobQueue, processes, freeList, readyQueue);

   
    for (int i = 0; i < (int)fakeMemory.size(); i++) {
        cout << i << " : " << fakeMemory[i] << "\n";
    }
  

    while (!readyQueue.empty() || !ioQueue.empty() ||
           runningBlock != nullptr || !newJobQueue.empty()) 
    {
        checkIOQueueSimultaneously(ioQueue, readyQueue, totalCpuCycles);
        if (!runningBlock) {
            if (readyQueue.empty() && !ioQueue.empty()) {
                contextSwitch(totalCpuCycles, contextSwitchTime, "CPU idle with I/O waiting");
                continue;
            }
            if (!readyQueue.empty()) {
                runningBlock = readyQueue.front();
                readyQueue.pop();
                if (!firstProcessPicked) {
                    contextSwitch(totalCpuCycles, contextSwitchTime, "Initial context switch");
                    firstProcessPicked = true;
                } else {
                    contextSwitch(totalCpuCycles, contextSwitchTime, "New process from ReadyQueue");
                }
                int procID = runningBlock->content[0];
                int theStartTime = 0;
                for (auto &proc : processes) {
                    if (proc.processID == procID) {
                        if (proc.startTime == -1) {
                            proc.startTime = totalCpuCycles;
                        }
                        theStartTime = proc.startTime;
                        break;
                    }
                }
                bool finished = executeProcess(runningBlock, totalCpuCycles, globalCPUAllocated, theStartTime);
                if (!finished) {
                    if (runningBlock->content[1] == STATE_IO_WAITING) {
                        ioQueue.push({runningBlock, totalCpuCycles + ioWaitTime});
                    } else {
                        readyQueue.push(runningBlock);
                    }
                } else {
                    freeMemoryBlock(runningBlock, freeList);
                    loadWaitingJobs(newJobQueue, processes, freeList, readyQueue);
                }
                runningBlock = nullptr;
            }
        }
    }
    contextSwitch(totalCpuCycles, contextSwitchTime, "Final context switch");
    cout << "Total CPU time used: " << totalCpuCycles << ".\n";
}

// -----------------------------------------------------------------------------
// Main
// -----------------------------------------------------------------------------
int main() {
    int maxMemory, numProcesses, globalCPUAllocated, contextSwitchTime;
    vector<Process> processes = parseProcesses(maxMemory, numProcesses, 
                                               globalCPUAllocated, 
                                               contextSwitchTime);

    // Initialize fakeMemory to size maxMemory with -1
    fakeMemory.resize(maxMemory, -1);

    MemBlock* freeList = initDynamicMemory(maxMemory);

    queue<int> newJobQueue;
    for (int i = 0; i < (int)processes.size(); i++) {
        newJobQueue.push(i);
    }

    queue<MemBlock*> readyQueue;
    queue<pair<MemBlock*, int>> ioQueue;

    schedulerLoop(readyQueue, ioQueue, newJobQueue, processes,
                  globalCPUAllocated, contextSwitchTime, freeList);

    return 0;
}
