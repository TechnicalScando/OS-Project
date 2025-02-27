#include <iostream>
#include <vector>
#include <queue>
#include <fstream>

using namespace std;

struct Process
{
    int processID;
    int maxMemoryNeeded;
    int numInstructions;
    vector<int> instructions;
    int startTime = -1; // -1 indicates the process hasn't started running yet
};

const int STATE_NEW = 0;
const int STATE_RUNNING = 1;
const int STATE_IO_WAITING = 2;
const int STATE_TERMINATED = 3;

int ioWaitTime = 0;

/**
 * Update the CPU clock by 'increment' cycles, with a debug reason if desired.
 * (Used for normal instruction execution, not for context switching.)
 */
void updateClock(int &totalCpuCycles, int increment, const string &reason)
{
    totalCpuCycles += increment;
    // Example debug output (comment out if you prefer less verbosity):
    // cout << "[DEBUG] Clock += " << increment
    //      << " | Now: " << totalCpuCycles
    //      << " | Reason: " << reason << endl;
}

/**
 * New function to handle all context switches.
 * Increments the clock by 'contextSwitchTime' and prints a debug statement.
 */
void contextSwitch(int &totalCpuCycles, int contextSwitchTime, const string &reason)
{
    updateClock(totalCpuCycles, contextSwitchTime, reason);

    // Extra debug message:
    // cout << "[CONTEXT SWITCH] Incremented clock by " << contextSwitchTime
    //      << " cycles. Current time is now " << totalCpuCycles
    //      << ". Reason: " << reason << endl;
}

/**
 * Parse process input. Expects to read:
 *  maxMemory, globalCPUAllocated, contextSwitchTime, numProcesses,
 *  plus details for each process.
 */
vector<Process> parseProcesses(int &maxMemory, int &numProcesses,
                               int &globalCPUAllocated, int &contextSwitchTime)
{
    cin >> maxMemory >> globalCPUAllocated >> contextSwitchTime >> numProcesses;
    vector<Process> processes;

    for (int p = 0; p < numProcesses; p++)
    {
        Process proc;
        cin >> proc.processID >> proc.maxMemoryNeeded >> proc.numInstructions;

        proc.instructions.clear();
        int instructionsRead = 0;

        // Read all instructions + operands
        while (instructionsRead < proc.numInstructions)
        {
            int opcode;
            if (!(cin >> opcode))
                break; // read failure
            proc.instructions.push_back(opcode);
            instructionsRead++;

            // Determine how many operands for this opcode
            int numOperands = 0;
            if (opcode == 1)
                numOperands = 2; // compute
            else if (opcode == 2)
                numOperands = 1; // print
            else if (opcode == 3)
                numOperands = 2; // store
            else if (opcode == 4)
                numOperands = 1; // load

            // Read that many operands
            for (int j = 0; j < numOperands; j++)
            {
                int operand;
                if (!(cin >> operand))
                    break; // read failure
                proc.instructions.push_back(operand);
            }
        }
        processes.push_back(proc);
    }
    return processes;
}

/**
 * Allocate memory for each process in mainMemory, storing:
 *   - PCB (10 words reserved)
 *   - instructions
 *   - data area
 */
