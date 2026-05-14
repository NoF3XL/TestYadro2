#pragma once

#include <cstdint>
#include <map>
#include <string>
#include <vector>

struct CpuInfo {
    double usage_percent = 0.0;
    std::vector<double> per_core_usage;
    int core_count = 0;
};

struct MemoryInfo {
    uint64_t total_kb = 0;
    uint64_t free_kb = 0;
    uint64_t available_kb = 0;
    uint64_t used_kb = 0;
    uint64_t buffers_kb = 0;
    uint64_t cached_kb = 0;
    uint64_t swap_total_kb = 0;
    uint64_t swap_free_kb = 0;
    uint64_t swap_used_kb = 0;
    double usage_percent = 0.0;
    double swap_usage_percent = 0.0;
};

struct ProcessInfo {
    int pid = 0;
    std::string name;
    std::string user;
    double cpu_percent = 0.0;
    double mem_percent = 0.0;
    uint64_t rss_kb = 0;
};

struct NetworkInfo {
    std::string name;
    uint64_t rx_bytes = 0;
    uint64_t tx_bytes = 0;
    double rx_rate_kbps = 0.0;
    double tx_rate_kbps = 0.0;
};

struct SystemInfo {
    std::string hostname;
    uint64_t uptime_seconds = 0;
    double loadavg_1 = 0.0;
    double loadavg_5 = 0.0;
    double loadavg_15 = 0.0;
    std::string kernel_version;
};

struct SystemMetrics {
    CpuInfo cpu;
    MemoryInfo memory;
    std::vector<ProcessInfo> top_processes;
    std::vector<NetworkInfo> networks;
    SystemInfo system;
    int total_processes = 0;
    int running_processes = 0;
    int sleeping_processes = 0;
    int stopped_processes = 0;
    int zombie_processes = 0;
};

class MetricsCollector {
public:
    MetricsCollector();

    SystemMetrics collect();

private:
    CpuInfo collect_cpu();
    MemoryInfo collect_memory();
    std::vector<ProcessInfo> collect_top_processes();
    std::vector<NetworkInfo> collect_network();
    SystemInfo collect_system();
    void count_process_states(int& running, int& sleeping, int& stopped, int& zombie, int& total);

    struct CpuSample {
        uint64_t total = 0;
        uint64_t idle = 0;
        std::vector<uint64_t> per_core_total;
        std::vector<uint64_t> per_core_idle;
    };
    CpuSample prev_cpu_;
    bool has_prev_cpu_ = false;

    struct NetSample {
        uint64_t rx_bytes = 0;
        uint64_t tx_bytes = 0;
    };
    std::map<std::string, NetSample> prev_net_;
    uint64_t prev_net_time_ = 0;

    struct ProcSample {
        uint64_t utime = 0;
        uint64_t stime = 0;
    };
    std::map<int, ProcSample> prev_proc_;
    uint64_t prev_proc_time_ = 0;
};
