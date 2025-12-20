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

/**
 * Get a snapshot from a scenario using its id.
 * @param scenario The scenario to get the snapshot from.
 * @param snapshot_id The id of the snapshot to get.
 * @param snapshot A pointer to where a pointer to the snapshot can be stored.
 * @throws FAILURE if id not in snapshot list.
 */
exception tekScenarioGetSnapshot(const TekScenario* scenario, const uint snapshot_id, TekBodySnapshot** snapshot) {
    // loop over the snapshots
    // linear search...
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

/**
 * Get a snapshot id/data by using the index at which its name appears in the name list. Very useful in TekGuiList callbacks when the user clicks the name of an object they want and you want the associated snapshot ¬_,¬
 * @param scenario The scenario to get the snapshot from.
 * @param name_index The index of the name in the name list.
 * @param snapshot The snapshot to get, provide NULL if unwanted.
 * @param snapshot_id The id to get, provide NULL if unwanted
 */
void tekScenarioGetByNameIndex(const TekScenario* scenario, const uint name_index, TekBodySnapshot** snapshot, int* snapshot_id) {
    // if name index out of range, return invalid
    if (name_index >= scenario->names.length) {
        if (snapshot) *snapshot = NULL;
        if (snapshot_id) *snapshot_id = -1;
    }

    // iterate over the list of names until reaching name index
    // this is so we can get the ListItem pointer, will match what the scenario pair points to
    // cannot access directly due to being a linked list
    const ListItem* item;
    uint index = 0;
    foreach(item, (&scenario->names), {
        if (index == name_index)
            break;
        index++;
    });

    // find the scenario pair that has a matching list item pointer as what we just found
    const ListItem* search_item;
    foreach(search_item, (&scenario->snapshots), {
        const struct TekScenarioPair* pair = search_item->data;
        if (pair->list_ptr == item) {
            if (snapshot) *snapshot = pair->snapshot;
            if (snapshot_id) *snapshot_id = (int)pair->id;
            return;
        }
    });

    // if no match, mark as being invalid.
    if (snapshot) *snapshot = NULL;
    if (snapshot_id) *snapshot_id = -1;
}

/**
 * Get the name of a snapshot using its id.
 * @param scenario The scenario from which to get the name
 * @param snapshot_id The id of the snapshot for which to get the name
 * @param snapshot_name A pointer to where a pointer to the buffer can be put
 * @note The name is a direct pointer into the name list, so don't edit unless you want changes to be reflected visually.
 * @throws FAILURE if id not in snapshot list. 
 */
exception tekScenarioGetName(const TekScenario* scenario, const uint snapshot_id, char** snapshot_name) {
    // gotta loop over the list of items.
    const ListItem* item;
    foreach(item, (&scenario->snapshots), {
        const struct TekScenarioPair* pair = item->data;
        if (pair->id == snapshot_id) {
            // found the name, so return pointer
            *snapshot_name = pair->list_ptr->data;
            return SUCCESS;
        }
    });
    // if made it to the end of the list without returning, then not found
    tekThrow(FAILURE, "ID not in snapshot list");
}

/**
 * Set the name of a snapshot in a scenario by id. Will copy the name into a new buffer.
 * @param scenario The scenario for which to set a name for. 
 * @param snapshot_id The id of the snapshot for which to update the name
 * @param snapshot_name The new name of the snapshot.
 * @throws MEMORY_EXCEPTION if malloc() explodes
 * @throws FAILURE if id is not in snapshot list
 */
exception tekScenarioSetName(const TekScenario* scenario, const uint snapshot_id, const char* snapshot_name) {
    // need to iterate over each snapshot to find the one associated with the specified id
    const ListItem* item;
    foreach(item, (&scenario->snapshots), {
        const struct TekScenarioPair* pair = item->data;
        if (pair->id == snapshot_id) {
            // got it
            // create a buffer that's big enough to fit the new name
            const uint len_name = strlen(snapshot_name) + 1;
            char* new_name = realloc(pair->list_ptr->data, len_name * sizeof(char));
            if (!new_name)
                tekThrow(MEMORY_EXCEPTION, "Failed to reallocate memory for name.");

            // copy in and update the name list
            memcpy(new_name, snapshot_name, len_name);
            pair->list_ptr->data = new_name;

            return SUCCESS;
        }
    });
    tekThrow(FAILURE, "ID not in snapshot list");
}

/**
 * Get the next available ID in the scenario. Will first check if any ids have been made available by removing old items, else will return the lowest fresh id.
 * @param scenario The scenario for which to get the next available id.
 * @param next_id A pointer to an unsigned integer which will have the id written to it.
 * @throws QUEUE_EXCEPTION if something horrible happens :P
 */
exception tekScenarioGetNextId(TekScenario* scenario, uint* next_id) {
    // check if there is any unused ids. if not, we can just boom out the length of the snapshots
    if (queueIsEmpty(&scenario->unused_ids)) {
        *next_id = scenario->snapshots.length;
        return SUCCESS;
    }

    // if non empty, then we dequeue the unused id queue to get an id to use.
    void* next_id_ptr;
    tekChainThrow(queueDequeue(&scenario->unused_ids, &next_id_ptr));
    *next_id = (uint)next_id_ptr;

    return SUCCESS;
}

/**
 * Create a scenario pair, this includes the data needed for each snapshot such as the name, id and properties.
 * @param scenario The scenario that the scenario pair is associated with, where the name should be stored.
 * @param copy_snapshot A body snapshot containing data to copy.
 * @param snapshot_id The id of the snapshot being created 
 * @param snapshot_name The name of the snapshot being created.
 * @param pair A pointer to the pair which will be created.
 * @throws MEMORY_EXCEPTION if malloc() fails.
 */
static exception tekScenarioCreatePair(TekScenario* scenario, const TekBodySnapshot* copy_snapshot, const uint snapshot_id, const char* snapshot_name, struct TekScenarioPair** pair) {
    // attempt to allocate memory for the new pair
    *pair = malloc(sizeof(struct TekScenarioPair));
    if (!*pair)
        tekThrow(MEMORY_EXCEPTION, "Failed to allocate snapshot pair.");

    // now attempt to allocate memory for the body
    (*pair)->snapshot = (TekBodySnapshot*)malloc(sizeof(TekBodySnapshot));
    if (!(*pair)->snapshot) {
        free(*pair);
        tekThrow(MEMORY_EXCEPTION, "Failed to allocate snapshot.");
    }

    // now attempt to allocate memory for the name
    const uint len_name = strlen(snapshot_name) + 1;
    char* name = malloc(len_name * sizeof(char));
    if (!name) {
        free(*pair);
        free((*pair)->snapshot);
        tekThrow(MEMORY_EXCEPTION, "Failed to allocate snapshot name.");
    }
    memcpy(name, snapshot_name, len_name);

    // once everything is allocated, attempt to add the pair to the list of snapshots
    tekChainThrowThen(listInsertItem(&scenario->names, scenario->names.length - 1, name), {
        free(*pair);
        free((*pair)->snapshot);
        free(name);
    });
    // after this, no more exceptions can occur, so we can start filling in the data

    // set the list_ptr of the pair as the newly added name
    ListItem* list_ptr = scenario->names.data;
    while (list_ptr->next) {
        if (!list_ptr->next->next) break;
        list_ptr = list_ptr->next;
    }
    (*pair)->list_ptr = list_ptr;

    // copy the snapshot data into the new buffer
    memcpy((*pair)->snapshot, copy_snapshot, sizeof(TekBodySnapshot));
    (*pair)->id = snapshot_id;

    return SUCCESS;
}

/**
 * Delete a scenario pair, free allocated memory and the pair.
 * @param pair The pair to delete.
 */
static void tekScenarioDeletePair(struct TekScenarioPair* pair) {
    // free the snapshot data first, or else we lose it.
    free(pair->snapshot);
    free(pair);
}

/**
 * Add a new snapshot or update an existing snapshot with a specified id. Will update the name and data associated with the id.
 * @param scenario The scenario that should be added to
 * @param copy_snapshot The body snapshot to copy into the scenario
 * @param snapshot_id The id that should be associated with the snapshot
 * @param snapshot_name The name of the snapshot being added.
 * @throws MEMORY_EXCEPTION if malloc() fails.
 */
exception tekScenarioPutSnapshot(TekScenario* scenario, const TekBodySnapshot* copy_snapshot, const uint snapshot_id, const char* snapshot_name) {
    // first, check if a snapshot with the id already exists.
    // need to check each id
    const ListItem* item;
    foreach(item, (&scenario->snapshots), {
        struct TekScenarioPair* pair = item->data;
        if (pair->id == snapshot_id) {
            // if we find one with an existing id, need to update it
            // first, copy the data we want
            memcpy(pair->snapshot, copy_snapshot, sizeof(struct TekBodySnapshot));

            // now, copy the new name into the name list
            // need to reallocate cuz length might be different.
            const uint len_name = strlen(snapshot_name) + 1;
            char* new_name = realloc(pair->list_ptr->data, len_name * sizeof(char));
            if (!new_name)
                tekThrow(MEMORY_EXCEPTION, "Failed to reallocate memory for name.");

            // copy new name into list
            memcpy(new_name, snapshot_name, len_name);
            pair->list_ptr->data = new_name;

            return SUCCESS;
        }
    });

    // if the id doesn't exist, create a new snapshot with the data.
    struct TekScenarioPair* new_pair;
    tekChainThrow(tekScenarioCreatePair(scenario, copy_snapshot, snapshot_id, snapshot_name, &new_pair));
    tekChainThrowThen(listAddItem(&scenario->snapshots, new_pair), {
        tekScenarioDeletePair(new_pair);
    });

    return SUCCESS;
}

/**
 * Delete a snapshot, freeing any allocated memory of the struct.
 * @param scenario The scenario to delete
 * @param snapshot_id The id of the snapshot to delete
 * @throws FAILURE if id does not exist.
 */
exception tekScenarioDeleteSnapshot(TekScenario* scenario, const uint snapshot_id) {
    // first thing to do is remove scenario from list of scenarios.
    // locate the scenario using its id.
    const ListItem* item;
    const ListItem* list_ptr = 0;
    uint index = 0;
    flag found = 0;
    foreach(item, (&scenario->snapshots), {
        struct TekScenarioPair* pair = item->data;
        if (pair->id == snapshot_id) {
            // if id matches, remove snapshot from list and mark this id as unused.
            // allows us to fill gaps of ids when they're removed
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

    // the other thing to do is remove it from the list of names
    // list_ptr should have been found in the last loop.
    // list_ptr points to the name list where the name is stored.
    // so loop until we stumble across the list_ptr and then remove it.
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

    // finally, remove the name
    tekChainThrow(listRemoveItem(&scenario->names, index, NULL));
    return SUCCESS;
}

/**
 * Create a new scenario, allocates some memory for different internal structures.
 * @param scenario The scenario to create.
 * @throws MEMORY_EXCEPTION if malloc() fails
 */
exception tekCreateScenario(TekScenario* scenario) {
    // initialise some internal structures
    listCreate(&scenario->snapshots);
    listCreate(&scenario->names);
    queueCreate(&scenario->unused_ids);

    // add the button to add a new object
    // shouldn't really be done here. ¯\_(ツ)_/¯
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

/**
 * Scan a single snapshot from a buffer. Expects the same format as specified by SNAPSHOT_READ_FORMAT - key value pairs of snapshot data seperated by new lines.
 * @param string The input buffer to scan from
 * @param snapshot The snapshot to write into.
 * @param snapshot_id An unsigned int to write the snapshot id into.
 * @param snapshot_name An allocated char buffer to write the name of the snapshot into.
 * @return Number of items successfully scanned.
 */
static int tekScanSnapshot(const char* string, TekBodySnapshot* snapshot, uint* snapshot_id, char* snapshot_name) {
    // wrapper around sscanf
    // just uses SNAPSHOT_READ_FORMAT for consistency.
    // also helps cuz you dont have to access the elemets of snapshot every time
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

/**
 * Read a scenario from a specified file, and load it into a scenario struct.
 * @param scenario_filepath The path of the file containing the scenario data.
 * @param scenario A pointer to a scenario to fill with the data in the file.
 * @throws MEMORY_EXCEPTION if malloc() fails.
 */
exception tekReadScenario(const char* scenario_filepath, TekScenario* scenario) {
    // initialise the scenario structure, initialise some internals.
    tekChainThrow(tekCreateScenario(scenario));

    // get the size of the file and attempt to write buffer with the data
    uint len_file;
    tekChainThrow(getFileSize(scenario_filepath, &len_file));
    char* file = (char*)malloc(len_file * sizeof(char));
    if (!file)
        tekThrow(MEMORY_EXCEPTION, "Failed to allocate memory to read file into.");
    tekChainThrow(readFile(scenario_filepath, len_file, file));

    // iterate over the file, looking for new lines.
    // SNAPSHOT_NUM_LINES is the number of lines per snapshot, so every "that many" lines we need to add a new object
    // every new line signals that a new piece of data is added to the current object
    // this is mostly handled by the tekScanSnapshot / scanf methods tho
    char* c = file;
    char* scenario_start = file;
    uint line_number = 0;
    while (*c) {
        // count new lines
        if (*c == '\n')
            line_number++;

        // once reaching magic number of  newlines, scan the last bit of file into a snapshot
        if (*c == '\n' && line_number % SNAPSHOT_NUM_LINES == 0) {
            // build a new snapshot, need some space to write stuff into
            TekBodySnapshot snapshot = {};
            uint snapshot_id = 0;
            // absolute bodge, i have no idea how to find the length of the name before scanning :)
            // ples dont name anything using more than 256 chars
            char snapshot_name[256];
            snapshot.model = malloc(256 * sizeof(char));
            snapshot.material = malloc(256 * sizeof(char));
            // now scan and write snapshot
            if (tekScanSnapshot(scenario_start, &snapshot, &snapshot_id, snapshot_name) < 0) {
                free(file);
                tekThrow(FAILURE, "Failed to read snapshot file.");
            }
            tekChainThrowThen(tekScenarioPutSnapshot(scenario, &snapshot, snapshot_id, snapshot_name), {
                free(file);
            });
            // go to next char in buffer
            scenario_start = c + 1;
        }

        c++;
    }

    return SUCCESS;
}

/**
 * Write a single snapshot of a body to a buffer. Also returns the number of bytes written in the buffer.
 * @param string A pointer to the buffer where the snapshot should be written.
 * @param max_length The maximum number of bytes that can be written (length of buffer?)
 * @param snapshot A pointer to the snapshot to be written.
 * @param snapshot_id The id of the snapshot.
 * @param snapshot_name The name of the snapshot
 * @return The number of bytes written.
 */
static int tekWriteSnapshot(char* string, size_t max_length, const TekBodySnapshot* snapshot, const uint snapshot_id, const char* snapshot_name) {
    // wrapper around snprintf, just reduces chance of error when reusing this func.
    // also, allows formatting to be changed more easily.
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
    //
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
    // allocate a buffer big enough to fit all the ids of this scenario
    // will equal the number of objects in the scenario, hence the use of length.
    const uint num_snapshots = scenario->snapshots.length;
    *num_ids = num_snapshots;
    *ids = (uint*)malloc(num_snapshots * sizeof(uint));
    if (!*ids)
        tekThrow(MEMORY_EXCEPTION, "Failed to allocate memory for id list.");

    // loop through "scenario pairs", which contain the id
    const ListItem* item;
    uint index = 0;
    foreach(item, (&scenario->snapshots), {
        // for each pair, get the id and add it to the array
        const struct TekScenarioPair* pair = item->data;
        (*ids)[index] = pair->id;
        index++;
    });

    return SUCCESS;
}

/**
 * Write the data stored in a scenario to a specified filepath.
 * @param scenario The scenario which should have its data written.
 * @param scenario_filepath The path of the file to write the scenario to.
 * @throws FILE_EXCEPTION if the file does not exist or cannot be written.
 * @throws MEMORY_EXCEPTION if malloc() fails.
 */
exception tekWriteScenario(const TekScenario* scenario, const char* scenario_filepath) {
    // for each object in the scenario, calculate the number of bytes needed to write it.
    const ListItem* item;
    int len_buffer = 0;
    foreach(item, (&scenario->snapshots), {
        // writing to NULL, max length 0 -> get the number of characters needed
        const struct TekScenarioPair* pair = (struct TekScenarioPair*)item->data;
        len_buffer += tekWriteSnapshot(NULL, 0, pair->snapshot, pair->id, pair->list_ptr->data);
    });

    // null-terminated string, so need to add the last character (=0)
    len_buffer += 1;

    // mallocate enough memory to write to
    char* buffer = (char*)malloc(len_buffer * sizeof(char));
    if (!buffer)
        tekThrow(MEMORY_EXCEPTION, "Failed to allocate memory for scenario file.");

    // now loop back over, writing to the buffer.
    // increment the write pointer after each item, so we dont just overwrite the first item over and over.
    int write_ptr = 0;
    uint id = 0;
    foreach(item, (&scenario->snapshots), {
        const struct TekScenarioPair* pair = (struct TekScenarioPair*)item->data;
        printf("[[[\n%s\n]]]\n", (char*)pair->list_ptr->data);
        write_ptr += tekWriteSnapshot(buffer + write_ptr, len_buffer - write_ptr, pair->snapshot, id++, pair->list_ptr->data);
    });

    // finally, write this buffer to the file.
    tekChainThrow(writeFile(buffer, scenario_filepath));

    free(buffer);
    return SUCCESS;
}

/**
 * Delete a scenario, removing any allocated memory of the struct.
 * @param scenario The scenario to delete
 */
void tekDeleteScenario(TekScenario* scenario) {
    // iterate over objects in scenario and free them.
    ListItem* item;
    foreach(item, (&scenario->snapshots), {
        struct TekScenarioPair* pair = item->data;
        tekScenarioDeletePair(pair);
    });

    // now free the list that stored the objects
    // and free helper structures
    listDelete(&scenario->snapshots);
    listFreeAllData(&scenario->names);
    listDelete(&scenario->names);
    queueDelete(&scenario->unused_ids);
}
