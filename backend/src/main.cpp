#include "metrics.hpp"

#include <nlohmann/json.hpp>
#define CPPHTTPLIB_OPENSSL_SUPPORT 0
#include <httplib.h>

#include <atomic>
#include <chrono>
#include <csignal>
#include <iostream>
#include <mutex>
#include <thread>

using json = nlohmann::json;

NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(CpuInfo, usage_percent, per_core_usage, core_count)
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(MemoryInfo, total_kb, free_kb, available_kb, used_kb, buffers_kb, cached_kb,
                                    swap_total_kb, swap_free_kb, swap_used_kb, usage_percent, swap_usage_percent)
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(ProcessInfo, pid, name, user, cpu_percent, mem_percent, rss_kb)
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(NetworkInfo, name, rx_bytes, tx_bytes, rx_rate_kbps, tx_rate_kbps)
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(SystemInfo, hostname, uptime_seconds, loadavg_1, loadavg_5, loadavg_15, kernel_version)
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(SystemMetrics, cpu, memory, top_processes, networks, system,
                                    total_processes, running_processes, sleeping_processes,
                                    stopped_processes, zombie_processes)

static std::mutex g_metrics_mutex;
static SystemMetrics g_metrics;
static std::atomic<bool> g_running{true};

static void collector_thread() {
    MetricsCollector collector;
    while (g_running) {
        SystemMetrics m = collector.collect();
        {
            std::lock_guard<std::mutex> lock(g_metrics_mutex);
            g_metrics = std::move(m);
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(900));
    }
}

static void signal_handler(int) {
    g_running = false;
}

int main(int argc, char* argv[]) {
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);

    std::thread collector(collector_thread);

    httplib::Server svr;

    svr.Get("/api/metrics", [](const httplib::Request&, httplib::Response& res) {
        json j;
        {
            std::lock_guard<std::mutex> lock(g_metrics_mutex);
            j = g_metrics;
        }
        res.set_content(j.dump(), "application/json");
    });

    const char* static_dir = "./static";
    if (argc > 1) {
        static_dir = argv[1];
    }
    std::cout << "Serving static files from: " << static_dir << std::endl;

    svr.set_mount_point("/", static_dir);

    int port = 8080;
    const char* host = "0.0.0.0";
    if (argc > 2) {
        port = std::stoi(argv[2]);
    }

    std::cout << "SysMon server starting on http://" << host << ":" << port << std::endl;
    std::cout << "Press Ctrl+C to stop" << std::endl;

    svr.listen(host, port);

    g_running = false;
    collector.join();

    return 0;
}