void allocateMemory(vector<int> &mainMemory, const vector<Process> &processes,
                    int maxMemory, vector<int> &pcbLocations)
{
    int currentAddress = 0;

    for (const Process &proc : processes)
    {
        vector<int> instructionList;
        vector<int> operandList;

        // Separate opcodes vs. operands
        for (size_t i = 0; i < proc.instructions.size(); /* no i++ here */)
        {
            int opcode = proc.instructions[i];
            instructionList.push_back(opcode);

            int numOperands = 0;
            if (opcode == 1)
                numOperands = 2;
            else if (opcode == 2)
                numOperands = 1;
            else if (opcode == 3)
                numOperands = 2;
            else if (opcode == 4)
                numOperands = 1;

            for (int j = 0; j < numOperands; j++)
            {
                if (i + 1 < proc.instructions.size())
                {
                    operandList.push_back(proc.instructions[i + 1]);
                    i++;
                }
            }
            i++;
        }

        int instructionCount = (int)instructionList.size();
        int operandCount = (int)operandList.size();
        int totalProcessMemory = proc.maxMemoryNeeded;

        int remainingDataSpace = totalProcessMemory - instructionCount - operandCount;
        if (remainingDataSpace < 0)
        {
            cout << "Error: Process " << proc.processID
                 << " requires more memory than allocated!\n";
            continue;
        }

        // 10 words for the PCB
        int requiredMemory = 10 + totalProcessMemory;
        if (currentAddress + requiredMemory > maxMemory)
        {
            cout << "Error: Not enough memory for process " << proc.processID
                 << ". Skipping process.\n";
            continue;
        }

        // Record where this PCB is stored
        pcbLocations.push_back(currentAddress);

        // Fill out the PCB in mainMemory
        mainMemory[currentAddress] = proc.processID;                             // ID
        mainMemory[currentAddress + 1] = STATE_RUNNING;                          // We'll treat as 'NEW/RUNNING' for now
        mainMemory[currentAddress + 2] = 0;                                      // PC
        mainMemory[currentAddress + 3] = currentAddress + 10;                    // instruction base
        mainMemory[currentAddress + 4] = currentAddress + 10 + instructionCount; // data base
        mainMemory[currentAddress + 5] = totalProcessMemory;                     // memory limit
        mainMemory[currentAddress + 6] = 0;                                      // CPU cycles used
        mainMemory[currentAddress + 7] = 0;                                      // register value
        mainMemory[currentAddress + 8] = totalProcessMemory;
        mainMemory[currentAddress + 9] = currentAddress; // main memory base

        // Copy instructions
        int instrAddress = currentAddress + 10;
        for (int instr : instructionList)
        {
            mainMemory[instrAddress++] = instr;
        }

        // Copy operands
        int dataAddress = currentAddress + 10 + instructionCount;
        for (int operand : operandList)
        {
            mainMemory[dataAddress++] = operand;
        }

        // Fill remaining data area with -1
        for (int i = 0; i < remainingDataSpace; i++)
        {
            mainMemory[dataAddress + i] = -1;
        }

        currentAddress += requiredMemory;
    }
}

void dumpMemoryToFile(const vector<int> &mainMemory, const string &filename)
{
    ofstream outFile(filename);
    if (!outFile)
    {
        cerr << "Error: Could not open file for writing: " << filename << endl;
        return;
    }

    outFile << "======= FULL MEMORY DUMP =======\n";
    for (size_t i = 0; i < mainMemory.size(); i++)
    {
        outFile << i << ": " << mainMemory[i] << "\n";
    }
    outFile << "================================\n";
    outFile.close();
}

void dumpMemoryToString(const vector<int> &mainMemory)
{
    for (size_t i = 0; i < mainMemory.size(); i++)
    {
        cout << i << ": " << mainMemory[i] << "\n";
    }
}

/**
 * Utility: figure out how many operands have been fetched so far,
 * so we know where the next operand is stored in memory.
 */
int computeOperandPointer(const vector<int> &mainMemory, int pcbLocation, int programCounter)
{
    int instructionBase = mainMemory[pcbLocation + 3];
    int dataBase = mainMemory[pcbLocation + 4];
    int operandCount = 0;

    for (int i = 0; i < programCounter; i++)
    {
        int opcode = mainMemory[instructionBase + i];
        if (opcode == 1)
            operandCount += 2; // compute
        else if (opcode == 2)
            operandCount += 1; // print
        else if (opcode == 3)
            operandCount += 2; // store
        else if (opcode == 4)
            operandCount += 1; // load
    }
    return dataBase + operandCount;
}

/**
 * Execute instructions for a single process, up to the CPU-allocation limit,
 * or until the process blocks on I/O or finishes entirely.
 */
bool executeProcess(vector<int> &mainMemory,
                    int pcbLocation,
                    int &totalCpuCycles,
                    int globalCPUAllocated,
                    int startTime)

