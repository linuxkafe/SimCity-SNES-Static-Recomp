#include "sc_simulation_profiler.h"
#include "sc_v11_runtime.h"
#include <stdio.h>
#include <string.h>
#include <inttypes.h>

ScSimGroupProfile g_sim_group_profiles[SC_V35_TOWN_ROUTE_GROUP_COUNT];
ScSimPcGroupProfile g_sim_pc_group_profiles[SC_SIM_PC_GROUP_COUNT];
uint8_t g_sim_profiling_enabled = 0;

/* Master clock of the previously traced fetch, so the interval between two
 * fetches can be charged to the instruction that ran in between. */
static uint64_t g_pc_prev_clock;
static uint32_t g_pc_prev_group;
static uint32_t g_pc_prev_address;
static uint8_t g_pc_has_prev;

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
    memset(g_sim_pc_group_profiles, 0, sizeof(g_sim_pc_group_profiles));
    g_pc_prev_clock = 0;
    g_pc_prev_group = 0;
    g_pc_prev_address = 0;
    g_pc_has_prev = 0;
    g_sim_profiling_enabled = 1;
}

void sc_sim_profiler_record_pc(uint32_t address, uint64_t master_clock) {
    uint32_t group;
    if (!g_sim_profiling_enabled) return;
    group = (address >> 10) & (SC_SIM_PC_GROUP_COUNT - 1u);
    if (g_pc_has_prev && master_clock >= g_pc_prev_clock) {
        ScSimPcGroupProfile *p = &g_sim_pc_group_profiles[g_pc_prev_group];
        p->instruction_count++;
        p->total_cycles += master_clock - g_pc_prev_clock;
        p->last_address = g_pc_prev_address;
    }
    g_pc_prev_group = group;
    g_pc_prev_clock = master_clock;
    g_pc_prev_address = address;
    g_pc_has_prev = 1;
}

static int pc_profile_desc(const void *a, const void *b) {
    const ScSimPcGroupProfile *left = a;
    const ScSimPcGroupProfile *right = b;
    if (left->total_cycles < right->total_cycles) return 1;
    if (left->total_cycles > right->total_cycles) return -1;
    return 0;
}

void sc_sim_profiler_dump_pc(uint32_t max_rows) {
    uint64_t grand_total = 0;
    uint32_t row = 0;
    if (max_rows == 0) return;
    for (uint32_t i = 0; i < SC_SIM_PC_GROUP_COUNT; i++)
        grand_total += g_sim_pc_group_profiles[i].total_cycles;
    printf("\n=== PC GROUP PROFILE (1 KiB address groups, top %u) ===\n",
           max_rows);
    printf("%-8s %-14s %-14s %-8s %s\n",
           "Group", "Instructions", "MasterClocks", "Share", "LastAddr");
    for (uint32_t i = 0; i < SC_SIM_PC_GROUP_COUNT && row < max_rows; i++) {
        ScSimPcGroupProfile *best = &g_sim_pc_group_profiles[i];
        uint32_t best_index = i;
        for (uint32_t j = i + 1; j < SC_SIM_PC_GROUP_COUNT; j++) {
            if (pc_profile_desc(&g_sim_pc_group_profiles[j], best) < 0) {
                best = &g_sim_pc_group_profiles[j];
                best_index = j;
            }
        }
        if (best->total_cycles == 0) break;
        if (best_index != i) {
            ScSimPcGroupProfile tmp = g_sim_pc_group_profiles[i];
            g_sim_pc_group_profiles[i] = *best;
            *best = tmp;
        }
        /* Sorting moves entries, so the group base comes from the recorded
           address rather than from the slot index. */
        printf("0x%05X  %-14" PRIu64 " %-14" PRIu64 " %5.1f%%  0x%06X\n",
               g_sim_pc_group_profiles[i].last_address & ~0x3FFu,
               g_sim_pc_group_profiles[i].instruction_count,
               g_sim_pc_group_profiles[i].total_cycles,
               grand_total ? 100.0 * (double)g_sim_pc_group_profiles[i].total_cycles /
                                 (double)grand_total
                           : 0.0,
               g_sim_pc_group_profiles[i].last_address);
        row++;
    }
    printf("================================================================\n\n");
}

int sc_sim_profiler_get_top_pc_groups(uint32_t *out_groups, uint32_t max_groups) {
    uint32_t count = 0;
    for (uint32_t i = 0; i < SC_SIM_PC_GROUP_COUNT; i++)
        if (g_sim_pc_group_profiles[i].total_cycles > 0) count++;
    uint32_t n = count < max_groups ? count : max_groups;
    for (uint32_t k = 0; k < n; k++) {
        uint32_t best = 0;
        uint64_t best_cycles = 0;
        int found = 0;
        for (uint32_t i = 0; i < SC_SIM_PC_GROUP_COUNT; i++) {
            uint64_t cycles = g_sim_pc_group_profiles[i].total_cycles;
            int already = 0;
            if (cycles == 0) continue;
            for (uint32_t m = 0; m < k; m++)
                if (out_groups[m] == i) { already = 1; break; }
            if (already) continue;
            if (!found || cycles > best_cycles) {
                best = i;
                best_cycles = cycles;
                found = 1;
            }
        }
        if (!found) return (int)k;
        out_groups[k] = best;
    }
    return (int)n;
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
    for (uint32_t i = 0; i + 1u < count; i++) {
        for (uint32_t j = i + 1u; j < count; j++) {
            if (pairs[j].cycles > pairs[i].cycles) {
                Pair tmp = pairs[i]; pairs[i] = pairs[j]; pairs[j] = tmp;
            }
        }
    }
    uint32_t n = count < max_groups ? count : max_groups;
    for (uint32_t i = 0; i < n; i++) out_groups[i] = pairs[i].id;
    return n;
}