#define _CRT_SECURE_NO_WARNINGS

#include <iostream>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <string>
#include <vector>
#include <map>
#include <set>
#include <algorithm>
#include <ctime>
#include <cstdint>
#include <thread>
#include <chrono>
#include <filesystem>

#include <windows.h>
#include <curl/curl.h>
#include <nlohmann/json.hpp>
#include "cpucounters.h"

#ifdef _WIN32
#define timegm _mkgmtime
#endif

using json = nlohmann::json;
using namespace pcm;
namespace fs = std::filesystem;


// EMISSION FACTORS  (gCO2/kWh)
struct EmissionFactors {
    double p188 = 0;    // Nuclear power
    double p191 = 0;    // Hydro power
    double p201 = 221;  // District heating CHP
    double p202 = 123;  // Industrial CHP
    double p205 = 30;   // Reserve / small-scale
    double p245 = 0;    // Wind forecast
    double p248 = 0;    // Solar forecast
    double p31  = 0;    // Transmission FI-SE1
    double p32  = 0;    // Transmission FI-SE3
    double p140 = 0;    // Transmission FI-EE
    double p370 = 0;    // Transmission FI-NO4
} P;


// DATASET MAPPINGS
const std::set<std::string> REALTIME_IDS = {
    "188", "191", "201", "202", "205", "265"
};

std::string shortName(const std::string& id) {
    if (id == "31")  return "fi_se1";
    if (id == "32")  return "fi_se3";
    if (id == "140") return "fi_ee";
    if (id == "370") return "fi_no4";
    if (id == "188") return "nuclear_rt";
    if (id == "191") return "hydro_rt";
    if (id == "201") return "chp_district_rt";
    if (id == "202") return "chp_industrial_rt";
    if (id == "205") return "reserve_rt";
    if (id == "245") return "wind_forecast";
    if (id == "248") return "solar_forecast";
    if (id == "242") return "production_forecast";
    if (id == "265") return "emission_rt";
    return "dataset_" + id;
}

std::string longName(const std::string& id) {
    if (id == "31")  return "Commercial transmission FI-SE1";       // 1h,   MW
    if (id == "32")  return "Commercial transmission FI-SE3";       // 1h,   MW
    if (id == "140") return "Commercial transmission FI-EE";        // 15min,MW
    if (id == "370") return "Commercial transmission FI-NO4";       // 1h,   MW
    if (id == "188") return "Nuclear power - real-time";            // 3min, MW
    if (id == "191") return "Hydro power - real-time";              // 3min, MW
    if (id == "201") return "District heating CHP - real-time";     // 3min, MW
    if (id == "202") return "Industrial CHP - real-time";           // 3min, MW
    if (id == "205") return "Reserve power plants - real-time";     // 3min, MW
    if (id == "245") return "Wind power forecast";                  // 15min,MW
    if (id == "248") return "Solar power forecast";                 // 15min,MW
    if (id == "242") return "Production forecast";                  // 15min,MW
    if (id == "265") return "Emission factor - real-time";          // 3min, gCO2/kWh
    return "dataset_" + id;
}

// TIME UTILITIES
std::string toIso(time_t t) {
    char buf[32];
    std::tm* utc = std::gmtime(&t);
    std::strftime(buf, sizeof(buf), "%Y-%m-%dT%H:%M:%SZ", utc);
    return std::string(buf);
}

std::string toFinnish(time_t t) {
    char buf[32];
    std::tm* loc = std::localtime(&t);
    std::strftime(buf, sizeof(buf), "%d.%m.%Y %H:%M", loc);
    return std::string(buf);
}

time_t parseIso(const std::string& iso) {
    std::tm tm = {};
    std::istringstream ss(iso);
    ss >> std::get_time(&tm, "%Y-%m-%dT%H:%M:%S");
    if (ss.fail()) return -1;
    return timegm(&tm);
}

time_t floorTo15(time_t t)   { return t - (t % 900);  }
time_t floorToHour(time_t t) { return t - (t % 3600); }

// Next 18:00 Helsinki time
time_t next18Helsinki(time_t now) {
    std::tm* loc = std::localtime(&now);
    std::tm target = *loc;
    target.tm_hour  = 18;
    target.tm_min   = 0;
    target.tm_sec   = 0;
    target.tm_isdst = -1;
    time_t t18 = mktime(&target);
    if (t18 <= now) t18 += 86400;
    return t18;
}


