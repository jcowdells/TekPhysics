#include "scenario.h"

#include <stdio.h>
#include <string.h>
#include <linux/limits.h>

#include "../core/file.h"

#define SNAPSHOT_WRITE_FORMAT "ID:%u\nNAME:%s\nPOSITION:%f %f %f\nROTATION:%f %f %f %f\nVELOCITY:%f %f %f\nMASS:%f\nCOEF_FRICTION:%f\nCOEF_RESTITUTION:%f\nIMMOVABLE:%d\nMODEL:%s\nMATERIAL:%s\n"
#define SNAPSHOT_READ_FORMAT  "ID:%u\nNAME:%[^\n]\nPOSITION:%f %f %f\nROTATION:%f %f %f %f\nVELOCITY:%f %f %f\nMASS:%f\nCOEF_FRICTION:%f\nCOEF_RESTITUTION:%f\nIMMOVABLE:%d\nMODEL:%[^\n]\nMATERIAL:%[^\n]\n"
#define SNAPSHOT_NUM_LINES 11

struct TekScenarioPair {
    TekBodySnapshot* snapshot;
    ListItem* list_ptr;
    uint id;
};

exception tekScenarioGetSnapshot(const TekScenario* scenario, const uint snapshot_id, TekBodySnapshot** snapshot) {
    const ListItem* item;
    foreach(item, (&scenario->snapshots), {
        const struct TekScenarioPair* pair = item->data;
        if (pair->id == snapshot_id) {
            *snapshot = pair->snapshot;
            return SUCCESS;
        }
    });
    tekThrow(FAILURE, "ID not in snapshot list");
}

void tekScenarioGetByNameIndex(const TekScenario* scenario, const uint name_index, TekBodySnapshot** snapshot, int* snapshot_id) {
    if (name_index >= scenario->names.length) {
        if (snapshot) *snapshot = NULL;
        if (snapshot_id) *snapshot_id = -1;
    }

    const ListItem* item;
    uint index = 0;
    foreach(item, (&scenario->names), {
        if (index == name_index)
            break;
        index++;
    });

    const ListItem* search_item;
    foreach(search_item, (&scenario->snapshots), {
        const struct TekScenarioPair* pair = search_item->data;
        if (pair->list_ptr == item) {
            if (snapshot) *snapshot = pair->snapshot;
            if (snapshot_id) *snapshot_id = (int)pair->id;
            return;
        }
    });

    if (snapshot) *snapshot = NULL;
    if (snapshot_id) *snapshot_id = -1;
}

exception tekScenarioGetName(const TekScenario* scenario, const uint snapshot_id, char** snapshot_name) {
    const ListItem* item;
    foreach(item, (&scenario->snapshots), {
        const struct TekScenarioPair* pair = item->data;
        if (pair->id == snapshot_id) {
            *snapshot_name = pair->list_ptr->data;
            return SUCCESS;
        }
    });
    tekThrow(FAILURE, "ID not in snapshot list");
}

exception tekScenarioSetName(const TekScenario* scenario, const uint snapshot_id, const char* snapshot_name) {
    const ListItem* item;
    foreach(item, (&scenario->snapshots), {
        const struct TekScenarioPair* pair = item->data;
        if (pair->id == snapshot_id) {
            const uint len_name = strlen(snapshot_name) + 1;
            char* new_name = realloc(pair->list_ptr->data, len_name * sizeof(char));
            if (!new_name)
                tekThrow(MEMORY_EXCEPTION, "Failed to reallocate memory for name.");
            memcpy(new_name, snapshot_name, len_name);
            pair->list_ptr->data = new_name;

            return SUCCESS;
        }
    });
    tekThrow(FAILURE, "ID not in snapshot list");
}

exception tekScenarioGetNextId(TekScenario* scenario, uint* next_id) {
    if (queueIsEmpty(&scenario->unused_ids)) {
        *next_id = scenario->snapshots.length;
        return SUCCESS;
    }

    void* next_id_ptr;
    tekChainThrow(queueDequeue(&scenario->unused_ids, &next_id_ptr));
    *next_id = (uint)next_id_ptr;

    return SUCCESS;
}

