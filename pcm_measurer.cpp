#define _CRT_SECURE_NO_WARNINGS
#include <windows.h>
#include <iostream>
#include <fstream>
#include "cpucounters.h" // Intel PCM main header

using namespace pcm;

int main(int argc, char* argv[]) {
    // argv[1] = "command to run", argv[2] = Language, argv[3] = benchmark name

    PCM* pcm = PCM::getInstance();
    pcm->resetPMU();

	//default events: package energy, DRAM counters and PP0 core enabled
    //nullptr: default CPU mapping
    PCM::ErrorCode status = pcm->program(PCM::DEFAULT_EVENTS, nullptr);
    if (status != PCM::Success) {
        std::cerr << "PCM init failed\n";
        return -1;
    }

    //pcm->program();  // initialise PCM, installs driver on first run (needs Admin)

    // Snapshot energy before
    SystemCounterState before = getSystemCounterState();

    // Run the benchmark as a subprocess
    STARTUPINFOA si = {};
    PROCESS_INFORMATION pi = {};
    CreateProcessA(NULL, argv[1], NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi);
    WaitForSingleObject(pi.hProcess, INFINITE);

    FILETIME ct, et, kt, ut;
    GetProcessTimes(pi.hProcess, &ct, &et, &kt, &ut);
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);

    // Snapshot energy after
    SystemCounterState after = getSystemCounterState();

    // Compute Joules consumed
    double pkg_joules = getConsumedJoules(before, after);
    double dram_joules = getDRAMConsumedJoules(before, after);

    // Write CSV: language ; benchmark ; PKG (J) ; DRAM (J)
    //std::ofstream csv(std::string(argv[2]) + ".csv", std::ios::app);
    std::ofstream csv("results.csv", std::ios::app);
    csv << argv[2] << " ; " << argv[3] << " ; "
        << pkg_joules << " ; " << dram_joules << "\n";


    //csv << argv[3] << " ; " << pkg_joules << " ; " << dram_joules << "\n";
    
    
    std::cout << "\n=== BENCHMARK DONE ===\n";
    std::cout << "Language: " << argv[2] << "\n";
    std::cout << "Benchmark: " << argv[3] << "\n";
    std::cout << "Energy: " << pkg_joules << " J\n\n";

    //clean PMU registers
    pcm->cleanup();

    return 0;
}