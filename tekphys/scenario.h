#pragma once

#include "body.h"
#include "../tekgl.h"
#include "../core/exception.h"
#include "../core/list.h"
#include "../core/queue.h"

typedef struct TekScenario {
    List snapshots;
    List names;
    Queue unused_ids;
    // TODO: think of some constants of this scenario, probably just gravity.
} TekScenario;

exception tekCreateScenario(TekScenario* scenario);
exception tekReadScenario(const char* scenario_filename, TekScenario* scenario);
exception tekWriteScenario(const TekScenario* scenario, const char* scenario_filename);

exception tekScenarioGetSnapshot(const TekScenario* scenario, uint snapshot_id, TekBodySnapshot** snapshot);
void tekScenarioGetByNameIndex(const TekScenario* scenario, uint name_index, TekBodySnapshot** snapshot, int* snapshot_id);

exception tekScenarioGetName(const TekScenario* scenario, uint snapshot_id, char** snapshot_name);
exception tekScenarioSetName(const TekScenario* scenario, uint snapshot_id, const char* snapshot_name);

exception tekScenarioPutSnapshot(TekScenario* scenario, const TekBodySnapshot* copy_snapshot, uint snapshot_id, const char* snapshot_name);
exception tekScenarioDeleteSnapshot(TekScenario* scenario, uint snapshot_id);
exception tekScenarioGetNextId(TekScenario* scenario, uint* next_id);
exception tekScenarioGetAllIds(const TekScenario* scenario, uint** ids, uint* num_ids);

void tekDeleteScenario(TekScenario* scenario);