static exception tekScenarioCreatePair(TekScenario* scenario, const TekBodySnapshot* copy_snapshot, const uint snapshot_id, const char* snapshot_name, struct TekScenarioPair** pair) {
    *pair = malloc(sizeof(struct TekScenarioPair));
    if (!*pair)
        tekThrow(MEMORY_EXCEPTION, "Failed to allocate snapshot pair.");

    (*pair)->snapshot = (TekBodySnapshot*)malloc(sizeof(TekBodySnapshot));
    if (!(*pair)->snapshot) {
        free(*pair);
        tekThrow(MEMORY_EXCEPTION, "Failed to allocate snapshot.");
    }

    const uint len_name = strlen(snapshot_name) + 1;
    char* name = malloc(len_name * sizeof(char));
    if (!name) {
        free(*pair);
        free((*pair)->snapshot);
        tekThrow(MEMORY_EXCEPTION, "Failed to allocate snapshot name.");
    }
    memcpy(name, snapshot_name, len_name);

    tekChainThrowThen(listInsertItem(&scenario->names, scenario->names.length - 1, name), {
        free(*pair);
        free((*pair)->snapshot);
        free(name);
    });

    ListItem* list_ptr = scenario->names.data;
    while (list_ptr->next) {
        if (!list_ptr->next->next) break;
        list_ptr = list_ptr->next;
    }
    (*pair)->list_ptr = list_ptr;

    memcpy((*pair)->snapshot, copy_snapshot, sizeof(TekBodySnapshot));
    (*pair)->id = snapshot_id;

    return SUCCESS;
}

static void tekScenarioDeletePair(struct TekScenarioPair* pair) {
    free(pair->snapshot);
    free(pair);
}

exception tekScenarioPutSnapshot(TekScenario* scenario, const TekBodySnapshot* copy_snapshot, const uint snapshot_id, const char* snapshot_name) {
    const ListItem* item;
    foreach(item, (&scenario->snapshots), {
        struct TekScenarioPair* pair = item->data;
        if (pair->id == snapshot_id) {
            memcpy(pair->snapshot, copy_snapshot, sizeof(struct TekScenarioPair));

            const uint len_name = strlen(snapshot_name) + 1;
            char* new_name = realloc(pair->list_ptr->data, len_name * sizeof(char));
            if (!new_name)
                tekThrow(MEMORY_EXCEPTION, "Failed to reallocate memory for name.");
            memcpy(new_name, snapshot_name, len_name);
            pair->list_ptr->data = new_name;

            return SUCCESS;
        }
    });

    struct TekScenarioPair* new_pair;
    tekChainThrow(tekScenarioCreatePair(scenario, copy_snapshot, snapshot_id, snapshot_name, &new_pair));
    tekChainThrowThen(listAddItem(&scenario->snapshots, new_pair), {
        tekScenarioDeletePair(new_pair);
    });

    return SUCCESS;
}

exception tekScenarioDeleteSnapshot(TekScenario* scenario, const uint snapshot_id) {
    const ListItem* item;
    const ListItem* list_ptr = 0;
    uint index = 0;
    flag found = 0;
    foreach(item, (&scenario->snapshots), {
        struct TekScenarioPair* pair = item->data;
        if (pair->id == snapshot_id) {
            tekChainThrow(listRemoveItem(&scenario->snapshots, index, NULL));
            tekChainThrow(queueEnqueue(&scenario->unused_ids, (void*)snapshot_id));
            list_ptr = pair->list_ptr;
            free(pair);
            found = 1;
            break;
        }

        index++;
    });

    if (!found) tekThrow(FAILURE, "ID not in snapshot list");

    index = 0;
    found = 0;
    foreach(item, (&scenario->names), {
        if (item == list_ptr) {
            found = 1;
            break;
        }
        index++;
    });

    if (!found)
        return SUCCESS;

    tekChainThrow(listRemoveItem(&scenario->names, index, NULL));

    if (!found) tekThrow(FAILURE, "ID not in snapshot list");
    return SUCCESS;
}

exception tekCreateScenario(TekScenario* scenario) {
    listCreate(&scenario->snapshots);
    listCreate(&scenario->names);
    queueCreate(&scenario->unused_ids);

    const char* new_object_string = "Add New Object";
    const uint len_new_object_string = strlen(new_object_string) + 1;
    char* new_object_buffer = (char*)malloc(sizeof(char) * len_new_object_string);
    if (!new_object_buffer)
        tekThrow(MEMORY_EXCEPTION, "Failed to allocate buffer for new object string.");
    memcpy(new_object_buffer, new_object_string, len_new_object_string);
    tekChainThrowThen(listAddItem(&scenario->names, new_object_buffer), {
        free(new_object_buffer);
    });

    return SUCCESS;
}

