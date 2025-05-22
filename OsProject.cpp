#include <iostream>
#include <vector>
#include <queue>
#include <fstream>
#include <sstream>
#include <algorithm>
using namespace std;

ostringstream logBuffer;
ostringstream freeListLog;

// -----------------------------------------------------------------------------
// Process Structure and Constants
// -----------------------------------------------------------------------------

struct MemBlock
{
    int processID;
    int start;
    int size;
    vector<int> content;
    MemBlock *next;
};

struct Process
{
    int processID;
    int maxMemoryNeeded;
    int numInstructions;
    vector<int> instructions;
    int startTime = -1;
    // NEW: save segmented memory blocks so we can free them on termination.
    vector<MemBlock *> segmentedBlocks;
};

const int STATE_NEW = 0;
const int STATE_RUNNING = 1;
const int STATE_IO_WAITING = 2;
const int STATE_TERMINATED = 3;

int ioWaitTime = 0;

// -----------------------------------------------------------------------------
// CPU Clock and Context Switching
// -----------------------------------------------------------------------------
void updateClock(int &totalCpuCycles, int increment, const string &reason)
{
    totalCpuCycles += increment;
}

void contextSwitch(int &totalCpuCycles, int contextSwitchTime, const string &reason)
{
    updateClock(totalCpuCycles, contextSwitchTime, reason);
}

