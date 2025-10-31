#include "scenario.h"

#include <stdio.h>
#include <string.h>

#include "../core/file.h"

#define SNAPSHOT_FORMAT "ID:%u\nPOSITION:%f %f %f\nROTATION:%f %f %f %f\nVELOCITY:%f %f %f\nMASS:%f\nCOEF_FRICTION:%f\nCOEF_RESTITUTION:%f\n"
#define SNAPSHOT_NUM_LINES 7

struct TekScenarioPair {
    TekBodySnapshot* snapshot;
    uint id;
    flag allocated;
};

exception tekScenarioGetSnapshot(const TekScenario* scenario, uint snapshot_id, TekBodySnapshot** snapshot) {
    const ListItem* item;
    foreach(item, (&scenario->scenarios), {
        const struct TekScenarioPair* pair = item->data;
        if (pair->id == snapshot_id) {
            *snapshot = pair->snapshot;
            return SUCCESS;
        }
    });
    tekThrow(FAILURE, "ID not in snapshot list");
}

static exception tekScenarioAllocateSnapshot(TekScenario* scenario, const TekBodySnapshot* copy_snapshot, const uint snapshot_id) {
    struct TekScenarioPair* pair = (struct TekScenarioPair*)malloc(sizeof(struct TekScenarioPair));
    if (!pair)
        tekThrow(MEMORY_EXCEPTION, "Failed to allocate snapshot pair.");

    pair->snapshot = (TekBodySnapshot*)malloc(sizeof(TekBodySnapshot));
    if (!pair->snapshot) {
        free(pair);
        tekThrow(MEMORY_EXCEPTION, "Failed to allocate snapshot.");
    }

    memcpy(pair->snapshot, copy_snapshot, sizeof(TekBodySnapshot));
    pair->id = snapshot_id;
    pair->allocated = 1;

    tekChainThrowThen(listAddItem(&scenario->scenarios, pair), {
        free(pair->snapshot);
        free(pair);
    });

    return SUCCESS;
}

exception tekScenarioPutSnapshot(TekScenario* scenario, TekBodySnapshot* snapshot, const uint snapshot_id) {
    const ListItem* item;
    foreach(item, (&scenario->scenarios), {
        struct TekScenarioPair* pair = item->data;
        if (pair->id == snapshot_id) {
            if (pair->allocated)
                free(pair->snapshot);
            pair->snapshot = snapshot;
            pair->allocated = 0;
            return SUCCESS;
        }
    });

    struct TekScenarioPair* new_pair = (struct TekScenarioPair*)malloc(sizeof(struct TekScenarioPair));
    if (!new_pair)
        tekThrow(MEMORY_EXCEPTION, "Failed to allocate memory for new scenario pair.");
    new_pair->snapshot = snapshot;
    new_pair->id = snapshot_id;
    tekChainThrow(listAddItem(&scenario->scenarios, new_pair));

    return SUCCESS;
}

exception tekScenarioDeleteSnapshot(TekScenario* scenario, uint snapshot_id) {
    const ListItem* item;
    uint index = 0;
    foreach(item, (&scenario->scenarios), {
        struct TekScenarioPair* pair = item->data;
        if (pair->id == snapshot_id) {
            tekChainThrow(listRemoveItem(&scenario->scenarios, index, NULL));
            free(pair);
            return SUCCESS;
        }

        index++;
    });
    tekThrow(FAILURE, "ID not in snapshot list");
}

exception tekCreateScenario(TekScenario* scenario) {
    listCreate(&scenario->scenarios);
    return SUCCESS;
}

static int tekScanSnapshot(const char* string, TekBodySnapshot* snapshot, uint* snapshot_id) {
    return sscanf(
        string,
        SNAPSHOT_FORMAT,
        snapshot_id,
        &snapshot->position[0], &snapshot->position[1], &snapshot->position[2],
        &snapshot->rotation[0], &snapshot->rotation[1], &snapshot->rotation[2], &snapshot->rotation[3],
        &snapshot->velocity[0], &snapshot->velocity[1], &snapshot->velocity[2],
        &snapshot->mass,
        &snapshot->friction,
        &snapshot->restitution
    );
}

exception tekReadScenario(const char* scenario_filepath, TekScenario* scenario) {
    tekChainThrow(tekCreateScenario(scenario));

    uint len_file;
    tekChainThrow(getFileSize(scenario_filepath, &len_file));
    char* file = (char*)malloc(len_file * sizeof(char));
    if (!file)
        tekThrow(MEMORY_EXCEPTION, "Failed to allocate memory to read file into.");
    tekChainThrow(readFile(scenario_filepath, len_file, file));

    char* c = file;
    char* scenario_start = file;
    uint line_number = 0;
    while (*c) {
        if (*c == '\n')
            line_number++;

        if (*c == '\n' && line_number % SNAPSHOT_NUM_LINES == 0) {
            TekBodySnapshot snapshot = {};
            uint snapshot_id = 0;
            if (tekScanSnapshot(scenario_start, &snapshot, &snapshot_id) < 0) {
                free(file);
                tekThrow(FAILURE, "Failed to read snapshot file.");
            }
            tekChainThrowThen(tekScenarioAllocateSnapshot(scenario, &snapshot, snapshot_id), {
                free(file);
            });
            scenario_start = c + 1;
        }

        c++;
    }

    return SUCCESS;
}

static int tekWriteSnapshot(char* string, size_t max_length, const TekBodySnapshot* snapshot, const uint snapshot_id) {
    return snprintf(
        string, max_length,
        SNAPSHOT_FORMAT,
        snapshot_id,
        EXPAND_VEC3(snapshot->position),
        EXPAND_VEC4(snapshot->rotation),
        EXPAND_VEC3(snapshot->velocity),
        snapshot->mass,
        snapshot->friction,
        snapshot->restitution
    );
}

exception tekWriteScenario(const TekScenario* scenario, const char* scenario_filepath) {
    const ListItem* item;
    int len_buffer = 0;
    foreach(item, (&scenario->scenarios), {
        const struct TekScenarioPair* pair = (struct TekScenarioPair*)item->data;
        len_buffer += tekWriteSnapshot(NULL, 0, pair->snapshot, pair->id);
    });

    // null-terminated string, so need to add the last character (=0)
    len_buffer += 1;

    char* buffer = (char*)malloc(len_buffer * sizeof(char));
    if (!buffer)
        tekThrow(MEMORY_EXCEPTION, "Failed to allocate memory for scenario file.");

    int write_ptr = 0;
    foreach(item, (&scenario->scenarios), {
        const struct TekScenarioPair* pair = (struct TekScenarioPair*)item->data;
        write_ptr += tekWriteSnapshot(buffer + write_ptr, len_buffer - write_ptr, pair->snapshot, pair->id);
    });

    tekChainThrow(writeFile(buffer, scenario_filepath));

    free(buffer);
    return SUCCESS;
}