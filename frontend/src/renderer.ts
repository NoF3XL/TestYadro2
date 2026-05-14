import { SystemMetrics, ProcessInfo, NetworkInfo } from './metrics';

function el(id: string): HTMLElement {
    return document.getElementById(id)!;
}

function formatBytes(kb: number): string {
    if (kb >= 1048576) return (kb / 1048576).toFixed(1) + ' GB';
    if (kb >= 1024) return (kb / 1024).toFixed(1) + ' MB';
    return kb.toFixed(0) + ' KB';
}

function formatRate(kbps: number): string {
    if (kbps >= 1048576) return (kbps / 1048576).toFixed(1) + ' GB/s';
    if (kbps >= 1024) return (kbps / 1024).toFixed(1) + ' MB/s';
    if (kbps >= 1) return kbps.toFixed(1) + ' KB/s';
    return (kbps * 1024).toFixed(0) + ' B/s';
}

function formatUptime(seconds: number): string {
    const d = Math.floor(seconds / 86400);
    const h = Math.floor((seconds % 86400) / 3600);
    const m = Math.floor((seconds % 3600) / 60);
    if (d > 0) return `${d}d ${h}h ${m}m`;
    if (h > 0) return `${h}h ${m}m`;
    return `${m}m`;
}

function usageClass(pct: number): string {
    if (pct >= 90) return 'critical';
    if (pct >= 70) return 'warning';
    return 'ok';
}

function usageColor(pct: number): string {
    if (pct >= 90) return '#ff4757';
    if (pct >= 70) return '#ffa502';
    return '#2ed573';
}

export function renderAll(m: SystemMetrics): void {
    renderSystem(m);
    renderCpu(m);
    renderMemory(m);
    renderProcesses(m);
    renderNetwork(m);
}

function renderSystem(m: SystemMetrics): void {
    const s = m.system;
    el('sys-hostname').textContent = s.hostname;
    el('sys-kernel').textContent = s.kernel_version;
    el('sys-uptime').textContent = formatUptime(s.uptime_seconds);
    el('sys-load1').textContent = s.loadavg_1.toFixed(2);
    el('sys-load5').textContent = s.loadavg_5.toFixed(2);
    el('sys-load15').textContent = s.loadavg_15.toFixed(2);
    el('sys-proc-total').textContent = String(m.total_processes);
    el('sys-proc-running').textContent = String(m.running_processes);
    el('sys-proc-sleeping').textContent = String(m.sleeping_processes);
    el('sys-proc-zombie').textContent = String(m.zombie_processes);
}

function renderCpu(m: SystemMetrics): void {
    const cpu = m.cpu;
    const pct = cpu.usage_percent;

    el('cpu-gauge-text').textContent = pct.toFixed(1) + '%';
    el('cpu-gauge-text').className = 'gauge-value ' + usageClass(pct);

    const gauge = el('cpu-gauge') as unknown as SVGElement;
    renderGauge(gauge, pct);

    let coresHtml = '';
    for (let i = 0; i < cpu.per_core_usage.length; i++) {
        const corePct = cpu.per_core_usage[i];
        const cls = usageClass(corePct);
        const barWidth = Math.max(corePct, 1);
        coresHtml += `
            <div class="core-row">
                <span class="core-label">Core ${i}</span>
                <div class="core-bar-track">
                    <div class="core-bar-fill ${cls}" style="width:${barWidth.toFixed(1)}%"></div>
                </div>
                <span class="core-value ${cls}">${corePct.toFixed(1)}%</span>
            </div>`;
    }
    el('cpu-cores').innerHTML = coresHtml;
}

function renderGauge(svgEl: SVGElement, pct: number): void {
    const radius = 54;
    const circumference = 2 * Math.PI * radius;
    const offset = circumference - (pct / 100) * circumference;
    const color = usageColor(pct);

    svgEl.innerHTML = `
        <circle cx="64" cy="64" r="${radius}" fill="none" stroke="#1e2a3a" stroke-width="10"/>
        <circle cx="64" cy="64" r="${radius}" fill="none" stroke="${color}" stroke-width="10"
            stroke-dasharray="${circumference}" stroke-dashoffset="${offset}"
            stroke-linecap="round" transform="rotate(-90 64 64)"
            style="transition: stroke-dashoffset 0.5s ease, stroke 0.5s ease"/>
    `;
}

function renderMemory(m: SystemMetrics): void {
    const mem = m.memory;
    const usedKb = mem.used_kb;
    const totalKb = mem.total_kb;
    const availKb = mem.available_kb;
    const cacheKb = mem.buffers_kb + mem.cached_kb;

    const memPct = mem.usage_percent;
    el('mem-pct').textContent = memPct.toFixed(1) + '%';
    el('mem-pct').className = 'mem-value ' + usageClass(memPct);
    el('mem-bar').style.width = Math.max(memPct, 1).toFixed(1) + '%';
    el('mem-bar').className = 'fill-bar ' + usageClass(memPct);
    el('mem-used').textContent = formatBytes(usedKb);
    el('mem-total').textContent = formatBytes(totalKb);
    el('mem-avail').textContent = formatBytes(availKb);
    el('mem-cache').textContent = formatBytes(cacheKb);

    if (mem.swap_total_kb > 0) {
        const swapPct = mem.swap_usage_percent;
        el('swap-section').style.display = '';
        el('swap-pct').textContent = swapPct.toFixed(1) + '%';
        el('swap-pct').className = 'mem-value ' + usageClass(swapPct);
        el('swap-bar').style.width = Math.max(swapPct, 1).toFixed(1) + '%';
        el('swap-bar').className = 'fill-bar ' + usageClass(swapPct);
        el('swap-used').textContent = formatBytes(mem.swap_used_kb);
        el('swap-total').textContent = formatBytes(mem.swap_total_kb);
    } else {
        el('swap-section').style.display = 'none';
    }
}

function renderProcesses(m: SystemMetrics): void {
    const tbody = el('proc-tbody');
    let rows = '';
    for (const p of m.top_processes) {
        const cpuCls = usageClass(p.cpu_percent);
        const memCls = usageClass(p.mem_percent);
        rows += `
            <tr>
                <td class="proc-pid">${p.pid}</td>
                <td class="proc-name" title="${escapeHtml(p.name)}">${escapeHtml(p.name)}</td>
                <td class="proc-user">${escapeHtml(p.user)}</td>
                <td class="proc-cpu ${cpuCls}">${p.cpu_percent.toFixed(1)}%</td>
                <td class="proc-mem ${memCls}">${p.mem_percent.toFixed(1)}%</td>
                <td class="proc-rss">${formatBytes(p.rss_kb)}</td>
            </tr>`;
    }
    tbody.innerHTML = rows;
}

function renderNetwork(m: SystemMetrics): void {
    let html = '';
    for (const net of m.networks) {
        html += `
            <div class="net-row">
                <span class="net-name">${escapeHtml(net.name)}</span>
                <span class="net-rx">▼ ${formatRate(net.rx_rate_kbps)}</span>
                <span class="net-tx">▲ ${formatRate(net.tx_rate_kbps)}</span>
            </div>`;
    }
    el('net-interfaces').innerHTML = html || '<div class="net-row"><span>No network data</span></div>';
}

function escapeHtml(s: string): string {
    const d = document.createElement('div');
    d.textContent = s;
    return d.innerHTML;
}