{
    int processID = mainMemory[pcbLocation];
    int &state = mainMemory[pcbLocation + 1];
    int &programCounter = mainMemory[pcbLocation + 2];
    int instructionBase = mainMemory[pcbLocation + 3];
    int dataBase = mainMemory[pcbLocation + 4];
    int memoryLimit = mainMemory[pcbLocation + 5];
    int &cpuCyclesUsed = mainMemory[pcbLocation + 6];
    int &registerValue = mainMemory[pcbLocation + 7];
    int maxMemoryNeeded = mainMemory[pcbLocation + 8];
    int mainMemoryBase = mainMemory[pcbLocation + 9];

    cout << "Process " << processID << " has moved to Running." << endl;
    state = STATE_RUNNING; // Mark running

    int timeSliceCounter = 0;
    bool brokeEarly = false;

    int operandPointer = computeOperandPointer(mainMemory, pcbLocation, programCounter);

    while ((instructionBase + programCounter) < dataBase)
    {
        int opcode = mainMemory[instructionBase + programCounter];

        int numOperands = 0;
        if (opcode == 1)
            numOperands = 2; // compute
        else if (opcode == 2)
            numOperands = 1; // print
        else if (opcode == 3)
            numOperands = 2; // store
        else if (opcode == 4)
            numOperands = 1; // load

        // Fetch
        vector<int> operands;
        for (int i = 0; i < numOperands; i++)
        {
            operands.push_back(mainMemory[operandPointer]);
            operandPointer++;
        }

        // Execute
        if (opcode == 1) // compute
        {
            int cpuCycles = operands[1]; // e.g. compute X Y
            cpuCyclesUsed += cpuCycles;
            timeSliceCounter += cpuCycles;
            updateClock(totalCpuCycles, cpuCycles, "Executing compute instruction");
            cout << "compute" << endl;
        }
        else if (opcode == 2) // print => triggers I/O
        {
            int cpuCycles = operands[0];
            cpuCyclesUsed += cpuCycles;
            timeSliceCounter += cpuCycles;

            programCounter++;
            state = STATE_IO_WAITING;
            ioWaitTime = cpuCycles;
            cout << "Process " << processID
                 << " issued an IOInterrupt and moved to the IOWaitingQueue." << endl;
            brokeEarly = true;
            break;
        }
        else if (opcode == 3) // store
        {
            int value = operands[0];
            int logicalAddr = operands[1];
            int physicalAddr = instructionBase + logicalAddr;

            registerValue = value;
            if (logicalAddr < memoryLimit)
            {
                mainMemory[physicalAddr] = value;
                cout << "stored" << endl;
            }
            else
            {
                cout << "store error!" << endl;
            }

            cpuCyclesUsed++;
            timeSliceCounter++;
            updateClock(totalCpuCycles, 1, "Executing store instruction");
        }
        else if (opcode == 4) // load
        {
            int logicalAddr = operands[0];
            int physicalAddr = instructionBase + logicalAddr;

            if (logicalAddr < memoryLimit)
            {
                registerValue = mainMemory[physicalAddr];
                cout << "loaded" << endl;
            }
            else
            {
                cout << "load error!" << endl;
            }

            cpuCyclesUsed++;
            timeSliceCounter++;
            updateClock(totalCpuCycles, 1, "Executing load instruction");
        }
        else
        {
            // unknown opcode
        }

        if (!brokeEarly)
        {
            programCounter++;
        }

        if (!brokeEarly && timeSliceCounter >= globalCPUAllocated &&
            (instructionBase + programCounter) < dataBase)
        {
            cout << "Process " << processID
                 << " has a TimeOUT interrupt and is moved to the ReadyQueue." << endl;
            brokeEarly = true;
            break;
        }
    }

    bool allInstructionsUsed = ((instructionBase + programCounter) >= dataBase);
    if (!brokeEarly && allInstructionsUsed)
    {
        // Process is done
        state = STATE_TERMINATED;
        cout << "Process ID: " << processID << "\n"
             << "State: TERMINATED\n"
             << "Program Counter: " << (instructionBase - 1) << "\n"
             << "Instruction Base: " << instructionBase << "\n"
             << "Data Base: " << dataBase << "\n"
             << "Memory Limit: " << memoryLimit << "\n"
             << "CPU Cycles Used: " << cpuCyclesUsed << "\n"
             << "Register Value: " << registerValue << "\n"
             << "Max Memory Needed: " << maxMemoryNeeded << "\n"
             << "Main Memory Base: " << mainMemoryBase << "\n"
             << "Total CPU Cycles Consumed: " << (totalCpuCycles - startTime) << "\n";

        return true;
    }
    else
    {
        return false;
    }
}

/**
 * Scan *all* items in the I/O queue, popping each one to see if it's finished.
 * If totalCpuCycles >= item’s readyTime, it goes to ReadyQueue; otherwise, re-queue it.
 */
void checkIOQueueSimultaneously(queue<pair<int, int>> &ioQueue,
                                queue<int> &readyQueue,
                                vector<int> &mainMemory,
                                int &totalCpuCycles)
{
    int originalSize = ioQueue.size();
    for (int i = 0; i < originalSize; i++)
    {
        auto ioEntry = ioQueue.front();
        ioQueue.pop();

        int pcbAddr   = ioEntry.first;
        int readyTime = ioEntry.second;

        // If this process's I/O is done
        if (totalCpuCycles >= readyTime)
        {
            // Mark it as ready again
            mainMemory[pcbAddr + 1] = STATE_NEW;

            // Look up the process ID (assuming mainMemory[pcbAddr] is the ID)
            int processID = mainMemory[pcbAddr];

            // Print your message
            cout << "print" << endl;
            cout << "Process " << processID
                 << " completed I/O and is moved to the ReadyQueue." << endl;

            // Now move it to readyQueue
            readyQueue.push(pcbAddr);
        }
        else
        {
            // Still waiting on I/O, push it back
            ioQueue.push(ioEntry);
        }
    }
}


