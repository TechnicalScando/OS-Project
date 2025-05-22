# Simulated Operating System
A simulated operating system with dynamic segmented memory allocation, process scheduling, and logging for educational use.

## Features
- Segmented memory allocation and deallocation
- Dynamic memory coalescing on process termination
- Process lifecycle management (NEW, RUNNING, IO_WAITING, TERMINATED)
- Instruction execution and I/O wait simulation
- CPU clock tracking with context switching
- Logging system for execution and memory status

## Getting Started
### Prerequisites
- C++ compiler (e.g., g++)
- Standard C++ library (C++11 or newer)

### Build Instructions
```
g++ -std=c++11 -o os_sim main.cpp
```

### Run
```
./os_sim
```

### Sample Input
Place your input file (e.g., input.txt) in the project directory and make sure the program reads from it (modify the ifstream in the source if needed).

## File Structure
- `main.cpp` - Core logic for simulation
- `input.txt` - Process and memory instructions
- `README.md` - Project documentation

## Logging
Two log files are created:
- `log.txt` – General execution and process transitions
- `freelist.txt` – Memory allocation and free list status over time

## License
This project is open source and free to use for educational purposes.