//Emission formatting - gCO2 value with 2 decimals and unit
std::string formatEmission(double gCO2) {
    std::ostringstream ss;
    ss << std::fixed;
    double abs = std::abs(gCO2);

    if      (abs >= 1.0)      ss << std::setprecision(2) << gCO2 << " gCO2";
    else if (abs >= 0.001)    ss << std::setprecision(4) << gCO2 << " gCO2";
    else if (abs >= 0.000001) ss << std::setprecision(7) << gCO2 << " gCO2";
    else                      ss << std::setprecision(9) << gCO2 << " gCO2";

    return ss.str();
}

// CURL
static size_t WriteCallback(void* contents, size_t size, size_t nmemb, void* userp) {
    std::string* response = static_cast<std::string*>(userp);
    response->append(static_cast<char*>(contents), size * nmemb);
    return size * nmemb;
}

std::string loadApiKey(const std::string& filename = ".env") {
    std::ifstream file(filename);
    if (!file.is_open()) return "";
    std::string line;
    while (std::getline(file, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (line.rfind("APIKEY=", 0) == 0) return line.substr(7);
    }
    return "";
}

// STAGE 1 FETCH  (Fingrid API to data.json as raw dump)
json fetchDataset(
    CURL* curl,
    const std::string& id,
    const std::string& apiKey,
    const std::string& startTime,
    const std::string& endTime,
    bool isRealtime
) {
    std::string response;
    std::string url =
        "https://data.fingrid.fi/api/datasets/" + id + "/data"
        "?startTime=" + startTime +
        "&endTime="   + endTime +
        (isRealtime ? "&pageSize=10&sortOrder=desc" : "&pageSize=100") +
        "&page=1";

    struct curl_slist* headers = nullptr;
    headers = curl_slist_append(headers, ("x-api-key: " + apiKey).c_str());

    curl_easy_reset(curl);
    curl_easy_setopt(curl, CURLOPT_URL,           url.c_str());
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER,    headers);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA,     &response);

    CURLcode res = curl_easy_perform(curl);
    long httpCode = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &httpCode);
    curl_slist_free_all(headers);

    if (res != CURLE_OK || httpCode != 200) {
        std::cerr << "  [ERROR] " << id
                  << " CURL:" << curl_easy_strerror(res)
                  << " HTTP:" << httpCode << "\n";
        return json{};
    }
    try   { return json::parse(response); }
    catch (...) {
        std::cerr << "  [ERROR] JSON parse failed for " << id << "\n";
        return json{};
    }
}

bool stageFetch(json& allData, const std::string& apiKey) {
    std::cout << " STAGE 1: Fetching Fingrid data\n";

    time_t now      = time(nullptr);
    time_t plus24h  = now + 24 * 3600;
    time_t minus30m = now - 1800;

    std::string forecastStart = toIso(now);
    std::string forecastEnd   = toIso(plus24h);
    std::string realtimeStart = toIso(minus30m);
    std::string realtimeEnd   = toIso(now);

    const std::vector<std::string> datasets = {
        "31", "32", "140", "370",
        "188", "191", "201", "202", "205",
        "245", "248", "242", "265"
    };

    CURL* curl = curl_easy_init();
    if (!curl) { std::cerr << "[FATAL] CURL init failed\n"; return false; }

    allData["fetched"] = toIso(now);
    allData["raw"]     = json::object();

    std::vector<std::string> failed;

    for (size_t i = 0; i < datasets.size(); ++i) {
        const std::string& id = datasets[i];
        bool isRealtime = REALTIME_IDS.count(id) > 0;

        std::cout << "[" << (i+1) << "/" << datasets.size() << "] "
                  << id << ": " << longName(id)
                  << (isRealtime ? " [RT -30min to now]" : " [FORECAST now to 24h]")
                  << "\n";

        json result = fetchDataset(
            curl, id, apiKey,
            isRealtime ? realtimeStart : forecastStart,
            isRealtime ? realtimeEnd   : forecastEnd,
            isRealtime
        );

        if (!result.empty()) {
            allData["raw"][shortName(id)] = result;
            std::cout << id << " done\n";
        } else {
            failed.push_back(id);
            std::cout << id << " FAILED\n";
        }

        if (i + 1 < datasets.size()) {
            std::cout << "  Waiting 3s...\n";
            std::this_thread::sleep_for(std::chrono::seconds(3));
        }
    }

    curl_easy_cleanup(curl);

    std::cout << "\nFetch complete: "
              << (datasets.size() - failed.size())
              << "/" << datasets.size() << " OK\n";

    if (!failed.empty()) {
        std::cout << "Failed:";
        for (auto& f : failed) std::cout << " " << f;
        std::cout << "\n";
    }

    return failed.empty();
}

