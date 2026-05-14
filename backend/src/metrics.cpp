#include "metrics.hpp"

#include <algorithm>
#include <cctype>
#include <chrono>
#include <cstring>
#include <dirent.h>
#include <fstream>
#include <pwd.h>
#include <sstream>
#include <sys/stat.h>
#include <sys/sysinfo.h>
#include <sys/utsname.h>
#include <unistd.h>
#include <vector>

static uint64_t get_time_ms() {
    auto now = std::chrono::steady_clock::now().time_since_epoch();
    return std::chrono::duration_cast<std::chrono::milliseconds>(now).count();
}

static std::string read_file(const std::string& path) {
    std::ifstream f(path);
    if (!f.is_open()) return {};
    std::stringstream ss;
    ss << f.rdbuf();
    return ss.str();
}

static std::vector<std::string> split_line(const std::string& line) {
    std::vector<std::string> parts;
    std::istringstream iss(line);
    std::string token;
    while (iss >> token) {
        parts.push_back(token);
    }
    return parts;
}

MetricsCollector::MetricsCollector() {}

SystemMetrics MetricsCollector::collect() {
    SystemMetrics m;
    m.cpu = collect_cpu();
    m.memory = collect_memory();
    m.system = collect_system();
    m.networks = collect_network();

    int running = 0, sleeping = 0, stopped = 0, zombie = 0, total = 0;
    count_process_states(running, sleeping, stopped, zombie, total);
    m.total_processes = total;
    m.running_processes = running;
    m.sleeping_processes = sleeping;
    m.stopped_processes = stopped;
    m.zombie_processes = zombie;

    m.top_processes = collect_top_processes();
    return m;
}

CpuInfo MetricsCollector::collect_cpu() {
    CpuInfo info;
    std::string data = read_file("/proc/stat");
    if (data.empty()) return info;

    std::vector<uint64_t> core_totals, core_idles;
    uint64_t total_total = 0, total_idle = 0;

    std::istringstream iss(data);
    std::string line;
    while (std::getline(iss, line)) {
        if (line.rfind("cpu", 0) != 0) break;

        std::vector<std::string> parts = split_line(line);
        if (parts.size() < 8) continue;

        std::string label = parts[0];
        uint64_t user    = std::stoull(parts[1]);
        uint64_t nice    = std::stoull(parts[2]);
        uint64_t system  = std::stoull(parts[3]);
        uint64_t idle    = std::stoull(parts[4]);
        uint64_t iowait  = parts.size() > 5 ? std::stoull(parts[5]) : 0;
        uint64_t irq     = parts.size() > 6 ? std::stoull(parts[6]) : 0;
        uint64_t softirq = parts.size() > 7 ? std::stoull(parts[7]) : 0;
        uint64_t steal   = parts.size() > 8 ? std::stoull(parts[8]) : 0;

        uint64_t line_total = user + nice + system + idle + iowait + irq + softirq + steal;
        uint64_t line_idle  = idle + iowait;

        if (label == "cpu") {
            total_total = line_total;
            total_idle = line_idle;
        } else {
            core_totals.push_back(line_total);
            core_idles.push_back(line_idle);
        }
    }

    info.core_count = static_cast<int>(core_totals.size());

    CpuSample prev = prev_cpu_;
    prev_cpu_.total = total_total;
    prev_cpu_.idle = total_idle;
    prev_cpu_.per_core_total = core_totals;
    prev_cpu_.per_core_idle = core_idles;
    has_prev_cpu_ = true;

    if (prev.total == 0) return info;

    uint64_t total_delta = total_total - prev.total;
    uint64_t idle_delta = total_idle - prev.idle;
    if (total_delta > 0) {
        info.usage_percent = 100.0 * (1.0 - static_cast<double>(idle_delta) / total_delta);
        if (info.usage_percent < 0) info.usage_percent = 0;
        if (info.usage_percent > 100) info.usage_percent = 100;
    }

    for (size_t i = 0; i < core_totals.size() && i < prev.per_core_total.size(); i++) {
        uint64_t dt = core_totals[i] - prev.per_core_total[i];
        uint64_t di = core_idles[i] - prev.per_core_idle[i];
        double pct = 0.0;
        if (dt > 0) {
            pct = 100.0 * (1.0 - static_cast<double>(di) / dt);
            if (pct < 0) pct = 0;
            if (pct > 100) pct = 100;
        }
        info.per_core_usage.push_back(pct);
    }

    return info;
}

