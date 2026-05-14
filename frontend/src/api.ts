import { SystemMetrics } from './metrics';

const API_URL = '/api/metrics';

export async function fetchMetrics(): Promise<SystemMetrics> {
    const response = await fetch(API_URL);
    if (!response.ok) {
        throw new Error(`HTTP ${response.status}: ${response.statusText}`);
    }
    return response.json();
}