// STAGE 2 FORECAST  (raw to gCO2/kWh per 15min slot)
struct Point { time_t startTime; double value; };

std::vector<Point> loadFromJson(const json& j) {
    std::vector<Point> pts;
    if (!j.contains("data")) return pts;
    for (auto& item : j["data"]) {
        if (!item.contains("startTime") || !item.contains("value")) continue;
        time_t t = parseIso(item["startTime"].get<std::string>());
        double v = item["value"].get<double>();
        if (t >= 0) pts.push_back({t, v});
    }
    std::sort(pts.begin(), pts.end(),
              [](const Point& a, const Point& b){ return a.startTime < b.startTime; });
    return pts;
}

std::map<time_t, double> hourlyTo15min(const std::vector<Point>& pts, time_t end) {
    std::map<time_t, double> result;
    for (auto& p : pts) {
        time_t h = floorToHour(p.startTime);
        for (int i = 0; i < 4; i++) {
            time_t slot = h + i * 900;
            if (slot < end) result[slot] = p.value;
        }
    }
    return result;
}

std::map<time_t, double> direct15min(const std::vector<Point>& pts) {
    std::map<time_t, double> result;
    for (auto& p : pts) result[floorTo15(p.startTime)] = p.value;
    return result;
}

double latestValue(const std::vector<Point>& pts) {
    return pts.empty() ? 0.0 : pts.back().value;
}

