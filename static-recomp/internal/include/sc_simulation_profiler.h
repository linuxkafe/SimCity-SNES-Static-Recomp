/*
 * sc_simulation_profiler.c — Profile v35 town route groups to identify
 * simulation hot paths for native reimplementation.
 *
 * Compiles into headless-test when SIMCITY_PROFILE_SIMULATION=1
 */

#ifndef SC_SIMULATION_PROFILER_H
#define SC_SIMULATION_PROFILER_H

#include <stdint.h>

/* Max group_id returned by group_id_from_address() is 0x60 (0x51+0x0F);
 * 0xFF is the out-of-range sentinel and is filtered by the record guards. */
#define SC_V35_TOWN_ROUTE_GROUP_COUNT 0x61

typedef struct {
    uint64_t call_count;
    uint64_t total_cycles;
    uint32_t min_cycles;
    uint32_t max_cycles;
    uint32_t last_address;
} ScSimGroupProfile;

extern ScSimGroupProfile g_sim_group_profiles[SC_V35_TOWN_ROUTE_GROUP_COUNT];
extern uint8_t g_sim_profiling_enabled;

/* Maps a v35 town-route address (group index in bits 10+) to a dense
 * profile slot. Returns 0xFF (out of range) for unhandled addresses.
 * Declared here because generated/sc_v35_town_route_index.c calls it. */
uint32_t group_id_from_address(uint32_t address);

void sc_sim_profiler_init(void);
void sc_sim_profiler_record_entry(uint32_t group_id, uint32_t address);
void sc_sim_profiler_record_exit(uint32_t group_id, uint32_t cycles);
void sc_sim_profiler_dump(void);
int sc_sim_profiler_get_top_groups(uint32_t *out_groups, uint32_t max_groups);

#endif