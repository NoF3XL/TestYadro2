import { fetchMetrics } from './api';
import { renderAll } from './renderer';
import { SystemMetrics } from './metrics';

let updateTimer: ReturnType<typeof setInterval> | null = null;
let errorCount = 0;

async function update(): Promise<void> {
    try {
        const metrics = await fetchMetrics();
        errorCount = 0;
        renderAll(metrics);
        hideError();
    } catch (e) {
        errorCount++;
        if (errorCount <= 3) {
            console.warn('Metrics fetch failed:', e);
        }
        if (errorCount >= 5) {
            showError('Connection to server lost. Retrying...');
        }
    }
}

function showError(msg: string): void {
    const banner = document.getElementById('error-banner');
    if (banner) {
        banner.textContent = msg;
        banner.style.display = 'block';
    }
}

function hideError(): void {
    const banner = document.getElementById('error-banner');
    if (banner) {
        banner.style.display = 'none';
    }
}

function start(): void {
    update();
    updateTimer = setInterval(update, 1000);
}

if (document.readyState === 'loading') {
    document.addEventListener('DOMContentLoaded', start);
} else {
    start();
}
