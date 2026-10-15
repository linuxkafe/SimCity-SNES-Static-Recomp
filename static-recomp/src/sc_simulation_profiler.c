#include "sc_simulation_profiler.h"
#include "sc_v11_runtime.h"
#include <stdio.h>
#include <inttypes.h>

ScSimGroupProfile g_sim_group_profiles[SC_V35_TOWN_ROUTE_GROUP_COUNT];
uint8_t g_sim_profiling_enabled = 0;

uint32_t group_id_from_address(uint32_t address) {
    uint32_t high = address >> 10;
    if (high >= 0x20 && high <= 0x33) return high - 0x20;
    if (high >= 0x62 && high <= 0x7E) return (high - 0x62) + 0x14;
    if (high >= 0xA0 && high <= 0xAF) return (high - 0xA0) + 0x30;
    if (high == 0xB8) return 0x40;
    if (high >= 0xE0 && high <= 0xEF) return (high - 0xE0) + 0x41;
    if (high >= 0xF0 && high <= 0xFF) return (high - 0xF0) + 0x51;
    if (high >= 0x164 && high <= 0x166) return (high - 0x164) + 0x61;
    return 0xFF;
}

void sc_sim_profiler_init(void) {
    for (uint32_t i = 0; i < SC_V35_TOWN_ROUTE_GROUP_COUNT; i++) {
        g_sim_group_profiles[i].call_count = 0;
        g_sim_group_profiles[i].total_cycles = 0;
        g_sim_group_profiles[i].min_cycles = 0xFFFFFFFF;
        g_sim_group_profiles[i].max_cycles = 0;
        g_sim_group_profiles[i].last_address = 0;
    }
    g_sim_profiling_enabled = 1;
}

void sc_sim_profiler_record_entry(uint32_t group_id, uint32_t address) {
    if (!g_sim_profiling_enabled || group_id >= SC_V35_TOWN_ROUTE_GROUP_COUNT) return;
    g_sim_group_profiles[group_id].last_address = address;
}

void sc_sim_profiler_record_exit(uint32_t group_id, uint32_t cycles) {
    if (!g_sim_profiling_enabled || group_id >= SC_V35_TOWN_ROUTE_GROUP_COUNT) return;
    ScSimGroupProfile *p = &g_sim_group_profiles[group_id];
    p->call_count++;
    p->total_cycles += cycles;
    if (cycles < p->min_cycles) p->min_cycles = cycles;
    if (cycles > p->max_cycles) p->max_cycles = cycles;
}

void sc_sim_profiler_dump(void) {
    printf("\n=== SIMULATION PROFILE (v35 town route groups) ===\n");
    printf("%-6s %-12s %-12s %-10s %-10s %-10s %s\n",
           "GrpID", "Calls", "TotalCycles", "AvgCycles", "Min", "Max", "LastAddr");
    for (uint32_t i = 0; i < SC_V35_TOWN_ROUTE_GROUP_COUNT; i++) {
        ScSimGroupProfile *p = &g_sim_group_profiles[i];
        if (p->call_count == 0) continue;
        uint32_t avg = p->call_count ? (uint32_t)(p->total_cycles / p->call_count) : 0;
        printf("0x%04X %-12" PRIu64 " %-12" PRIu64 " %-10u %-10u %-10u 0x%06X\n",
               i, p->call_count, p->total_cycles, avg, p->min_cycles, p->max_cycles, p->last_address);
    }
    printf("==================================================\n\n");
}

int sc_sim_profiler_get_top_groups(uint32_t *out_groups, uint32_t max_groups) {
    typedef struct { uint32_t id; uint64_t cycles; } Pair;
    Pair pairs[SC_V35_TOWN_ROUTE_GROUP_COUNT];
    uint32_t count = 0;
    for (uint32_t i = 0; i < SC_V35_TOWN_ROUTE_GROUP_COUNT; i++) {
        if (g_sim_group_profiles[i].call_count > 0) {
            pairs[count++] = (Pair){i, g_sim_group_profiles[i].total_cycles};
        }
    }
    for (uint32_t i = 0; i < count - 1; i++) {
        for (uint32_t j = i + 1; j < count; j++) {
            if (pairs[j].cycles > pairs[i].cycles) {
                Pair tmp = pairs[i]; pairs[i] = pairs[j]; pairs[j] = tmp;
            }
        }
    }
    uint32_t n = count < max_groups ? count : max_groups;
    for (uint32_t i = 0; i < n; i++) out_groups[i] = pairs[i].id;
    return n;
}