void stageForecast(json& allData) {
    std::cout << " STAGE 2: Calculating emission forecast\n";

    time_t now       = time(nullptr);
    time_t horizon   = next18Helsinki(now);
    time_t slotStart = floorTo15(now);

    std::cout << "Now:       " << toFinnish(now)     << "\n";
    std::cout << "Horizon:   " << toFinnish(horizon)  << " (next 18:00)\n";
    std::cout << "Slots:     " << (horizon - slotStart) / 900 << "\n\n";

    json& raw = allData["raw"];

    auto pts31  = loadFromJson(raw["fi_se1"]);
    auto pts32  = loadFromJson(raw["fi_se3"]);
    auto pts140 = loadFromJson(raw["fi_ee"]);
    auto pts370 = loadFromJson(raw["fi_no4"]);
    auto pts188 = loadFromJson(raw["nuclear_rt"]);
    auto pts191 = loadFromJson(raw["hydro_rt"]);
    auto pts201 = loadFromJson(raw["chp_district_rt"]);
    auto pts202 = loadFromJson(raw["chp_industrial_rt"]);
    auto pts205 = loadFromJson(raw["reserve_rt"]);
    auto pts245 = loadFromJson(raw["wind_forecast"]);
    auto pts248 = loadFromJson(raw["solar_forecast"]);
    auto pts242 = loadFromJson(raw["production_forecast"]);

    auto map31  = hourlyTo15min(pts31,  horizon);
    auto map32  = hourlyTo15min(pts32,  horizon);
    auto map370 = hourlyTo15min(pts370, horizon);
    auto map140 = direct15min(pts140);
    auto map245 = direct15min(pts245);
    auto map248 = direct15min(pts248);
    auto map242 = direct15min(pts242);

    double val188 = latestValue(pts188);
    double val191 = latestValue(pts191);
    double val201 = latestValue(pts201);
    double val202 = latestValue(pts202);
    double val205 = latestValue(pts205);

    std::cout << "Real-time production snapshot:\n";
    std::cout << "  Nuclear:        " << val188 << " MW\n";
    std::cout << "  Hydro:          " << val191 << " MW\n";
    std::cout << "  District CHP:   " << val201 << " MW\n";
    std::cout << "  Industrial CHP: " << val202 << " MW\n";
    std::cout << "  Reserve/other:  " << val205 << " MW\n\n";

    auto getOrZero = [](const std::map<time_t, double>& m, time_t t) -> double {
        auto it = m.find(t);
        return (it != m.end()) ? it->second : 0.0;
    };

    std::set<time_t> allSlots;
    for (auto& [t, v] : map242)
        if (t >= slotStart && t < horizon) allSlots.insert(t);
    for (auto& [t, v] : map245)
        if (t >= slotStart && t < horizon) allSlots.insert(t);

    json forecastOutput;
    forecastOutput["generated"] = toIso(now);
    forecastOutput["horizon"]   = toIso(horizon);
    forecastOutput["unit"]      = "gCO2/kWh";
    forecastOutput["slots"]     = json::array();

    int okCount = 0, skipCount = 0;

    for (time_t slot : allSlots) {
        double C = getOrZero(map242, slot);
        if (C <= 0.0) { skipCount++; continue; }

        double B =
            (val188 * P.p188 + val191 * P.p191 +
             val201 * P.p201 + val202 * P.p202 +
             val205 * P.p205 +
             getOrZero(map245, slot) * P.p245 +
             getOrZero(map248, slot) * P.p248) +
            (getOrZero(map31,  slot) * P.p31  +
             getOrZero(map32,  slot) * P.p32  +
             getOrZero(map140, slot) * P.p140 +
             getOrZero(map370, slot) * P.p370);

        double A = B / C;

        json slot_j;
        slot_j["time"]          = toIso(slot);
        slot_j["timeHelsinki"]  = toFinnish(slot);
        slot_j["A_gCO2_kWh"]   = A;
        slot_j["B_totalCO2"]   = B;
        slot_j["C_forecast_MW"] = C;
        slot_j["debug"] = {
            {"188_nuclear_MW",   val188},
            {"191_hydro_MW",     val191},
            {"201_chp_dh_MW",    val201},
            {"202_chp_ind_MW",   val202},
            {"205_reserve_MW",   val205},
            {"245_wind_MW",      getOrZero(map245, slot)},
            {"248_solar_MW",     getOrZero(map248, slot)},
            {"31_fi_se1_MW",     getOrZero(map31,  slot)},
            {"32_fi_se3_MW",     getOrZero(map32,  slot)},
            {"140_fi_ee_MW",     getOrZero(map140, slot)},
            {"370_fi_no4_MW",    getOrZero(map370, slot)}
        };

        forecastOutput["slots"].push_back(slot_j);
        okCount++;
    }

    allData["forecast"] = forecastOutput;

    std::cout << "Slots calculated: " << okCount   << "\n";
    std::cout << "Skipped (no C):   " << skipCount << "\n";
}

// STAGE 3 PCM BENCHMARK
struct BenchmarkResult {
    std::string program;
    std::string param;
    double      pkg_joules;
    double      dram_joules;
    double      wall_seconds;
    int         exit_code;
};