int main()
{
    int maxMemory, numProcesses, globalCPUAllocated, contextSwitchTime;
    // Parse input
    vector<Process> processes = parseProcesses(
        maxMemory, numProcesses,
        globalCPUAllocated, contextSwitchTime);

    // Allocate memory
    vector<int> mainMemory(maxMemory, -1);
    vector<int> pcbLocations;
    allocateMemory(mainMemory, processes, maxMemory, pcbLocations);

    // Dump initial memory
    dumpMemoryToString(mainMemory);
    dumpMemoryToFile(mainMemory, "memory");

    // Create readyQueue, fill with all allocated processes
    queue<int> readyQueue;
    for (int pcbAddr : pcbLocations)
    {
        readyQueue.push(pcbAddr);
    }

    // I/O queue: stores <pcbAddress, readyTime>
    queue<pair<int, int>> ioQueue;

    int totalCpuCycles = 0;
    int runningProcess = -1; // No process running initially

    bool firstProcessPicked = false;

    // Main scheduler loop
    while (!readyQueue.empty() || !ioQueue.empty() || runningProcess != -1)
    {
        // 1) Check for I/O completions
        checkIOQueueSimultaneously(ioQueue, readyQueue, mainMemory, totalCpuCycles);

        // 2) If no running process, try to pick one
        if (runningProcess == -1)
        {
            // If CPU is idle but we have processes in I/O:
            if (readyQueue.empty() && !ioQueue.empty())
            {
                contextSwitch(totalCpuCycles, contextSwitchTime,
                              "CPU idle with I/O waiting");
                continue;
            }

            // Otherwise, pop from readyQueue if not empty
            if (!readyQueue.empty())
            {
                runningProcess = readyQueue.front();
                readyQueue.pop();

                // Check if this is the very first process we pick
                if (!firstProcessPicked)
                {
                    contextSwitch(totalCpuCycles, contextSwitchTime,
                                  "Initial context switch to first process");
                    firstProcessPicked = true;
                }
                else
                {
                    // Normal context switch
                    contextSwitch(totalCpuCycles, contextSwitchTime,
                                  "New process from ReadyQueue");
                }

                // Mark startTime if needed
                int pcbIndex = -1;
                for (int i = 0; i < (int)pcbLocations.size(); i++)
                {
                    if (pcbLocations[i] == runningProcess)
                    {
                        pcbIndex = i;
                        break;
                    }
                }
                // If this is truly the first time the process is dispatched, set startTime
                if (pcbIndex != -1 && processes[pcbIndex].startTime == -1)
                {
                    processes[pcbIndex].startTime = totalCpuCycles;
                }
            }
        }

        // 3) If we have a running process, execute it
        if (runningProcess != -1)
        {
            // Find the index again so we can pass the correct startTime
            int pcbIndex = -1;
            for (int i = 0; i < (int)pcbLocations.size(); i++)
            {
                if (pcbLocations[i] == runningProcess)
                {
                    pcbIndex = i;
                    break;
                }
            }

            // Pass the process's startTime as the new fifth parameter
            bool finished = executeProcess(
                mainMemory,
                runningProcess,
                totalCpuCycles,
                globalCPUAllocated,
                processes[pcbIndex].startTime // <--- pass startTime here
            );

            // If the process hasn't finished...
            if (!finished)
            {
                // If it's waiting on I/O:
                if (mainMemory[runningProcess + 1] == STATE_IO_WAITING)
                {
                    // Use the global or local ioWaitTime you set in executeProcess
                    ioQueue.push({ runningProcess, totalCpuCycles + ioWaitTime });
                }
                else
                {
                    // Time-slice ended
                    readyQueue.push(runningProcess);
                }
            }
            else
            {
                // The process terminated
                if (pcbIndex != -1)
                {
                    // Optionally print it again here, or comment this out
                    cout << "Process " << mainMemory[runningProcess]
                         << " terminated. Entered running state at: "
                         << processes[pcbIndex].startTime
                         << ". Terminated at: " << totalCpuCycles
                         << ". Total Execution Time: "
                         << (totalCpuCycles - processes[pcbIndex].startTime)
                         << ".\n";
                }
                // If that was the final process, do one last context switch
                if (readyQueue.empty() && ioQueue.empty())
                {
                    contextSwitch(totalCpuCycles, contextSwitchTime,
                                  "Final context switch after last process finishes");
                }
            }

            // Done with that process for now
            runningProcess = -1;
        }
    }

    cout << "Total CPU time used: " << totalCpuCycles << "." << "\n";
    return 0;
}
