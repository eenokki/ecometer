#define _CRT_SECURE_NO_WARNINGS

#include <windows.h>
#include <iostream>
#include <fstream>
#include <cstdint>
#include <filesystem>

// Note: Ensure Intel PCM include path is configured in your project settings
// Add PCM installation directory to: Project Properties > VC++ Directories > Include Directories
#include "cpucounters.h" // Intel PCM main header

using namespace pcm;
namespace fs = std::filesystem;

// Usage:
//   pcm_measurer.exe <exe> <language> <benchmark>
//   pcm_measurer.exe <exe> <param> <language> <benchmark>
//
// Examples:
//   .\pcm_measurer.exe fannkuchredux.gcc-5.exe C fannkuch-redux
//   .\pcm_measurer.exe fannkuchredux.gcc-5.exe 12 C fannkuch-redux

int main(int argc, char* argv[]) {
    std::string exe, param, language, benchmark, command;

	// Parse command line arguments to avoid failure
    if (argc == 4) {
        // No parameter: <exe> <language> <benchmark>
        exe = argv[1];
        language = argv[2];
        benchmark = argv[3];
        command = exe;
    }
    else if (argc == 5) {
        // With parameter: <exe> <param> <language> <benchmark>
        exe = argv[1];
        param = argv[2];
        language = argv[3];
        benchmark = argv[4];
        command = exe + " " + param;
    }
    else {
        std::cerr << "Usage:\n"
            << "  pcm_measurer.exe <exe> <language> <benchmark>\n"
            << "  pcm_measurer.exe <exe> <param> <language> <benchmark>\n";
        return -1;
    }

    PCM* pcm = PCM::getInstance();
    //Resets the PMU counters
    pcm->resetPMU();

    // Default events: package energy, DRAM counters and PP0 core enabled
    // nullptr: default CPU mapping
    PCM::ErrorCode status = pcm->program(PCM::DEFAULT_EVENTS, nullptr);
    if (status != PCM::Success) {
        std::cerr << "PCM init failed\n";
        return -1;
    }

    // Snapshot energy + TSC before
    SystemCounterState before = getSystemCounterState();

    // Run the benchmark as a subprocess
    STARTUPINFOA si = {};
    si.cb = sizeof(si);  // Required: must set cb before calling CreateProcessA
    PROCESS_INFORMATION pi = {};

    // CreateProcessA requires a mutable char* for the command line
    std::vector<char> cmdBuf(command.begin(), command.end());
    cmdBuf.push_back('\0');

	// Start new process in Windows
    if (!CreateProcessA(NULL, cmdBuf.data(), NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi)) {
        std::cerr << "Failed to start process '" << command
            << "': " << GetLastError() << "\n";
        pcm->cleanup();
        return -1;
    }

	//No end time limit, wait until the process finishes
    WaitForSingleObject(pi.hProcess, INFINITE);

    // Snapshot energy and time in the form of "TSC" after
    SystemCounterState after = getSystemCounterState();

    // Capture exit code before closing handles
    DWORD exitCode = 0;
    GetExitCodeProcess(pi.hProcess, &exitCode);

    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);

    // Compute time from invariant TSC (same hardware source as energy counters)
    uint64_t tsc_ticks = getInvariantTSC(before, after);
    double   avg_freq_hz = getAverageFrequency(before, after); // Turbo-aware, in Hz
    double   wallSeconds = (avg_freq_hz > 0.0)
        ? static_cast<double>(tsc_ticks) / avg_freq_hz
        : 0.0;

    // Compute Joules consumed
    double pkg_joules = getConsumedJoules(before, after);

	// DRAM energy may not be available on all platforms, default to -1.0 if not available
    double dram_joules = -1.0;

    if (pcm->memoryTrafficMetricsAvailable())
    {
        dram_joules =
            getDRAMConsumedJoules(before, after);
    }

    // Warn if the benchmark itself failed
    if (exitCode != 0) {
        std::cerr << "Warning: benchmark exited with code " << exitCode
            << " � results may be invalid\n";
    }

    //.csv file
    std::string filename = "results_" + benchmark + ".csv";
	
    // Check if the file already exists to determine whether to write the header
    bool fileExists = fs::exists(filename);

    std::ofstream csv(filename, std::ios::app);

    if (!fileExists) {
        csv << "Language;Benchmark;Param;PKG_J;DRAM_J;WallTime_s;ExitCode\n";
    }

    csv << language << ";"
        << benchmark << ";"
        << param << ";"
        << pkg_joules << ";"
        << dram_joules << ";"
        << wallSeconds << ";"
        << exitCode << "\n";

    std::cout << "\n=== BENCHMARK DONE ===\n";
    std::cout << "Language:    " << language << "\n";
    std::cout << "Benchmark:   " << benchmark << "\n";
    if (!param.empty())
        std::cout << "Param:       " << param << "\n";
    std::cout << "PKG energy:  " << pkg_joules << " J\n";
    std::cout << "DRAM energy: " << dram_joules << " J\n";
    std::cout << "TSC ticks:   " << tsc_ticks << "\n";
    std::cout << "Avg freq:    " << avg_freq_hz / 1e9 << " GHz\n";
    std::cout << "Wall time:   " << wallSeconds << " s\n";
    std::cout << "Exit code:   " << exitCode << "\n\n";

    // Clean PMU registers
    pcm->cleanup();

    return 0;
}