MemoryInfo MetricsCollector::collect_memory() {
    MemoryInfo info;
    std::string data = read_file("/proc/meminfo");
    if (data.empty()) return info;

    std::istringstream iss(data);
    std::string line;
    while (std::getline(iss, line)) {
        std::vector<std::string> parts = split_line(line);
        if (parts.size() < 2) continue;

        uint64_t val = std::stoull(parts[1]);
        if (line.rfind("MemTotal:", 0) == 0)      info.total_kb = val;
        else if (line.rfind("MemFree:", 0) == 0)   info.free_kb = val;
        else if (line.rfind("MemAvailable:", 0) == 0) info.available_kb = val;
        else if (line.rfind("Buffers:", 0) == 0)   info.buffers_kb = val;
        else if (line.rfind("Cached:", 0) == 0)    info.cached_kb = val;
        else if (line.rfind("SwapTotal:", 0) == 0) info.swap_total_kb = val;
        else if (line.rfind("SwapFree:", 0) == 0)  info.swap_free_kb = val;
    }

    if (info.total_kb > 0) {
        info.used_kb = info.total_kb - info.available_kb;
        info.usage_percent = 100.0 * info.used_kb / info.total_kb;
    }
    if (info.swap_total_kb > 0) {
        info.swap_used_kb = info.swap_total_kb - info.swap_free_kb;
        info.swap_usage_percent = 100.0 * info.swap_used_kb / info.swap_total_kb;
    }

    return info;
}

SystemInfo MetricsCollector::collect_system() {
    SystemInfo info;

    char host[256] = {};
    gethostname(host, sizeof(host));
    info.hostname = host;

    utsname uts{};
    if (uname(&uts) == 0) {
        info.kernel_version = std::string(uts.sysname) + " " + uts.release;
    }

    std::string uptime_str = read_file("/proc/uptime");
    if (!uptime_str.empty()) {
        std::vector<std::string> parts = split_line(uptime_str);
        if (!parts.empty()) {
            info.uptime_seconds = static_cast<uint64_t>(std::stod(parts[0]));
        }
    }

    std::string load_str = read_file("/proc/loadavg");
    if (!load_str.empty()) {
        std::vector<std::string> parts = split_line(load_str);
        if (parts.size() >= 3) {
            info.loadavg_1  = std::stod(parts[0]);
            info.loadavg_5  = std::stod(parts[1]);
            info.loadavg_15 = std::stod(parts[2]);
        }
    }

    return info;
}

void MetricsCollector::count_process_states(int& running, int& sleeping, int& stopped, int& zombie, int& total) {
    running = sleeping = stopped = zombie = total = 0;
    DIR* dir = opendir("/proc");
    if (!dir) return;

    dirent* entry;
    while ((entry = readdir(dir)) != nullptr) {
        if (entry->d_type != DT_DIR) continue;
        if (!std::isdigit(entry->d_name[0])) continue;

        total++;
        std::string stat_path = std::string("/proc/") + entry->d_name + "/stat";
        std::string stat_data = read_file(stat_path);
        if (stat_data.empty()) continue;

        size_t close_paren = stat_data.rfind(')');
        if (close_paren == std::string::npos) continue;

        std::string after = stat_data.substr(close_paren + 2);
        if (!after.empty()) {
            char state = after[0];
            switch (state) {
                case 'R': running++; break;
                case 'S':
                case 'D': sleeping++; break;
                case 'T': stopped++; break;
                case 'Z': zombie++; break;
            }
        }
    }
    closedir(dir);
}