BenchmarkResult stagePCM(const std::string& exe, const std::string& param) {
    std::cout << " STAGE 3: PCM Energy Measurement\n";
    std::string command = param.empty() ? exe : exe + " " + param;
    std::cout << "Running: " << command << "\n\n";

    PCM* pcm = PCM::getInstance();
    pcm->resetPMU();

    PCM::ErrorCode status = pcm->program(PCM::DEFAULT_EVENTS, nullptr);
    if (status != PCM::Success) {
        std::cerr << "[FATAL] PCM init failed\n";
        return {exe, param, 0, 0, 0, -1};
    }

    SystemCounterState before = getSystemCounterState();

    STARTUPINFOA si = {};
    si.cb = sizeof(si);
    PROCESS_INFORMATION pi = {};
    std::vector<char> cmdBuf(command.begin(), command.end());
    cmdBuf.push_back('\0');

    // Start subprocess and begin wall time measurement
    auto wallStart = std::chrono::steady_clock::now();

    if (!CreateProcessA(NULL, cmdBuf.data(), NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi)) {
        std::cerr << "[FATAL] Failed to start: " << command
                  << " (error " << GetLastError() << ")\n";
        pcm->cleanup();
        return {exe, param, 0, 0, 0, -1};
    }

    // Wait for subprocess to finish
    WaitForSingleObject(pi.hProcess, INFINITE);
    SystemCounterState after = getSystemCounterState();

    // Measure elapsed wall time with steady_clock
    double wallSeconds = std::chrono::duration<double>(
        std::chrono::steady_clock::now() - wallStart).count();

    // Average CPU frequency and load estimation based on TSC and nominal frequency
    uint64_t tsc_ticks   = getInvariantTSC(before, after);
    double   avg_freq_hz = getAverageFrequency(before, after);
    double   cpu_load    = (avg_freq_hz / pcm->getNominalFrequency()) * 100.0;

    DWORD exitCode = 0;
    GetExitCodeProcess(pi.hProcess, &exitCode);
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);

    // Read package and DRAM energy from RAPL counters
    double pkg_joules  = getConsumedJoules(before, after);
    double dram_joules = pcm->memoryTrafficMetricsAvailable()
                       ? getDRAMConsumedJoules(before, after)
                       : -1.0;

    // Treat zero as unavailable (driver could not read the counter)
    if (dram_joules == 0.0) dram_joules = -1.0;

    // Clean up hardware counters
    pcm->cleanup();

    if (exitCode != 0)
        std::cerr << "[WARN] Program exited with code " << exitCode << "\n";

    std::cout << "PKG energy:  " << pkg_joules << " J\n";
    std::cout << "DRAM energy: "
              << (dram_joules >= 0 ? std::to_string(dram_joules) + " J" : "N/A") << "\n";
    std::cout << "Wall time:   " << wallSeconds << " s\n";
    std::cout << "Avg freq:    " << avg_freq_hz / 1e9 << " GHz\n";
    std::cout << "Nominal:     " << pcm->getNominalFrequency() / 1e9 << " GHz\n";
    std::cout << "CPU load:    " << std::fixed << std::setprecision(1)
              << cpu_load << " %\n";
    std::cout << "Exit code:   " << exitCode << "\n";

    return {exe, param, pkg_joules, dram_joules, wallSeconds, (int)exitCode};
}

