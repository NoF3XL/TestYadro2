export interface CpuInfo {
    usage_percent: number;
    per_core_usage: number[];
    core_count: number;
}

export interface MemoryInfo {
    total_kb: number;
    free_kb: number;
    available_kb: number;
    used_kb: number;
    buffers_kb: number;
    cached_kb: number;
    swap_total_kb: number;
    swap_free_kb: number;
    swap_used_kb: number;
    usage_percent: number;
    swap_usage_percent: number;
}

export interface ProcessInfo {
    pid: number;
    name: string;
    user: string;
    cpu_percent: number;
    mem_percent: number;
    rss_kb: number;
}

export interface NetworkInfo {
    name: string;
    rx_bytes: number;
    tx_bytes: number;
    rx_rate_kbps: number;
    tx_rate_kbps: number;
}

export interface SystemInfo {
    hostname: string;
    uptime_seconds: number;
    loadavg_1: number;
    loadavg_5: number;
    loadavg_15: number;
    kernel_version: string;
}

export interface SystemMetrics {
    cpu: CpuInfo;
    memory: MemoryInfo;
    top_processes: ProcessInfo[];
    networks: NetworkInfo[];
    system: SystemInfo;
    total_processes: number;
    running_processes: number;
    sleeping_processes: number;
    stopped_processes: number;
    zombie_processes: number;
}