std::vector<ProcessInfo> MetricsCollector::collect_top_processes() {
    std::vector<ProcessInfo> result;
    uint64_t now_ms = get_time_ms();

    struct sysinfo si{};
    uint64_t total_ram = 0;
    if (sysinfo(&si) == 0) {
        total_ram = si.totalram;
    }

    DIR* dir = opendir("/proc");
    if (!dir) return result;

    dirent* entry;
    while ((entry = readdir(dir)) != nullptr) {
        if (entry->d_type != DT_DIR) continue;
        if (!std::isdigit(entry->d_name[0])) continue;

        int pid = std::stoi(entry->d_name);
        std::string stat_path = std::string("/proc/") + entry->d_name + "/stat";

        std::string stat_data = read_file(stat_path);
        if (stat_data.empty()) continue;

        size_t close_paren = stat_data.rfind(')');
        if (close_paren == std::string::npos) continue;

        std::string after = stat_data.substr(close_paren + 2);
        std::vector<std::string> parts = split_line(after);
        if (parts.size() < 22) continue;

        // Fields after comm: state(0) ppid(1) ... utime(11) stime(12) ... vsize(20) rss(21)
        uint64_t utime = std::stoull(parts[11]);
        uint64_t stime = std::stoull(parts[12]);
        uint64_t rss = std::stoull(parts[21]);
        long page_size = sysconf(_SC_PAGESIZE);
        uint64_t rss_kb = rss * page_size / 1024;

        double cpu_pct = 0.0;
        auto it = prev_proc_.find(pid);
        if (it != prev_proc_.end() && prev_proc_time_ > 0) {
            uint64_t delta_cpu = (utime + stime) - (it->second.utime + it->second.stime);
            double elapsed_sec = (now_ms - prev_proc_time_) / 1000.0;
            long ticks = sysconf(_SC_CLK_TCK);
            if (elapsed_sec > 0 && ticks > 0) {
                cpu_pct = 100.0 * delta_cpu / (elapsed_sec * ticks);
                if (cpu_pct < 0) cpu_pct = 0;
                if (cpu_pct > 100) cpu_pct = 100;
            }
        }

        prev_proc_[pid] = {utime, stime};

        std::string name;
        size_t open_paren = stat_data.find('(');
        if (open_paren != std::string::npos && close_paren > open_paren) {
            name = stat_data.substr(open_paren + 1, close_paren - open_paren - 1);
        }
        if (name.empty()) name = "?";

        std::string user;
        std::string status_path = std::string("/proc/") + entry->d_name + "/status";
        std::string status_data = read_file(status_path);
        std::istringstream status_stream(status_data);
        std::string status_line;
        while (std::getline(status_stream, status_line)) {
            if (status_line.rfind("Uid:", 0) == 0) {
                std::vector<std::string> uid_parts = split_line(status_line);
                if (uid_parts.size() >= 2) {
                    uid_t uid = std::stoi(uid_parts[1]);
                    passwd* pw = getpwuid(uid);
                    if (pw) user = pw->pw_name;
                }
                break;
            }
        }
        if (user.empty()) user = "?";

        double mem_pct = 0.0;
        if (total_ram > 0) {
            mem_pct = 100.0 * (rss_kb * 1024.0) / total_ram;
            if (mem_pct > 100) mem_pct = 100;
        }

        result.push_back({pid, name, user, cpu_pct, mem_pct, rss_kb});
    }
    closedir(dir);

    prev_proc_time_ = now_ms;

    std::sort(result.begin(), result.end(),
              [](const ProcessInfo& a, const ProcessInfo& b) {
                  return a.cpu_percent > b.cpu_percent;
              });

    if (result.size() > 20) {
        result.resize(20);
    }

    return result;
}

std::vector<NetworkInfo> MetricsCollector::collect_network() {
    std::vector<NetworkInfo> result;
    std::string data = read_file("/proc/net/dev");
    if (data.empty()) return result;

    uint64_t now_ms = get_time_ms();

    std::istringstream iss(data);
    std::string line;
    bool header_skipped = false;
    while (std::getline(iss, line)) {
        if (!header_skipped) {
            if (line.find(':') != std::string::npos) {
                header_skipped = true;
            }
            continue;
        }

        size_t colon = line.find(':');
        if (colon == std::string::npos) continue;

        std::string name = line.substr(0, colon);
        name.erase(0, name.find_first_not_of(" \t"));
        name.erase(name.find_last_not_of(" \t") + 1);
        if (name == "lo") continue;

        std::string rest = line.substr(colon + 1);
        std::vector<std::string> parts = split_line(rest);
        if (parts.size() < 10) continue;

        uint64_t rx = std::stoull(parts[0]);
        uint64_t tx = std::stoull(parts[8]);

        double rx_rate = 0.0, tx_rate = 0.0;
        auto it = prev_net_.find(name);
        if (it != prev_net_.end() && prev_net_time_ > 0) {
            double elapsed = (now_ms - prev_net_time_) / 1000.0;
            if (elapsed > 0) {
                rx_rate = (rx > it->second.rx_bytes ? (rx - it->second.rx_bytes) : 0)
                          / elapsed / 1024.0;
                tx_rate = (tx > it->second.tx_bytes ? (tx - it->second.tx_bytes) : 0)
                          / elapsed / 1024.0;
            }
        }

        prev_net_[name] = {rx, tx};
        result.push_back({name, rx, tx, rx_rate, tx_rate});
    }

    prev_net_time_ = now_ms;
    return result;
}