// Debug: Print the free list with each block's start and size.
void printFreeList(MemBlock *list, const string &label, const string &funclabel)
{
    cout << "----- " << label << " -----" << endl;
    cout << "Calling function: " << endl;
    if (!list)
    {
        cout << "Free list is empty." << endl;
    }
    else
    {
        MemBlock *curr = list;
        while (curr)
        {
            cout << "Block Start: " << curr->start << ", Size: " << curr->size << endl;
            curr = curr->next;
        }
    }
    cout << "---------------------" << endl;
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

    for (int p = 0; p < numProcesses; p++)
    {
        Process proc;
        cin >> proc.processID >> proc.maxMemoryNeeded >> proc.numInstructions;
        proc.instructions.clear();
        proc.instructions.reserve(proc.numInstructions * 3);

        int instructionsRead = 0;
        while (instructionsRead < proc.numInstructions)
        {
            int opcode;
            if (!(cin >> opcode))
                break;
            proc.instructions.push_back(opcode);
            instructionsRead++;

            int numOperands = 0;
            switch (opcode)
            {
            case 1:
                numOperands = 2;
                break; // compute
            case 2:
                numOperands = 1;
                break; // print
            case 3:
                numOperands = 2;
                break; // store
            case 4:
                numOperands = 1;
                break; // load
            default:
                break;
            }
            for (int j = 0; j < numOperands; j++)
            {
                int operand;
                if (!(cin >> operand))
                    break;
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

MemBlock *initDynamicMemory(int maxMemory)
{
    MemBlock *head = new MemBlock();
    head->processID = -1;
    head->start = 0;
    head->size = maxMemory;
    head->next = nullptr;
    return head;
}

// Insert a free block in ascending 'start'
void insertFreeBlock(MemBlock *block, MemBlock *&logicalList)
{
    if (!logicalList || block->start < logicalList->start)
    {
        block->next = logicalList;
        logicalList = block;
        return;
    }
    MemBlock *curr = logicalList;
    while (curr->next && curr->next->start < block->start)
    {
        curr = curr->next;
    }
    block->next = curr->next;
    curr->next = block;
}

// Coalesce adjacent free blocks
void coalesceFreeList(MemBlock *&logicalList)
{

    if (!logicalList)
        return;
    MemBlock *curr = logicalList;

    while (curr && curr->next)
    {
        if (curr->start + curr->size == curr->next->start)
        {
            MemBlock *temp = curr->next;
            curr->size += temp->size;
            curr->next = temp->next;
            delete temp;
        }
        else
        {
            curr = curr->next;
        }
    }
}

// Freed memory => insert inlogical WITHOUT coalescing
void freeMemoryBlock(MemBlock *block, MemBlock *&logicalList)
{
    int startAddr = block->start;
    int endAddr = block->start + block->size - 1;
    int procID = block->processID;
    block->processID = -1;
    insertFreeBlock(block, logicalList);
}

// -----------------------------------------------------------------------------
// Global fakeMemory array for printing physical addresses
// -----------------------------------------------------------------------------
static vector<int> physicalMemory; // This will be resized to maxMemory in main()

// -----------------------------------------------------------------------------
// Allocation and Loading
// -----------------------------------------------------------------------------

// Helper function to print the new job queue.
void printNewJobQueue(queue<int> newJobQueue, const vector<Process> &processes)
{
    cout << "----- New Job Queue Contents -----" << endl;
    if (newJobQueue.empty())
    {
        cout << "New Job Queue is empty." << endl;
    }
    while (!newJobQueue.empty())
    {
        int idx = newJobQueue.front();
        newJobQueue.pop();
        // Print the process index and its process ID (or any other info you want)
        cout << "Queue Entry - Index: " << idx
             << ", Process ID: " << processes[idx].processID << endl;
    }
    cout << "----------------------------------" << endl;
}

MemBlock *allocateMemoryForJob(MemBlock *&logicalList, Process &job)
{
    int requiredSize = 10 + job.maxMemoryNeeded;
    MemBlock *current = logicalList;
    MemBlock *prev = nullptr;

    while (current)
    {
        if (current->processID == -1 && current->size >= requiredSize)
        {
            if (current->size == requiredSize)
            {
                if (!prev)
                {
                    logicalList = current->next;
                }
                else
                {
                    prev->next = current->next;
                }
                current->next = nullptr;
                current->processID = job.processID;
                current->content.resize(requiredSize, 0);
                return current;
            }
            else
            {
                MemBlock *allocated = new MemBlock();
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

bool loadJobIntoBlock(const Process &proc, MemBlock *block)
{
    vector<int> instrList;
    vector<int> operandList;

    for (size_t i = 0; i < proc.instructions.size();)
    {
        int opcode = proc.instructions[i];
        instrList.push_back(opcode);
        int numOperands = 0;
        switch (opcode)
        {
        case 1:
            numOperands = 2;
            break;
        case 2:
            numOperands = 1;
            break;
        case 3:
            numOperands = 2;
            break;
        case 4:
            numOperands = 1;
            break;
        default:
            break;
        }
        for (int j = 0; j < numOperands; j++)
        {
            if ((i + 1) < proc.instructions.size())
            {
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

    if (remainData < 0)
    {
        cout << "Error: Process " << proc.processID
             << " requires more memory than allocated!" << endl;
        return false;
    }

    // Setup PCB in block->content (all stored as relative values)
    block->content[0] = proc.processID;        // ID
    block->content[1] = STATE_RUNNING;         // state
    block->content[2] = 0;                     // relative PC
    block->content[3] = 10;                    // relative instruction base
    block->content[4] = 10 + instructionCount; // relative data base
    block->content[5] = totalMem;              // memory limit
    block->content[6] = 0;                     // CPU cycles used
    block->content[7] = 0;                     // register
    block->content[8] = totalMem;              // duplicate memory limit
    block->content[9] = block->start;          // physical base

    // Copy instructions and operands into block->content
    int instrIndex = 10;
    for (int instr : instrList)
    {
        if (instrIndex < (int)block->content.size())
            block->content[instrIndex++] = instr;
    }
    int dataIndex = 10 + instructionCount;
    for (int op : operandList)
    {
        if (dataIndex < (int)block->content.size())
            block->content[dataIndex++] = op;
    }
    for (int i = 0; i < remainData; i++)
    {
        if (dataIndex + i < (int)block->content.size())
            block->content[dataIndex + i] = -1;
    }

    return true;
}

// -----------------------------------------------------------------------------
// loadWaitingJobs: Coalesce only if a job fails to fit on first try
// -----------------------------------------------------------------------------
void printAllocatedSegments(const vector<MemBlock *> &segments, int processID)
{
    logBuffer << "Process " << processID << " allocated segments:" << endl;
    int totalSize = 0; // Accumulate total size
    for (size_t i = 0; i < segments.size(); i++)
    {
        int start = segments[i]->start;
        int size = segments[i]->size;
        int end = start + size - 1;
        totalSize += size;
        logBuffer << "  Segment " << i << ": Start = " << start
                  << ", End = " << end
                  << ", Size = " << size << endl;
    }
    // Append the total size at the end.
    logBuffer << "Total size allocated: " << totalSize << endl;
}

// A function to output all accumulated logs at the end.
void flushLogs()
{
    cout << logBuffer.str();
    // cout << freeListLog.str();
}

// A helper function to capture the current free list.
void captureFreeList(MemBlock *freeList, string type)
{

    freeListLog << type;
    for (MemBlock *current = freeList; current != nullptr; current = current->next)
    {

        freeListLog << "[Start:" << current->start << ", Size:" << current->size
                    << ", End:" << current->start + current->size - 1 << "] -> " << "\n";
    }

    freeListLog << "\n";
}

// Define error codes for clarity:
#define ALLOC_ERROR_NONE 0
#define ALLOC_ERROR_NO_SEGMENT_BLOCK 1
#define ALLOC_ERROR_INSUFFICIENT_FREE_MEMORY 2

vector<MemBlock *> allocateProcessSegments(MemBlock *&segmentedMemory, Process &job, int &errorCode)
{
    vector<MemBlock *> segments;
    errorCode = ALLOC_ERROR_NONE; // assume success to start

    // Calculate the total number of cells needed:
    // 10 for PCB fields, 13 for a minimum segment table reservation,
    // plus job.maxMemoryNeeded cells for the process data.
    int requiredTotal = 10 + 13 + job.maxMemoryNeeded;
    int allocatedTotal = 0;

    captureFreeList(segmentedMemory, "before");
    coalesceFreeList(segmentedMemory);

    // ***** Preliminary Check: Separate Conditions *****
    int totalFree = 0;
    bool foundBlockOf13 = false;
    for (MemBlock *iter = segmentedMemory; iter != nullptr; iter = iter->next)
    {
        if (iter->processID == -1)
        {
            totalFree += iter->size;
            if (iter->size >= 13)
                foundBlockOf13 = true;
        }
    }
    // First check: Is there at least one free block of size >= 13?
    if (!foundBlockOf13)
    {
        errorCode = ALLOC_ERROR_NO_SEGMENT_BLOCK;
        return segments;
    }
    // Second check: Is the total free memory available enough for the required total?
    if (totalFree < requiredTotal)
    {
        errorCode = ALLOC_ERROR_INSUFFICIENT_FREE_MEMORY;
        return segments;
    }
    // ***** End Preliminary Checks *****

    // STEP 1: Find the first free block that is large enough (>= 13 cells) for the segment table.
    MemBlock *prev = nullptr, *current = segmentedMemory;
    while (current != nullptr)
    {
        if (current->processID == -1 && current->size >= 13)
            break;
        prev = current;
        current = current->next;
    }
    if (current == nullptr)
    {
        // This should not occur because the preliminary check passed.
        errorCode = ALLOC_ERROR_NO_SEGMENT_BLOCK;
        return segments;
    }

    // Record the boundary address. Only blocks with start >= this will be used
    // for additional allocations.
    int boundary = current->start;

    // STEP 2: Allocate from the found block.
    int needed = requiredTotal - allocatedTotal;
    // Ensure the first allocated segment gets at least 13 cells.
    if (segments.empty() && needed < 13)
        needed = 13;

    if (current->size > needed)
    {
        // Allocate 'needed' cells from this block by splitting.
        MemBlock *allocatedBlock = new MemBlock();
        allocatedBlock->processID = job.processID;
        allocatedBlock->start = current->start;
        allocatedBlock->size = needed;
        allocatedBlock->content.resize(needed, -1);
        allocatedBlock->next = nullptr;
        segments.push_back(allocatedBlock);
        allocatedTotal += needed;

        // Adjust the free block.
        current->start += needed;
        current->size -= needed;
    }
    else
    {
        // current->size exactly equals needed: remove block from free list.
        if (prev == nullptr)
            segmentedMemory = current->next;
        else
            prev->next = current->next;
        current->next = nullptr;
        current->processID = job.processID;
        segments.push_back(current);
        allocatedTotal += current->size;
    }

    // STEP 3: Gather additional free blocks (only considering blocks that come after our boundary)
    // until allocatedTotal >= requiredTotal, but do not allow more than 6 segments per process.
    // Here we iterate over the free list and skip any block whose 'start' is less than boundary.
    MemBlock *prev2 = nullptr;
    MemBlock *p = segmentedMemory;
    while (allocatedTotal < requiredTotal && p != nullptr && segments.size() < 6)
    {
        if (p->start < boundary)
        {
            // Skip blocks that come before the boundary.
            prev2 = p;
            p = p->next;
            continue;
        }

        // Remove p from the free list.
        MemBlock *block = p;
        p = p->next;
        if (prev2 == nullptr)
        {
            segmentedMemory = p;
        }
        else
        {
            prev2->next = p;
        }
        block->next = nullptr;

        // Only consider free blocks.
        if (block->processID != -1)
        {
            continue;
        }

        int stillNeeded = requiredTotal - allocatedTotal;

        // CASE 1: block size exactly matches the remaining needed.
        if (block->size == stillNeeded)
        {
            block->processID = job.processID;
            segments.push_back(block);
            allocatedTotal += block->size;
        }
        // CASE 2: block size is larger than needed => split it.
        else if (block->size > stillNeeded)
        {
            MemBlock *allocatedBlock = new MemBlock();
            allocatedBlock->processID = job.processID;
            allocatedBlock->start = block->start;
            allocatedBlock->size = stillNeeded;
            allocatedBlock->content.resize(stillNeeded, -1);
            allocatedBlock->next = nullptr;
            segments.push_back(allocatedBlock);
            allocatedTotal += stillNeeded;

            // Adjust the free block to reflect the split.
            block->start += stillNeeded;
            block->size -= stillNeeded;
            // Mark it free again.
            block->processID = -1;
            // Reinsert the leftover block into segmentedMemory (keeping it sorted by start).
            insertFreeBlock(block, segmentedMemory);
        }
        // CASE 3: block is smaller than needed => take the whole block and continue.
        else
        {
            block->processID = job.processID;
            segments.push_back(block);
            allocatedTotal += block->size;
        }
    }

    // STEP 4: If we have reached the maximum segment count and still haven't allocated enough memory,
    // the allocation fails.
    if (allocatedTotal < requiredTotal)
    {
        // Roll back: Mark all allocated blocks as free and reinsert them.
        for (MemBlock *blk : segments)
        {
            blk->processID = -1; // Mark block free.
            insertFreeBlock(blk, segmentedMemory);
        }
        segments.clear(); // Indicate failure.
        errorCode = ALLOC_ERROR_INSUFFICIENT_FREE_MEMORY;
    }

    return segments;
}

bool loadJobIntoSegments(const Process &proc, vector<MemBlock *> &segments)
{
    // Step 0: Ensure each segment's content array is resized properly.
    for (size_t i = 0; i < segments.size(); i++)
    {
        segments[i]->content.resize(segments[i]->size, -1);
    }

    // Step 1: Separate the instructions and operands from the process.
    vector<int> instrList;
    vector<int> operandList;
    for (size_t i = 0; i < proc.instructions.size();)
    {
        int opcode = proc.instructions[i];
        instrList.push_back(opcode);
        int numOperands = 0;
        switch (opcode)
        {
        case 1:
            numOperands = 2;
            break; // compute
        case 2:
            numOperands = 1;
            break; // print
        case 3:
            numOperands = 2;
            break; // store
        case 4:
            numOperands = 1;
            break; // load
        default:
            break;
        }
        for (int j = 0; j < numOperands; j++)
        {
            if ((i + 1) < proc.instructions.size())
            {
                operandList.push_back(proc.instructions[i + 1]);
                i++; // advance for each operand
            }
        }
        i++; // advance to next instruction/opcode
    }

    int instructionCount = (int)instrList.size();
    int operandCount = (int)operandList.size();

    // Step 2: Build a contiguous logical image similar to loadJobIntoBlock.
    // In the old code, the first 10 cells were for the PCB, so let’s assume:
    //   - We reserve a segment table at the beginning.
    // Let the segment table size be 2*number_of_segments (each segment: start and size)
    int numSegments = segments.size();
    int segTableSize = numSegments * 2; // two entries per segment
    int PCBFields = 10;                 // fixed 10 PCB fields
    int totalLogicalSize = segTableSize + 1 + PCBFields + instructionCount + operandCount;

    // Create a temporary contiguous array for the process image.
    vector<int> logicalMemory(totalLogicalSize, -1);

    // Write the segment table at the beginning:
    // At index 0, we store segTableSize. Then store each segment's physical start and size.
    logicalMemory[0] = segTableSize;
    for (int i = 0; i < numSegments; i++)
    {
        logicalMemory[1 + 2 * i] = segments[i]->start;
        logicalMemory[2 + 2 * i] = segments[i]->size;
    }

    // Write the PCB fields.
    // PCB fields start at index = segTableSize + 1.
    int offset = segTableSize + 1;
    logicalMemory[offset + 0] = proc.processID; // Process ID
    logicalMemory[offset + 1] = STATE_RUNNING;  // State
    logicalMemory[offset + 2] = 0;              // Relative PC
    // The following two fields are normally set to 10 in the old code, so we adjust them.
    logicalMemory[offset + 3] = 10 + offset;                    // Instruction Base (adjust by offset)
    logicalMemory[offset + 4] = 10 + offset + instructionCount; // Data Base (after instructions)
    logicalMemory[offset + 5] = proc.maxMemoryNeeded;           // Memory limit
    logicalMemory[offset + 6] = 0;                              // CPU Cycles Used
    logicalMemory[offset + 7] = 0;                              // Register Value
    logicalMemory[offset + 8] = proc.maxMemoryNeeded;           // Duplicate Memory Limit
    // Main Memory Base: we use the start of the first segment.
    logicalMemory[offset + 9] = segments[0]->start;

    // Write the instructions and operands immediately following the PCB fields.
    int dataStart = offset + PCBFields;
    int index = dataStart;
    for (int instr : instrList)
    {
        if (index < totalLogicalSize)
            logicalMemory[index++] = instr;
    }
    for (int op : operandList)
    {
        if (index < totalLogicalSize)
            logicalMemory[index++] = op;
    }

    // Step 3: Distribute the contiguous logicalMemory into the allocated segments.
    // This is similar to our old distribution loop but iterating over segments.
    int logicalIndex = 0;
    for (size_t seg = 0; seg < segments.size() && logicalIndex < totalLogicalSize; seg++)
    {
        for (int j = 0; j < segments[seg]->size && logicalIndex < totalLogicalSize; j++)
        {
            segments[seg]->content[j] = logicalMemory[logicalIndex++];
        }
    }

    // Step 4: Update PhysicalMemory for every segment.
    for (size_t seg = 0; seg < segments.size(); seg++)
    {
        for (int j = 0; j < segments[seg]->size; j++)
        {
            physicalMemory[segments[seg]->start + j] = segments[seg]->content[j];
        }
    }

    return (logicalIndex == totalLogicalSize);
}

// Helper function to print details about allocated segmented blocks.

// Debugging function to print allocated blocks for a process.
void printAllocatedBlocksForProcess(int processID, const vector<MemBlock *> &segments)
{
    cout << "Process " << processID << " allocated blocks:" << endl;
    for (size_t i = 0; i < segments.size(); i++)
    {
        MemBlock *block = segments[i];
        cout << "  Block " << i
             << " -> Start: " << block->start
             << ", Size: " << block->size << endl;
    }
}

void loadWaitingJobs(queue<int> &newJobQueue,
                     vector<Process> &processes,
                     MemBlock *&logicalList,        // contiguous (logical) free list for execution
                     MemBlock *&segmentedMemory,    // free list for segmented allocation
                     queue<MemBlock *> &readyQueue) // readyQueue for execution (contains contiguous block)
{
    // Process jobs one at a time from the newJobQueue.
    while (!newJobQueue.empty())
    {
        int idx = newJobQueue.front(); // Look at the first process (do not pop yet)
        Process &job = processes[idx];

        // cout << "Process " << job.processID
        //      << " - Free segments BEFORE allocation:" << endl;
        // printFreeList(segmentedMemory, "Free segments before load", "watever");

        // Attempt to allocate segmented memory for this process.

        int allocError = ALLOC_ERROR_NONE;
        vector<MemBlock *> segments = allocateProcessSegments(segmentedMemory, job, allocError);

        // If segmented memory allocation fails...
        if (segments.empty())
        {

            if (allocError == ALLOC_ERROR_NO_SEGMENT_BLOCK)
            {
                cout << "Process " << job.processID
                     << " could not be loaded due to insufficient contiguous space for segment table." << endl;
                break; // or continue to the next job based on your scheduling policy.
            }
            else if (allocError == ALLOC_ERROR_INSUFFICIENT_FREE_MEMORY)
            {
                // Print the free list before coalescing.
                cout << "Insufficient memory for Process " << job.processID
                     << ". Attempting memory coalescing." << endl;

                coalesceFreeList(segmentedMemory);

                // Try allocation again.
                segments = allocateProcessSegments(segmentedMemory, job, allocError);
            }

            if (segments.empty())
            {
                cout << "Process " << job.processID
                     << " waiting in NewJobQueue due to insufficient memory." << endl;
                // Since the process at the head cannot be loaded even after coalescing,
                // break out of the loop to stop checking further processes.
                break;
            }
        }

        // If segmented allocation succeeded, load the process image into these segments.
        if (loadJobIntoSegments(job, segments))
        {

            cout << "Process " << job.processID
                 << " loaded with segment table stored at physical address "
                 << segments[0]->start << endl;

            // Save the allocated segmented blocks into the Process object for later freeing.

            job.segmentedBlocks = segments;

            printAllocatedSegments(segments, job.processID);
        }
        else
        {
            cout << "Process " << job.processID
                 << " failed to load into segmented memory." << endl;
            // Remove this process from the newJobQueue and continue.
            newJobQueue.pop();
            continue;
        }

        // Allocate a contiguous block from the logical free list for execution.
        MemBlock *contiguousBlock = allocateMemoryForJob(logicalList, job);
        if (contiguousBlock == nullptr || !loadJobIntoBlock(job, contiguousBlock))
        {
            cout << "Contiguous allocation or load failed for Process " << job.processID << endl;
            // Stop processing further if contiguous allocation fails.
            break;
        }

        // If both segmented and contiguous allocations succeeded, push the process to readyQueue.
        readyQueue.push(contiguousBlock);

        // Finally, remove the process from the newJobQueue (it is now loaded).
        newJobQueue.pop();
    }
}

// -----------------------------------------------------------------------------
// Execution
// -----------------------------------------------------------------------------
int computeOperandPointerFromBlock(const vector<int> &mem,
                                   int instructionBase,
                                   int dataBase,
                                   int programCounter)
{
    int operandCount = 0;
    for (int i = 0; i < programCounter; i++)
    {
        int opcode = mem[instructionBase + i];
        switch (opcode)
        {
        case 1:
            operandCount += 2;
            break;
        case 2:
            operandCount += 1;
            break;
        case 3:
            operandCount += 2;
            break;
        case 4:
            operandCount += 1;
            break;
        default:
            break;
        }
    }
    return dataBase + operandCount;
}

// Translates a logical address to a physical address using the segment table
// stored in the first allocated block from the process's segmented blocks vector.
//
// Parameters:
//   logicalAddress - the address generated by the CPU (logical address)
//   segBlocks      - the vector of allocated segments for the process; the
//                    first element contains the segment table.
// Returns the corresponding physical address or -1 if the address is invalid.
int translateLogicalToPhysical(int logicalAddress, const vector<MemBlock *> &segBlocks)
{
    if (segBlocks.empty() || segBlocks[0]->content.empty())
    {
        cout << "Error: No segment table found." << endl;
        return -1;
    }

    // The segment table is stored in the first block.
    int segTableSize = segBlocks[0]->content[0];
    int numSegments = segTableSize / 2;
    int remaining = logicalAddress;

    for (int i = 0; i < numSegments; i++)
    {
        // The table is stored in segBlocks[0]->content:
        //   [1+2*i] is the physical start for segment i.
        //   [2+2*i] is the size (length) of segment i.
        int segmentStart = segBlocks[0]->content[1 + 2 * i];
        int segmentSize = segBlocks[0]->content[1 + 2 * i + 1];

        if (remaining < segmentSize)
        {
            // The logical address falls in this segment.
            int physicalAddress = segmentStart + remaining;
            return physicalAddress;
        }
        else
        {
            remaining -= segmentSize;
        }
    }

    cout << "Memory violation: logical address " << logicalAddress << " out of bounds." << endl;
    return -1; // return error code if address is invalid
}

bool executeProcess(MemBlock *block,
                    int &totalCpuCycles,
                    int globalCPUAllocated,
                    int startTime, const vector<MemBlock *> &segBlocks)
{
    vector<int> &mem = block->content;
    int processID = mem[0];
    int &state = mem[1];
    int &relProgramCounter = mem[2];
    int relInstructionBase = mem[3];
    int relDataBase = mem[4];
    int memoryLimit = mem[5];
    int &cpuCyclesUsed = mem[6];
    int &registerValue = mem[7];
    int maxMemoryNeeded = mem[8];
    int mainMemoryBase = segBlocks.empty() ? mem[9] : segBlocks[0]->start;

    cout << "Process " << processID << " has moved to Running." << endl;
    state = STATE_RUNNING;

    int timeSliceCounter = 0;
    bool brokeEarly = false;

    int operandPointer = computeOperandPointerFromBlock(
        mem, relInstructionBase, relDataBase, relProgramCounter);

    while ((relInstructionBase + relProgramCounter) < relDataBase)
    {
        int opcode = mem[relInstructionBase + relProgramCounter];
        int numOperands = 0;
        switch (opcode)
        {
        case 1:
            numOperands = 2;
            break;
        case 2:
            numOperands = 1;
            break;
        case 3:
            numOperands = 2;
            break;
        case 4:
            numOperands = 1;
            break;
        default:
            break;
        }
        vector<int> operands(numOperands);
        for (int i = 0; i < numOperands; i++)
        {
            operands[i] = mem[operandPointer++];
        }
        if (opcode == 1)
        {
            int cpuCycles = operands[1];
            cpuCyclesUsed += cpuCycles;
            timeSliceCounter += cpuCycles;
            updateClock(totalCpuCycles, cpuCycles, "compute");
            cout << "compute" << endl;
        }
        else if (opcode == 2)
        {
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
        }
        else if (opcode == 3)
        {
            int value = operands[0];
            int logicalAddr = operands[1];
            int physicalAddr = relInstructionBase + logicalAddr;
            int translatedAddress;
            registerValue = value;
            if (logicalAddr < memoryLimit)
            {
                mem[physicalAddr] = value;
                translatedAddress = translateLogicalToPhysical(logicalAddr, segBlocks);

                translatedAddress = translatedAddress;
                cout << "stored" << endl;
                cout << "Logical address " << logicalAddr << " translated to physical address "
                     << translatedAddress << " for Process " << processID << endl;
            }
            else
            {
                cout << "store error!" << endl;
            }
            cpuCyclesUsed++;
            timeSliceCounter++;
            updateClock(totalCpuCycles, 1, "store");
        }
        else if (opcode == 4)
        {
            int logicalAddr = operands[0];
            int physicalAddr = relInstructionBase + logicalAddr;
            int translatedAddress;
            if (logicalAddr < memoryLimit)
            {
                registerValue = mem[physicalAddr];
                translatedAddress = translateLogicalToPhysical(logicalAddr, segBlocks);

                translatedAddress = translatedAddress;
                cout << "loaded" << endl;
                cout << "Logical address " << logicalAddr << " translated to physical address "
                     << translatedAddress << " for Process " << processID << endl;
            }
            else
            {
                cout << "load error!" << endl;
            }
            cpuCyclesUsed++;
            timeSliceCounter++;
            updateClock(totalCpuCycles, 1, "load");
        }
        if (!brokeEarly)
        {
            relProgramCounter++;
        }
        bool endOfInstructions = ((relInstructionBase + relProgramCounter) >= relDataBase);
        if (!brokeEarly && timeSliceCounter >= globalCPUAllocated && !endOfInstructions)
        {
            cout << "Process " << processID
                 << " has a TimeOUT interrupt and is moved to the ReadyQueue." << endl;
            brokeEarly = true;
            break;
        }
    }
    bool finishedAll = ((relInstructionBase + relProgramCounter) >= relDataBase);
    if (!brokeEarly && finishedAll)
    {
        state = STATE_TERMINATED;
        int physicalPC = mainMemoryBase + relProgramCounter;
        int physicalInstructionBase = mainMemoryBase + relInstructionBase;
        int physicalDataBase = mainMemoryBase + relDataBase;
        int segTableSize = segBlocks.empty() ? mem[0] : segBlocks[0]->content[0];
        int bias;

        cout << "Process ID: " << processID << "\n"
             << "State: TERMINATED\n"
             << "Program Counter: " << (segTableSize + 10) << "\n"
             << "Instruction Base: " << segTableSize + 11 << "\n"
             << "Data Base: " << relDataBase + segTableSize + 1 << "\n"
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
void checkIOQueueSimultaneously(queue<pair<MemBlock *, int>> &ioQueue,
                                queue<MemBlock *> &readyQueue,
                                int &totalCpuCycles)
{
    int originalSize = (int)ioQueue.size();
    for (int i = 0; i < originalSize; i++)
    {
        auto ioEntry = ioQueue.front();
        ioQueue.pop();
        MemBlock *block = ioEntry.first;
        int readyTime = ioEntry.second;
        if (totalCpuCycles >= readyTime)
        {
            block->content[1] = STATE_NEW; // done waiting
            int processID = block->content[0];
            cout << "print" << endl;
            cout << "Process " << processID
                 << " completed I/O and is moved to the ReadyQueue." << endl;
            readyQueue.push(block);
        }
        else
        {
            ioQueue.push({block, readyTime});
        }
    }
}

// -----------------------------------------------------------------------------
// Scheduler
// -----------------------------------------------------------------------------

void updatePhysicalMemoryForSegments(const vector<MemBlock *> &segments)
{
    for (size_t s = 0; s < segments.size(); s++)
    {
        for (int i = 0; i < segments[s]->size; i++)
        {
            physicalMemory[segments[s]->start + i] = segments[s]->content[i];
        }
    }
}

void schedulerLoop(queue<MemBlock *> &readyQueue,
                   queue<pair<MemBlock *, int>> &ioQueue,
                   queue<int> &newJobQueue,
                   vector<Process> &processes,
                   int globalCPUAllocated,
                   int contextSwitchTime,
                   MemBlock *&logicalList,     // contiguous pool for execution
                   MemBlock *&segmentedMemory) // segmented allocation pool
{
    int totalCpuCycles = 0;
    const int MAX_IDLE_ITERATIONS = 1000; // maximum iterations to wait while idle
    int idleIterationCount = 0;
    MemBlock *runningBlock = nullptr;
    bool firstProcessPicked = false;

    // Load waiting processes.
    loadWaitingJobs(newJobQueue, processes, logicalList, segmentedMemory, readyQueue);

    for (int i = 0; i < (int)physicalMemory.size(); i++)
    {
        cout << i << " : " << physicalMemory[i] << "\n";
    }

    // Main scheduling loop.
    while (!readyQueue.empty() || !ioQueue.empty() || !newJobQueue.empty())
    {
        // Check if we're idle: no ready or I/O work, but jobs are waiting in NewJobQueue.
        if (readyQueue.empty() && ioQueue.empty() && !newJobQueue.empty())
        {
            // Increment idle iteration count.
            idleIterationCount++;
            // Optionally, simulate time passing by incrementing totalCpuCycles even if idle.
            totalCpuCycles++;
            // If we exceed threshold, exit gracefully.
            if (idleIterationCount >= MAX_IDLE_ITERATIONS)
            {
                cout << "Error: A job has been stuck in the NewJobQueue for too long due to insufficient memory." << endl;
                cout << "Exiting gracefully." << endl;
                return; // Alternatively, call exit(0);
            }
            continue; // Skip the rest of the loop, nothing to do this iteration.
        }
        else
        {
            // Reset idle counter when we have activity.
            idleIterationCount = 0;
        }

        checkIOQueueSimultaneously(ioQueue, readyQueue, totalCpuCycles);
        if (!runningBlock)
        {
            if (readyQueue.empty() && !ioQueue.empty())
            {
                contextSwitch(totalCpuCycles, contextSwitchTime, "CPU idle with I/O waiting");
                continue;
            }
            if (!readyQueue.empty())
            {
                runningBlock = readyQueue.front();
                readyQueue.pop();
                if (!firstProcessPicked)
                {
                    contextSwitch(totalCpuCycles, contextSwitchTime, "Initial context switch");
                    firstProcessPicked = true;
                }
                else
                {
                    contextSwitch(totalCpuCycles, contextSwitchTime, "New process from ReadyQueue");
                }
                int procID = runningBlock->content[0];
                int theStartTime = 0;
                for (auto &proc : processes)
                {
                    if (proc.processID == procID)
                    {
                        if (proc.startTime == -1)
                        {
                            proc.startTime = totalCpuCycles;
                        }
                        theStartTime = proc.startTime;
                        break;
                    }
                }

                vector<MemBlock *> segBlocks;
                for (size_t i = 0; i < processes.size(); i++)
                {
                    if (processes[i].processID == procID)
                    {
                        segBlocks = processes[i].segmentedBlocks; // found the matching process
                        break;
                    }
                }

                bool finished = executeProcess(runningBlock, totalCpuCycles, globalCPUAllocated, theStartTime, segBlocks);
                if (!finished)
                {
                    if (runningBlock->content[1] == STATE_IO_WAITING)
                    {
                        ioQueue.push({runningBlock, totalCpuCycles + ioWaitTime});
                    }
                    else
                    {
                        readyQueue.push(runningBlock);
                    }
                }
                else
                {
                    for (size_t i = 0; i < processes.size(); i++)
                    {
                        if (processes[i].processID == procID)
                        {
                            for (MemBlock *seg : processes[i].segmentedBlocks)
                            {
                                freeMemoryBlock(seg, segmentedMemory);
                            }
                            processes[i].segmentedBlocks.clear();
                            break;
                        }
                    }
                    cout << "Process " << procID << " terminated and freed memory blocks." << endl;

                    loadWaitingJobs(newJobQueue, processes, logicalList, segmentedMemory, readyQueue);
                }
                runningBlock = nullptr;
            }
        }
    }
    contextSwitch(totalCpuCycles, contextSwitchTime, "Final context switch");
    cout << "Total CPU time used: " << totalCpuCycles << ".\n";

    flushLogs();
}

// -----------------------------------------------------------------------------
// Main
// -----------------------------------------------------------------------------
int main()
{
    int maxMemory, numProcesses, globalCPUAllocated, contextSwitchTime;
    vector<Process> processes = parseProcesses(maxMemory, numProcesses,
                                               globalCPUAllocated,
                                               contextSwitchTime);

    // Initialize fakeMemory to size maxMemory with -1
    physicalMemory.resize(maxMemory, -1);

    MemBlock *logicalList = initDynamicMemory(maxMemory + 10000000);
    MemBlock *segmentedMemory = initDynamicMemory(maxMemory);

    queue<int> newJobQueue;
    for (int i = 0; i < (int)processes.size(); i++)
    {
        newJobQueue.push(i);
    }

    queue<MemBlock *> readyQueue;
    queue<pair<MemBlock *, int>> ioQueue;

    schedulerLoop(readyQueue, ioQueue, newJobQueue, processes,
                  globalCPUAllocated, contextSwitchTime, logicalList, segmentedMemory);

    return 0;
}
