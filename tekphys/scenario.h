#pragma once

#include "body.h"
#include "../tekgl.h"
#include "../core/exception.h"
#include "../core/list.h"

typedef struct TekScenario {
    List scenarios;
    // TODO: think of some constants of this scenario, probably just gravity.
} TekScenario;

exception tekCreateScenario(TekScenario* scenario);
exception tekReadScenario(const char* scenario_filename, TekScenario* scenario);
exception tekWriteScenario(const TekScenario* scenario, const char* scenario_filename);

exception tekScenarioGetSnapshot(const TekScenario* scenario, uint snapshot_id, TekBodySnapshot** snapshot);
exception tekScenarioPutSnapshot(TekScenario* scenario, TekBodySnapshot* snapshot, uint snapshot_id);
exception tekScenarioDeleteSnapshot(TekScenario* scenario, uint snapshot_id);

void tekDeleteScenario(TekScenario* scenario);