static int tekScanSnapshot(const char* string, TekBodySnapshot* snapshot, uint* snapshot_id, char* snapshot_name) {
    return sscanf(
        string,
        SNAPSHOT_READ_FORMAT,
        snapshot_id,
        snapshot_name,
        &snapshot->position[0], &snapshot->position[1], &snapshot->position[2],
        &snapshot->rotation[0], &snapshot->rotation[1], &snapshot->rotation[2], &snapshot->rotation[3],
        &snapshot->velocity[0], &snapshot->velocity[1], &snapshot->velocity[2],
        &snapshot->mass,
        &snapshot->friction,
        &snapshot->restitution,
        &snapshot->immovable,
        snapshot->model,
        snapshot->material
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
            char snapshot_name[256];
            snapshot.model = malloc(256 * sizeof(char));
            snapshot.material = malloc(256 * sizeof(char));
            if (tekScanSnapshot(scenario_start, &snapshot, &snapshot_id, snapshot_name) < 0) {
                free(file);
                tekThrow(FAILURE, "Failed to read snapshot file.");
            }
            tekChainThrowThen(tekScenarioPutSnapshot(scenario, &snapshot, snapshot_id, snapshot_name), {
                free(file);
            });
            scenario_start = c + 1;
        }

        c++;
    }

    return SUCCESS;
}

static int tekWriteSnapshot(char* string, size_t max_length, const TekBodySnapshot* snapshot, const uint snapshot_id, const char* snapshot_name) {
    return snprintf(
        string, max_length,
        SNAPSHOT_WRITE_FORMAT,
        snapshot_id,
        snapshot_name,
        EXPAND_VEC3(snapshot->position),
        EXPAND_VEC4(snapshot->rotation),
        EXPAND_VEC3(snapshot->velocity),
        snapshot->mass,
        snapshot->friction,
        snapshot->restitution,
        snapshot->immovable,
        snapshot->model,
        snapshot->material
    );
}

/**
 * Allocate a buffer containing the ids of all snapshots in the scenario.
 * @param scenario The scenario to generate this buffer for.
 * @param ids A pointer to where a pointer to the newly created buffer will be written.
 * @param num_ids A pointer to where the number of ids in the new buffer will be written.
 * @throws MEMORY_EXCEPTION if malloc() fails.
 * @note This function allocates memory which you are responsible for freeing.
 */
exception tekScenarioGetAllIds(const TekScenario* scenario, uint** ids, uint* num_ids) {
    const uint num_snapshots = scenario->snapshots.length;
    *num_ids = num_snapshots;
    *ids = (uint*)malloc(num_snapshots * sizeof(uint));
    if (!*ids)
        tekThrow(MEMORY_EXCEPTION, "Failed to allocate memory for id list.");

    const ListItem* item;
    uint index = 0;
    foreach(item, (&scenario->snapshots), {
        const struct TekScenarioPair* pair = item->data;
        (*ids)[index] = pair->id;
        index++;
    });

    return SUCCESS;
}

exception tekWriteScenario(const TekScenario* scenario, const char* scenario_filepath) {
    const ListItem* item;
    int len_buffer = 0;
    foreach(item, (&scenario->snapshots), {
        const struct TekScenarioPair* pair = (struct TekScenarioPair*)item->data;
        len_buffer += tekWriteSnapshot(NULL, 0, pair->snapshot, pair->id, pair->list_ptr->data);
    });

    // null-terminated string, so need to add the last character (=0)
    len_buffer += 1;

    char* buffer = (char*)malloc(len_buffer * sizeof(char));
    if (!buffer)
        tekThrow(MEMORY_EXCEPTION, "Failed to allocate memory for scenario file.");

    int write_ptr = 0;
    uint id = 0;
    foreach(item, (&scenario->snapshots), {
        const struct TekScenarioPair* pair = (struct TekScenarioPair*)item->data;
        printf("[[[\n%s\n]]]\n", (char*)pair->list_ptr->data);
        write_ptr += tekWriteSnapshot(buffer + write_ptr, len_buffer - write_ptr, pair->snapshot, id++, pair->list_ptr->data);
    });

    tekChainThrow(writeFile(buffer, scenario_filepath));

    free(buffer);
    return SUCCESS;
}

void tekDeleteScenario(TekScenario* scenario) {
    ListItem* item;
    foreach(item, (&scenario->snapshots), {
        struct TekScenarioPair* pair = item->data;
        tekScenarioDeletePair(pair);
    });
    listDelete(&scenario->snapshots);
    listFreeAllData(&scenario->names);
    listDelete(&scenario->names);
    queueDelete(&scenario->unused_ids);
}