// STAGE 4 EMISSION REPORT
void stageEmissionReport(
    const BenchmarkResult& bench,
    const json& allData,
    time_t now
) { std::cout << " STAGE 4: Emission Report\n";

    // Include DRAM if available, otherwise PKG only
    double totalJoules = bench.pkg_joules;
    if (bench.dram_joules >= 0)
        totalJoules += bench.dram_joules;
    double kWh = totalJoules / 3600000.0;

    // Current emission from dataset 265 (real-time factor)
    double currentFactor = 0.0;
    std::string currentTime = "N/A";
    try {
        auto pts265 = allData["raw"]["emission_rt"]["data"];
        if (!pts265.empty()) {
            // Last entry is the most recent (sorted asc)
            auto& latest = pts265.back();
            currentFactor = latest["value"].get<double>();
            currentTime   = latest["startTime"].get<std::string>();
        }
    } catch (...) {}

    double nowEmission = kWh * currentFactor;

    std::cout << "\nProgram:      " << bench.program << "\n";
    if (!bench.param.empty())
        std::cout << "Parameter:    " << bench.param << "\n";
   std::cout << "Energy used:  "
        << std::fixed << std::setprecision(4) << bench.pkg_joules << " J (PKG)";
        if (bench.dram_joules >= 0)
        std::cout << " + " << bench.dram_joules << " J (DRAM)"
        << " = " << totalJoules << " J (total)";
        std::cout << "  (" << std::setprecision(8) << kWh << " kWh)\n";
    std::cout << "Wall time:    " 
          << std::fixed << std::setprecision(2) 
          << bench.wall_seconds << " s\n";
    std::cout << "\nCurrent emission\n";
    std::cout << "Time:         " << toFinnish(parseIso(currentTime)) << " (Helsinki)\n";
    std::cout << "Grid factor:  " 
          << std::fixed << std::setprecision(3) 
          << currentFactor << " gCO2/kWh\n";
    std::cout << "Emission:     " << formatEmission(nowEmission) << "\n";

    // 3 green windows from forecast
    struct SlotInfo { time_t t; double factor; double emission; };
    std::vector<SlotInfo> slots;

    try {
        for (auto& s : allData["forecast"]["slots"]) {
            time_t t      = parseIso(s["time"].get<std::string>());
            double factor = s["A_gCO2_kWh"].get<double>();
            slots.push_back({t, factor, kWh * factor});
        }
    } catch (...) {}

    std::sort(slots.begin(), slots.end(),
              [](const SlotInfo& a, const SlotInfo& b){ return a.factor < b.factor; });

    std::cout << "\n3 green windows (3 lowest emission slots until 18:00)\n";
    for (int i = 0; i < 3 && i < (int)slots.size(); i++) {
        double saving = (nowEmission > 0)
                      ? (1.0 - slots[i].emission / nowEmission) * 100.0
                      : 0.0;
        std::cout << "  #" << (i+1) << "  "
                  << toFinnish(slots[i].t)
                  << "   " << std::fixed << std::setprecision(2)
                  << slots[i].factor << " gCO2/kWh"
                  << " and " << formatEmission(slots[i].emission)
                  << " (saves " << saving << "%)\n";
    }

    // Write to CSV
    std::string csvFile = "emissions.csv";
    bool fileExists = fs::exists(csvFile);
    std::ofstream csv(csvFile, std::ios::app);

    if (!fileExists) {
        csv << "Timestamp;Program;Param;PKG_J;DRAM_J;WallTime_s;"
            << "Now_gCO2_kWh;Now_emission;"
            << "Best1_time;Best1_gCO2_kWh;Best1_emission;Best1_saving_pct;"
            << "Best2_time;Best2_gCO2_kWh;Best2_emission;Best2_saving_pct;"
            << "Best3_time;Best3_gCO2_kWh;Best3_emission;Best3_saving_pct\n";
    }

    csv << toFinnish(now) << ";"
        << bench.program  << ";"
        << bench.param    << ";"
        << bench.pkg_joules << ";"
        << bench.dram_joules << ";"
        << bench.wall_seconds << ";"
        << currentFactor  << ";"
        << formatEmission(nowEmission);

    for (int i = 0; i < 3; i++) {
        if (i < (int)slots.size()) {
            double saving = (nowEmission > 0)
                          ? (1.0 - slots[i].emission / nowEmission) * 100.0
                          : 0.0;
            csv << ";" << toFinnish(slots[i].t)
                << ";" << slots[i].factor
                << ";" << formatEmission(slots[i].emission)
                << ";" << std::fixed << std::setprecision(2) << saving;
        } else {
            csv << ";N/A;N/A;N/A;N/A";
        }
    }
    csv << "\n";

    std::cout << "\nAppended to: " << csvFile << "\n";
}

int main(int argc, char* argv[]) {
SetConsoleOutputCP(CP_UTF8);

    // Usage: apitest.exe <program.exe> [param]
    if (argc < 2) {
        std::cerr << "Usage:\n"
                  << "  apitest.exe <program.exe>\n"
                  << "  apitest.exe <program.exe> <param>\n";
        return 1;
    }

    std::string targetExe = argv[1];
    std::string targetParam = (argc >= 3) ? argv[2] : "";

    std::string apiKey = loadApiKey();
    if (apiKey.empty()) {
        std::cerr << "[FATAL] API key missing (.env not found or APIKEY= not set)\n";
        return 1;
    }

    time_t now = time(nullptr);

    // Print header

    std::cout << "  Green Benchmark Emission Analyzer  \n";
    std::cout << "Time:     " << toFinnish(now) << " (Helsinki)\n";
    std::cout << "Program:  " << targetExe << "\n";
    if (!targetParam.empty())
        std::cout << "Param:    " << targetParam << "\n";

    // Delete old data.json
    if (std::remove("data.json") == 0)
        std::cout << "\n[INFO] Cleared old data.json\n";

    json allData;

    // Stage 1: Fetch
    stageFetch(allData, apiKey);

    // Write data.json with raw data
    {
        std::ofstream f("data.json");
        f << allData.dump(2);
    }

    // Stage 2: Forecast
    stageForecast(allData);

    // Update data.json with forecast appended
    {
        std::ofstream f("data.json");
        f << allData.dump(2);
        std::cout << "Updated data.json (raw + forecast)\n";
    }

    // Stage 3: PCM benchmark
    BenchmarkResult bench = stagePCM(targetExe, targetParam);

    // Stage 4: Emission report
    stageEmissionReport(bench, allData, now);
    std::cout << " All done.\n";
    return 0;
}
