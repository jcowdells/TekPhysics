// main.c
#include <stdio.h>
#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <cglm/mat4.h>
#include "core/exception.h"
#include "tekgl/mesh.h"
#include "tekgl/shader.h"
#include "tekgl/texture.h"
#include "tekgl/font.h"
#include "tekgl/text.h"

#include <cglm/cam.h>

#include "core/list.h"
#include "tekgl/manager.h"
#include "tekgui/primitives.h"
#include "core/yml.h"
#include "tekgl/material.h"

#include "tekgl/camera.h"

#include <time.h>
#include <math.h>
#include <unistd.h>

#include "core/bitset.h"
#include "core/file.h"
#include "core/priorityqueue.h"
#include "core/queue.h"
#include "core/threadqueue.h"
#include "core/vector.h"
#include "tekphys/engine.h"
#include "tekphys/body.h"
#include "tekphys/collider.h"
#include "tekphys/collisions.h"

#include "tekgui/tekgui.h"
#include "tekgui/window.h"
#include "tekgui/button.h"
#include "tekgui/list_window.h"
#include "tekgui/text_button.h"
#include "tekgui/text_input.h"
#include "tekgui/option_window.h"
#include "tekphys/scenario.h"

#define WINDOW_WIDTH  640
#define WINDOW_HEIGHT 480

#define SAVE_OPTION 0
#define LOAD_OPTION 1
#define RUN_OPTION  2
#define QUIT_OPTION 3
#define NUM_OPTIONS 4

#define MOUSE_SENSITIVITY 0.5f
#define MOVE_SPEED        2.0f

#define DEFAULT_RATE 100.0
#define DEFAULT_SPEED  1.0

struct TekGuiComponents {
    TekGuiTextButton start_button;
    TekGuiImage logo;
    TekText version_text;
    TekGuiListWindow hierarchy_window;
    TekGuiOptionWindow editor_window;
    TekGuiListWindow action_window;
    TekGuiOptionWindow save_window;
    TekGuiOptionWindow load_window;
    TekGuiOptionWindow runner_window;
};

flag mode = MODE_MAIN_MENU;
flag next_mode = -1;
int hierarchy_index = -1;

float width, height;
mat4 perp_projection;
ThreadQueue event_queue = {};

uint depth = 0;
char depth_message[64];
flag text_init = 0;
TekText depth_text = {};
TekBitmapFont depth_font = {};

double mouse_x = 0.0, mouse_y = 0.0;
float mouse_dx = 0.0f, mouse_dy = 0.0f;
flag mouse_moved = 0, mouse_right = 0;
flag w_pressed = 0, a_pressed = 0, s_pressed = 0, d_pressed = 0;
TekScenario active_scenario = {};

void tekMainFramebufferCallback(const int fb_width, const int fb_height) {
    width = (float)fb_width;
    height = (float)fb_height;
    glm_perspective(1.2f, width / height, 0.1f, 100.0f, perp_projection);
}

void tekMainKeyCallback(const int key, const int scancode, const int action, const int mods) {
    if (key == GLFW_KEY_W) {
        if (action == GLFW_RELEASE) w_pressed = 0;
        else w_pressed = 1;
    }
    if (key == GLFW_KEY_A) {
        if (action == GLFW_RELEASE) a_pressed = 0;
        else a_pressed = 1;
    }
    if (key == GLFW_KEY_S) {
        if (action == GLFW_RELEASE) s_pressed = 0;
        else s_pressed = 1;
    }
    if (key == GLFW_KEY_D) {
        if (action == GLFW_RELEASE) d_pressed = 0;
        else d_pressed = 1;
    }
}

void tekMainMousePosCallback(const double x, const double y) {
    if (mouse_moved) return;
    mouse_dx = (float)(x - mouse_x);
    mouse_dy = (float)(y - mouse_y);
    mouse_x = x;
    mouse_y = y;
    mouse_moved = 1;
}

void tekMainMouseButtonCallback(const int button, const int action, const int mods) {
    if (button == GLFW_MOUSE_BUTTON_RIGHT) {
        if (action == GLFW_PRESS) {
            mouse_right = 1;
        } else if (action == GLFW_RELEASE) {
            mouse_right = 0;
        }
    }
}

static exception tekBodyCreateEvent(const TekBodySnapshot* snapshot, const int snapshot_id) {
    TekEvent event = {};
    event.type = BODY_CREATE_EVENT;
    memcpy(&event.data.body.snapshot, snapshot, sizeof(TekBodySnapshot));
    event.data.body.id = snapshot_id;
    tekChainThrow(pushEvent(&event_queue, event));
    return SUCCESS;
}

static exception tekCreateBodySnapshot(TekScenario* scenario, int* snapshot_id) {
    TekBodySnapshot dummy = {};
    dummy.mass = 1.0f;
    dummy.friction = 0.5f;
    dummy.restitution = 0.5f;
    glm_vec3_zero(dummy.position);
    glm_vec3_zero(dummy.rotation);
    glm_vec3_zero(dummy.velocity);
    glm_vec3_zero(dummy.angular_velocity);
    dummy.immovable = 0;

    const char* default_model = "../res/rad1.tmsh";
    const uint len_default_model = strlen(default_model) + 1;
    const char* default_material = "../res/material.tmat";
    const uint len_default_material = strlen(default_material) + 1;

    dummy.model = malloc(len_default_model * sizeof(char));
    if (!dummy.model)
        tekThrow(MEMORY_EXCEPTION, "Failed to allocate memory for model filepath.");
    dummy.material = malloc(len_default_material * sizeof(char));
    if (!dummy.material) {
        free(dummy.model);
        tekThrow(MEMORY_EXCEPTION, "Failed to allocate memory for material filepath");
    }

    memcpy(dummy.model, default_model, len_default_model);
    memcpy(dummy.material, default_material, len_default_material);

    uint id;
    tekChainThrowThen(tekScenarioGetNextId(scenario, &id), {
        free(dummy.model);
        free(dummy.material);
    });
    tekChainThrowThen(tekScenarioPutSnapshot(scenario, &dummy, id, "New Object"), {
        free(dummy.model);
        free(dummy.material);
    });
    *snapshot_id = (int)id;

    tekChainThrowThen(tekBodyCreateEvent(&dummy, id), {
        free(dummy.model);
        free(dummy.material);
    });

    return SUCCESS;
}

static exception tekUpdateBodySnapshot(const TekScenario* scenario, const int snapshot_id) {
    if (snapshot_id < 0)
        return SUCCESS;

    TekBodySnapshot* body_snapshot;
    tekChainThrow(tekScenarioGetSnapshot(scenario, snapshot_id, &body_snapshot));

    TekEvent event = {};
    event.type = BODY_UPDATE_EVENT;
    memcpy(&event.data.body.snapshot, body_snapshot, sizeof(TekBodySnapshot));
    event.data.body.id = (uint)snapshot_id;
    tekChainThrow(pushEvent(&event_queue, event));

    return SUCCESS;
}

static exception tekDeleteBodySnapshot(TekScenario* scenario, const int snapshot_id) {
    TekBodySnapshot* snapshot;
    tekChainThrow(tekScenarioGetSnapshot(scenario, (uint)snapshot_id, &snapshot));
    free(snapshot->model);
    free(snapshot->material);

    tekChainThrow(tekScenarioDeleteSnapshot(scenario, (uint)snapshot_id));

    TekEvent event = {};
    event.type = BODY_DELETE_EVENT;
    event.data.body.id = (uint)snapshot_id;
    tekChainThrow(pushEvent(&event_queue, event));

    return SUCCESS;
}

static exception tekRecreateBodySnapshot(const TekBodySnapshot* snapshot, const int snapshot_id) {
    TekEvent event = {};
    event.type = BODY_DELETE_EVENT;
    event.data.body.id = (uint)snapshot_id;
    tekChainThrow(pushEvent(&event_queue, event));

    tekChainThrow(tekBodyCreateEvent(snapshot, snapshot_id));
    return SUCCESS;
}

static exception tekChangeBodySnapshotModel(const TekScenario* scenario, const int snapshot_id, const char* model) {
    TekBodySnapshot* snapshot;
    tekChainThrow(tekScenarioGetSnapshot(scenario, (uint)snapshot_id, &snapshot));

    const uint len_model = strlen(model) + 1;
    char* new_model = realloc(snapshot->model, len_model);
    if (!new_model)
        tekThrow(MEMORY_EXCEPTION, "Failed to allocate memory for new model.");
    memcpy(new_model, model, len_model);
    snapshot->model = new_model;

    tekChainThrow(tekRecreateBodySnapshot(snapshot, snapshot_id));

    return SUCCESS;
}

static exception tekChangeBodySnapshotMaterial(const TekScenario* scenario, const int snapshot_id, const char* material) {
    TekBodySnapshot* snapshot;
    tekChainThrow(tekScenarioGetSnapshot(scenario, (uint)snapshot_id, &snapshot));

    const uint len_material = strlen(material) + 1;
    char* new_material = realloc(snapshot->material, len_material);
    if (!new_material)
        tekThrow(MEMORY_EXCEPTION, "Failed to allocate memory for new material.");
    memcpy(new_material, material, len_material);
    snapshot->material = new_material;

    tekChainThrow(tekRecreateBodySnapshot(snapshot, snapshot_id));

    return SUCCESS;
}

static exception tekResetScenario(const TekScenario* scenario) {
    TekEvent event = {};
    event.type = CLEAR_EVENT;
    tekChainThrow(pushEvent(&event_queue, event));

    uint* ids;
    uint num_ids;
    tekChainThrow(tekScenarioGetAllIds(scenario, &ids, &num_ids));

    for (uint i = 0; i < num_ids; i++) {
        const uint id = ids[i];
        TekBodySnapshot* snapshot;
        tekChainThrow(tekScenarioGetSnapshot(scenario, id, &snapshot));
        tekChainThrow(tekBodyCreateEvent(snapshot, id));
    }

    free(ids);

    return SUCCESS;
}

static exception tekRestartScenario(const TekScenario* scenario) {
    uint* ids;
    uint num_ids;
    tekChainThrow(tekScenarioGetAllIds(scenario, &ids, &num_ids));

    for (uint i = 0; i < num_ids; i++) {
        const uint id = ids[i];
        tekChainThrow(tekUpdateBodySnapshot(scenario, id));
    }

    free(ids);
    return SUCCESS;
}

static exception tekSimulationSpeedEvent(const double rate, const double speed, TekGuiOptionWindow* runner_window) {
    TekEvent event = {};
    event.type = TIME_EVENT;
    event.data.time.rate = rate;
    event.data.time.speed = speed;

    tekChainThrow(pushEvent(&event_queue, event));

    tekChainThrow(tekGuiWriteNumberOption(runner_window, "rate", rate));
    tekChainThrow(tekGuiWriteNumberOption(runner_window, "speed", speed));

    return SUCCESS;
}

static void tekHideAllWindows(struct TekGuiComponents* gui) {
    gui->hierarchy_window.window.visible = 0;
    gui->editor_window.window.visible = 0;
    gui->action_window.window.visible = 0;
    gui->save_window.window.visible = 0;
    gui->load_window.window.visible = 0;
    gui->runner_window.window.visible = 0;
}

static exception tekSwitchToMainMenu(struct TekGuiComponents* gui) {
    tekSetWindowColour((vec3){0.3f, 0.3f, 0.3f});
    tekGuiBringButtonToFront(&gui->start_button.button);
    tekHideAllWindows(gui);
    return SUCCESS;
}

static exception tekSwitchToBuilderMenu(struct TekGuiComponents* gui) {
    tekHideAllWindows(gui);
    gui->hierarchy_window.window.visible = 1;
    gui->action_window.window.visible = 1;
    tekChainThrow(tekGuiBringWindowToFront(&gui->hierarchy_window.window));
    tekChainThrow(tekGuiBringWindowToFront(&gui->action_window.window));
    tekChainThrow(tekGuiBringWindowToFront(&gui->editor_window.window));
    tekChainThrow(tekRestartScenario(&active_scenario));
    return SUCCESS;
}

static exception tekSwitchToSaveMenu(struct TekGuiComponents* gui) {
    tekHideAllWindows(gui);
    gui->save_window.window.visible = 1;
    int window_width, window_height;
    tekGetWindowSize(&window_width, &window_height);
    tekGuiSetWindowPosition(
        &gui->save_window.window,
        (window_width - (int)gui->save_window.window.width) / 2,
        (window_height - (int)gui->save_window.window.height) / 2
    );
    tekChainThrow(tekGuiBringWindowToFront(&gui->save_window.window));
    return SUCCESS;
}

static exception tekSwitchToLoadMenu(struct TekGuiComponents* gui) {
    tekHideAllWindows(gui);
    gui->load_window.window.visible = 1;
    int window_width, window_height;
    tekGetWindowSize(&window_width, &window_height);
    tekGuiSetWindowPosition(
        &gui->load_window.window,
        (window_width - (int)gui->load_window.window.width) / 2,
        (window_height - (int)gui->load_window.window.height) / 2
    );
    tekChainThrow(tekGuiBringWindowToFront(&gui->load_window.window));
    return SUCCESS;
}

static exception tekSwitchToRunnerMenu(struct TekGuiComponents* gui) {
    tekHideAllWindows(gui);
    gui->runner_window.window.visible = 1;
    tekChainThrow(tekGuiBringWindowToFront(&gui->runner_window.window));
    tekChainThrow(tekSimulationSpeedEvent(DEFAULT_RATE, DEFAULT_SPEED, &gui->runner_window));

    return SUCCESS;
}

static exception tekChangeMenuMode(struct TekGuiComponents* gui) {
    TekEvent event = {};
    event.type = MODE_CHANGE_EVENT;
    event.data.mode = next_mode;
    tekChainThrow(pushEvent(&event_queue, event));

    switch (next_mode) {
    case MODE_MAIN_MENU:
        tekChainThrow(tekSwitchToMainMenu(gui));
        break;
    case MODE_BUILDER:
        tekChainThrow(tekSwitchToBuilderMenu(gui));
        break;
    case MODE_RUNNER:
        tekChainThrow(tekSwitchToRunnerMenu(gui));
        break;
    case MODE_SAVE:
        tekChainThrow(tekSwitchToSaveMenu(gui));
        break;
    case MODE_LOAD:
        tekChainThrow(tekSwitchToLoadMenu(gui));
        break;
    default:
        // if the mode is not recognised, then dont do anything.
        next_mode = -1;
        return SUCCESS;
    }

    mode = next_mode;
    next_mode = -1;
    return SUCCESS;
}

static void tekStartButtonCallback(TekGuiTextButton* button, TekGuiButtonCallbackData callback_data) {
    if (mode != MODE_MAIN_MENU)
        return;

    if (callback_data.type != TEK_GUI_BUTTON_MOUSE_BUTTON_CALLBACK)
        return;

    if (callback_data.data.mouse_button.action != GLFW_RELEASE || callback_data.data.mouse_button.button != GLFW_MOUSE_BUTTON_LEFT)
        return;

    next_mode = MODE_BUILDER;
}

static exception tekCreateMainMenu(const int window_width, const int window_height, struct TekGuiComponents* gui) {
    tekChainThrow(tekGuiCreateTextButton("Start", &gui->start_button));
    tekChainThrow(tekGuiSetTextButtonPosition(&gui->start_button, window_width / 2 - 100, window_height / 2 - 50));
    gui->start_button.text_height = 50;
    gui->start_button.callback = tekStartButtonCallback;
    const float centre_x = (float)window_width / 2;
    const float centre_y = 100.0f;
    const float half_width = 150;
    const float half_height = 25;
    tekChainThrow(tekGuiCreateImage(
        (vec2){centre_x - half_width, centre_y - half_height},
        (vec2){centre_x + half_width, centre_y + half_height},
        "../res/tekphysics.png",
        &gui->logo
    ));
    return SUCCESS;
}

static exception tekDrawMainMenu(const struct TekGuiComponents* gui) {
    int window_width, window_height;
    tekGetWindowSize(&window_width, &window_height);
    tekGuiDrawTextButton(&gui->start_button);
    tekGuiDrawImage(&gui->logo);
    return SUCCESS;
}

static void tekDeleteMainMenu(const struct TekGuiComponents* gui) {
    tekGuiDeleteTextButton(&gui->start_button);
    tekGuiDeleteImage(&gui->logo);
}

static exception tekUpdateEditorWindow(TekGuiOptionWindow* editor_window, const TekScenario* scenario) {
    if (hierarchy_index < 0) {
        editor_window->window.visible = 0;
        return SUCCESS;
    }
    editor_window->window.visible = 1;
    TekBodySnapshot* body;
    tekChainThrow(tekScenarioGetSnapshot(scenario, (uint)hierarchy_index, &body));
    char* object_name;
    tekChainThrow(tekScenarioGetName(scenario, (uint)hierarchy_index, &object_name));
    tekChainThrow(tekGuiWriteStringOption(editor_window, "name", object_name, strlen(object_name) + 1));

    tekChainThrow(tekGuiWriteVec3Option(editor_window, "position", body->position));
    tekChainThrow(tekGuiWriteVec3Option(editor_window, "rotation", body->rotation));
    tekChainThrow(tekGuiWriteVec3Option(editor_window, "velocity", body->velocity));
    tekChainThrow(tekGuiWriteNumberOption(editor_window, "mass", body->mass));
    tekChainThrow(tekGuiWriteNumberOption(editor_window, "friction", body->friction));
    tekChainThrow(tekGuiWriteNumberOption(editor_window, "restitution", body->restitution));
    tekChainThrow(tekGuiWriteBooleanOption(editor_window, "immovable", (flag)body->immovable));
    tekChainThrow(tekGuiWriteStringOption(editor_window, "model", body->model, strlen(body->model) + 1));
    tekChainThrow(tekGuiWriteStringOption(editor_window, "material", body->material, strlen(body->material) + 1));

    return SUCCESS;
}

static void tekHierarchyCallback(TekGuiListWindow* hierarchy_window) {
    if (mode != MODE_BUILDER)
        return;
    TekGuiOptionWindow* editor_window = hierarchy_window->data;

    int index;
    tekScenarioGetByNameIndex(&active_scenario, hierarchy_window->select_index, NULL, &index);

    if (index == -1) {
        tekCreateBodySnapshot(&active_scenario, &hierarchy_index);
    } else {
        hierarchy_index = index;
    }

    tekUpdateEditorWindow(editor_window, &active_scenario);
}

static exception tekDisplayOptionError(TekGuiOptionWindow* window, const char* key, const char* error_message) {
    tekChainThrow(tekGuiWriteStringOption(window, key, error_message, strlen(error_message) + 1))
    return SUCCESS;
}

static exception tekReadNewFileInput(TekGuiOptionWindow* window, const char* key, const char* directory, const char* extension, char** filepath) {
    char* filename;
    tekChainThrow(tekGuiReadStringOption(window, key, &filename));

    if (!filename || !filename[0]) {
        *filepath = 0;
        return tekDisplayOptionError(window, key, "ENTER FILE");
    }

    tekChainThrow(addPathToFile(directory, filename, filepath));

    const uint len_filename = strlen(filename);
    const uint len_extension = strlen(extension);
    flag needs_ext = 1;
    if (len_filename >= len_extension) {
        const char* ext_ptr = filename + len_filename - 5;
        if (!strcmp(ext_ptr, extension)) {
            needs_ext = 0;
        }
    }

    if (needs_ext) {
        char* filepath_ext;
        tekChainThrow(addPathToFile(*filepath, ".tscn", &filepath_ext));
        free(*filepath);
        *filepath = filepath_ext;
    }

    return SUCCESS;
}

static exception tekReadExistingFileInput(TekGuiOptionWindow* window, const char* key, const char* directory, const char* extension, char** filepath) {
    char* filename;
    tekChainThrow(tekGuiReadStringOption(window, key, &filename));
    if (!filename || !filename[0]) {
        *filepath = 0;
        return tekDisplayOptionError(window, key, "ENTER FILE");
    }

    if (fileExists(filename)) {
        *filepath = 0;
        return SUCCESS;
    }

    tekChainThrow(addPathToFile(directory, filename, filepath));

    if (!fileExists(*filepath)) {
        char* filepath_ext;
        tekChainThrow(addPathToFile(*filepath, extension, &filepath_ext));
        free(*filepath);
        if (!fileExists(filepath_ext)) {
            free(filepath_ext);
            *filepath = 0;
            return tekDisplayOptionError(window, key, "NOT FOUND");
        }
        *filepath = filepath_ext;
    }

    return SUCCESS;
}

static exception tekEditorCallback(TekGuiOptionWindow* window, TekGuiOptionWindowCallbackData callback_data) {
    if (mode != MODE_BUILDER)
        return SUCCESS;

    if (hierarchy_index < 0)
        return SUCCESS;

    if (!strcmp(callback_data.name, "delete")) {
        tekChainThrow(tekDeleteBodySnapshot(&active_scenario, hierarchy_index));
        hierarchy_index = -1;
        return SUCCESS;
    }

    if (!strcmp(callback_data.name, "name")) {
        char* inputted_name;
        tekChainThrow(tekGuiReadStringOption(window, "name", &inputted_name));
        tekChainThrow(tekScenarioSetName(&active_scenario, (uint)hierarchy_index, inputted_name));
        return SUCCESS;
    }

    if (!strcmp(callback_data.name, "model")) {
        char* filepath;
        tekChainThrow(tekReadExistingFileInput(window, "model", "../res/", ".tmsh", &filepath));
        if (!filepath)
            return SUCCESS;

        tekChainThrow(tekChangeBodySnapshotModel(&active_scenario, hierarchy_index, filepath));

        free(filepath);

        return SUCCESS;
    }

    if (!strcmp(callback_data.name, "material")) {
        char* filepath;
        tekChainThrow(tekReadExistingFileInput(window, "material", "../res/", ".tmat", &filepath));
        if (!filepath)
            return SUCCESS;

        tekChainThrow(tekChangeBodySnapshotMaterial(&active_scenario, hierarchy_index, filepath));

        free(filepath);

        return SUCCESS;
    }

    TekBodySnapshot* snapshot;
    tekChainThrow(tekScenarioGetSnapshot(&active_scenario, (uint)hierarchy_index, &snapshot));
    tekChainThrow(tekGuiReadVec3Option(window, "position", snapshot->position));
    tekChainThrow(tekGuiReadVec3Option(window, "rotation", snapshot->rotation));
    tekChainThrow(tekGuiReadVec3Option(window, "velocity", snapshot->velocity));
    double number;
    tekChainThrow(tekGuiReadNumberOption(window, "mass", &number));
    snapshot->mass = (float)number;
    if (snapshot->mass < 0.0001f)
        snapshot->mass = 0.0001f;
    tekChainThrow(tekGuiReadNumberOption(window, "restitution", &number));
    snapshot->restitution = (float)number;
    if (snapshot->restitution > 1.0f)
        snapshot->restitution = 1.0f;
    if (snapshot->restitution < 0.0f)
        snapshot->restitution = 0.0f;
    tekChainThrow(tekGuiReadNumberOption(window, "friction", &number));
    snapshot->friction = (float)number;
    if (snapshot->friction > 1.0f)
        snapshot->friction = 1.0f;
    if (snapshot->friction < 0.0f)
        snapshot->friction = 0.0f;

    flag immovable;
    tekChainThrow(tekGuiReadBooleanOption(window, "immovable", &immovable));
    snapshot->immovable = (int)immovable;

    tekChainThrow(tekUpdateEditorWindow(window, &active_scenario));
    tekChainThrow(tekUpdateBodySnapshot(&active_scenario, hierarchy_index));

    return SUCCESS;
}

static void tekActionCallback(TekGuiListWindow* window) {
    if (mode != MODE_BUILDER)
        return;

    switch (window->select_index) {
    case SAVE_OPTION:
        next_mode = MODE_SAVE;
        break;
    case LOAD_OPTION:
        next_mode = MODE_LOAD;
        break;
    case RUN_OPTION:
        next_mode = MODE_RUNNER;
        break;
    case QUIT_OPTION:
        next_mode = MODE_MAIN_MENU;
        break;
    default:
        return;
    }
}

static exception tekCreateActionsList(List** actions_list) {
    *actions_list = (List*)malloc(sizeof(List));
    listCreate(*actions_list);

    for (uint i = 0; i < NUM_OPTIONS; i++) {
        const char* string;
        switch (i) {
        case SAVE_OPTION:
            string = "Save";
            break;
        case LOAD_OPTION:
            string = "Load";
            break;
        case RUN_OPTION:
            string = "Run";
            break;
        case QUIT_OPTION:
            string = "Quit";
            break;
        default:
           tekThrow(FAILURE, "Unknown option id in action menu.");
        }
        const uint len_string = strlen(string) + 1;
        char* buffer = (char*)malloc(sizeof(char) * len_string);
        memcpy(buffer, string, len_string);
        tekChainThrow(listAddItem(*actions_list, buffer));
    }

    return SUCCESS;
}

static exception tekCreateBuilderMenu(const int window_width, const int window_height, struct TekGuiComponents* gui) {
    tekCreateScenario(&active_scenario);
    tekChainThrow(tekGuiCreateListWindow(&gui->hierarchy_window, &active_scenario.names));
    gui->hierarchy_window.data = &gui->editor_window;
    gui->hierarchy_window.callback = tekHierarchyCallback;
    tekChainThrow(tekGuiSetWindowTitle(&gui->hierarchy_window.window, "Hierarchy"));
    tekGuiSetWindowPosition(&gui->hierarchy_window.window, 10, 10 + (int)gui->hierarchy_window.window.title_width);

    tekChainThrow(tekGuiCreateOptionWindow("../res/windows/editor.yml", &gui->editor_window))
    gui->editor_window.window.visible = 0;
    gui->editor_window.data = NULL;
    gui->editor_window.callback = tekEditorCallback;

    List* actions_list;
    tekChainThrow(tekCreateActionsList(&actions_list));
    gui->action_window.callback = tekActionCallback;
    tekChainThrow(tekGuiCreateListWindow(&gui->action_window, actions_list));
    tekChainThrow(tekGuiSetWindowTitle(&gui->action_window.window, "Actions"))
    tekGuiSetWindowPosition(&gui->action_window.window, 10, window_height - (int)gui->action_window.window.height - 10);

    return SUCCESS;
}

static exception tekDrawBuilderMenu(struct TekGuiComponents* gui) {
    if (hierarchy_index < 0) {
        gui->editor_window.window.visible = 0;
    }
    tekChainThrow(tekGuiDrawAllWindows());
    return SUCCESS;
}

static void tekDeleteBuilderMenu(struct TekGuiComponents* gui) {
    tekGuiDeleteListWindow(&gui->hierarchy_window);
    tekGuiDeleteOptionWindow(&gui->editor_window);
    listFreeAllData(gui->action_window.text_list);
    listDelete(gui->action_window.text_list);
    tekGuiDeleteListWindow(&gui->action_window);
}

static exception tekSaveCallback(TekGuiOptionWindow* window, TekGuiOptionWindowCallbackData callback_data) {
    if (callback_data.type != TEK_BUTTON_INPUT) {
        return SUCCESS;
    }

    if (!strcmp(callback_data.name, "cancel")) {
        next_mode = MODE_BUILDER;
        return SUCCESS;
    }

    char* filepath;
    tekChainThrow(tekReadNewFileInput(window, "filename", "../saves/", ".tscn", &filepath))
    if (filepath) {
        tekChainThrow(tekWriteScenario(&active_scenario, filepath));
        next_mode = MODE_BUILDER;
        free(filepath);
    }

    return SUCCESS;
}

static exception tekLoadCallback(TekGuiOptionWindow* window, TekGuiOptionWindowCallbackData callback_data) {
    if (callback_data.type != TEK_BUTTON_INPUT) {
        return SUCCESS;
    }

    if (!strcmp(callback_data.name, "cancel")) {
        next_mode = MODE_BUILDER;
        return SUCCESS;
    }

    char* filepath;
    tekChainThrow(tekReadExistingFileInput(window, "filename", "../saves/", ".tscn", &filepath));
    if (filepath) {
        tekDeleteScenario(&active_scenario);
        tekChainThrow(tekReadScenario(filepath, &active_scenario));
        tekChainThrow(tekResetScenario(&active_scenario));

        free(filepath);

        next_mode = MODE_BUILDER;
    }

    return SUCCESS;
}

static exception tekPauseEvent(const flag paused) {
    TekEvent event = {};
    event.type = PAUSE_EVENT;
    event.data.paused = paused;
    tekChainThrow(pushEvent(&event_queue, event));

    return SUCCESS;
}

static exception tekRunnerCallback(TekGuiOptionWindow* window, TekGuiOptionWindowCallbackData callback_data) {
    if (!strcmp(callback_data.name, "stop")) {
        next_mode = MODE_BUILDER;
        return SUCCESS;
    }

    if (!strcmp(callback_data.name, "pause")) {
        tekChainThrow(tekPauseEvent(1));
        return SUCCESS;
    }

    if (!strcmp(callback_data.name, "play")) {
        tekChainThrow(tekPauseEvent(0));
        return SUCCESS;
    }

    if (!strcmp(callback_data.name, "step")) {
        TekEvent event = {};
        event.type = STEP_EVENT;
        tekChainThrow(pushEvent(&event_queue, event));
    }

    double speed;
    tekChainThrow(tekGuiReadNumberOption(window, "speed", &speed));
    if (speed < 0.01)
        speed = 0.01;
    if (speed > 100)
        speed = 100;

    double rate;
    tekChainThrow(tekGuiReadNumberOption(window, "rate", &rate));
    if (rate < 1)
        rate = 1;
    if (rate > 100)
        rate = 100;

    if (1 / rate / speed > 0.1) {
        rate = 10.0 / speed;
    }

    tekChainThrow(tekSimulationSpeedEvent(rate, speed, window));

    return SUCCESS;
}

static exception tekCreateRunnerMenu(struct TekGuiComponents* gui) {
    tekChainThrow(tekGuiCreateOptionWindow("../res/windows/runner.yml", &gui->runner_window));
    tekChainThrow(tekSimulationSpeedEvent(DEFAULT_RATE, DEFAULT_SPEED, &gui->runner_window));
    gui->runner_window.callback = tekRunnerCallback;
    return SUCCESS;
}

static exception tekDrawRunnerMenu(struct TekGuiComponents* gui) {
    tekChainThrow(tekGuiDrawAllWindows());
    return SUCCESS;
}

static exception tekCreateMenu(struct TekGuiComponents* gui) {
    // create version font + text.
    TekBitmapFont* font;
    tekChainThrow(tekGuiGetDefaultFont(&font));
    tekChainThrow(tekCreateText("TekPhysics vI.D.K Alpha", 16, font, &gui->version_text));

    tekChainThrow(tekCreateMainMenu(WINDOW_WIDTH, WINDOW_HEIGHT, gui));
    tekChainThrow(tekCreateBuilderMenu(WINDOW_WIDTH, WINDOW_HEIGHT, gui));
    tekChainThrow(tekCreateRunnerMenu(gui));

    tekChainThrow(tekGuiCreateOptionWindow("../res/windows/save.yml", &gui->save_window));
    gui->save_window.callback = tekSaveCallback;
    tekChainThrow(tekGuiCreateOptionWindow("../res/windows/load.yml", &gui->load_window));
    gui->load_window.callback = tekLoadCallback;

    tekChainThrow(tekSwitchToMainMenu(gui));
    return SUCCESS;
}

static exception tekDrawMenu(struct TekGuiComponents* gui) {
    int window_width, window_height;
    tekGetWindowSize(&window_width, &window_height);

    if (next_mode != -1) {
        tekChangeMenuMode(gui);
    }

    tekSetDrawMode(DRAW_MODE_GUI);

    switch (mode) {
    case MODE_MAIN_MENU:
        tekChainThrow(tekDrawMainMenu(gui));
        break;
    case MODE_RUNNER:
        tekChainThrow(tekDrawRunnerMenu(gui));
        break;
    case MODE_BUILDER:
        tekChainThrow(tekDrawBuilderMenu(gui));
        break;
    case MODE_SAVE:
    case MODE_LOAD:
        tekGuiDrawAllWindows();
        break;
    default:
        break;
    }

    tekChainThrow(tekDrawText(&gui->version_text, (float)window_width - 185.0f, (float)window_height - 20.0f));

    tekSetDrawMode(DRAW_MODE_NORMAL);

    return SUCCESS;
}

static void tekDeleteMenu(struct TekGuiComponents* gui) {
    tekDeleteText(&gui->version_text);
    tekDeleteMainMenu(gui);
    tekDeleteBuilderMenu(gui);
}

#define tekRunCleanup() \
pushEvent(&event_queue, quit_event); \
tekAwaitEngineStop(engine_thread); \
tekDeleteMenu(&gui); \
tekDeleteScenario(&scenario); \
vectorDelete(&bodies); \
vectorDelete(&entities); \
tekDelete() \

exception run() {
    // set up GLFW window and other utilities.
    tekChainThrow(tekInit("TekPhysics", WINDOW_WIDTH, WINDOW_HEIGHT));

    // set up callbacks for window.
    tekChainThrowThen(tekAddKeyCallback(tekMainKeyCallback), {
        tekDelete();
    });
    tekChainThrowThen(tekAddMousePosCallback(tekMainMousePosCallback), {
        tekDelete();
    });
    tekChainThrowThen(tekAddMouseButtonCallback(tekMainMouseButtonCallback), {
        tekDelete();
    });

    // create state queue and thread queue, allow to communicate with thread.
    ThreadQueue state_queue = {};
    tekChainThrowThen(threadQueueCreate(&event_queue, 4096), {
        tekDelete();
    });
    tekChainThrowThen(threadQueueCreate(&state_queue, 4096), {
        threadQueueDelete(&event_queue);
        tekDelete();
    });

    // create vector to store all entities to be drawn.
    Vector entities = {};
    tekChainThrowThen(vectorCreate(0, sizeof(TekEntity), &entities), {
        threadQueueDelete(&state_queue);
        threadQueueDelete(&event_queue);
        tekDelete();
    });

    // create vector to store body snapshots / starting state of bodies.
    Vector bodies = {};
    tekChainThrowThen(vectorCreate(0, sizeof(TekBodySnapshot), &bodies), {
        vectorDelete(&entities);
        threadQueueDelete(&state_queue);
        threadQueueDelete(&event_queue);
        tekDelete();
    });

    // initialise the engine.
    TekEvent quit_event;
    quit_event.type = QUIT_EVENT;
    quit_event.data.message = 0;
    unsigned long long engine_thread;
    tekChainThrowThen(tekInitEngine(&event_queue, &state_queue, 1.0 / 30.0, &engine_thread), {
        vectorDelete(&bodies);
        vectorDelete(&entities);
        threadQueueDelete(&event_queue);
        threadQueueDelete(&state_queue);
        tekDelete();
    });

    // some random but useful values
    vec3 camera_position = {0.0f, 0.0f, 0.0f};
    vec3 camera_rotation = {0.0f, 0.0f, 0.0f};
    const vec3 light_color = {0.4f, 0.4f, 0.4f};
    const vec3 light_position = {4.0f, 12.0f, 0.0f};

    struct TekGuiComponents gui = {};

    TekScenario scenario = {};

    tekChainThrowThen(tekCreateMenu(
        &gui
    ), {
        tekDeleteScenario(&scenario);
        vectorDelete(&bodies);
        vectorDelete(&entities);
        threadQueueDelete(&event_queue);
        threadQueueDelete(&state_queue);
        tekDelete();
    });

    // create the camera struct
    TekCamera camera = {};
    tekChainThrowThen(tekCreateCamera(&camera, camera_position, camera_rotation, 1.2f, 0.1f, 100.0f), {
        tekRunCleanup();
    });

    // tekSetMouseMode(MOUSE_MODE_CAMERA);

    struct timespec curr_time, prev_time;
    float delta_time = 0.0f;
    clock_gettime(CLOCK_MONOTONIC, &prev_time);
    flag force_exit = 0;
    TekState state = {};
    while (tekRunning()) {
        while (recvState(&state_queue, &state) == SUCCESS) {
            switch (state.type) {
            case MESSAGE_STATE:
                if (state.data.message) {
                    printf("%s", state.data.message);
                    free(state.data.message);
                }
                break;
            case EXCEPTION_STATE:
                force_exit = 1;
                tekChainThrowThen(state.data.exception, {
                    tekRunCleanup();
                    threadQueueDelete(&event_queue);
                    threadQueueDelete(&state_queue);
                });
                break;
            case ENTITY_CREATE_STATE:
                TekEntity dummy_entity = {};
                if (state.object_id == entities.length) {
                    tekChainThrowThen(vectorAddItem(&entities, &dummy_entity), { tekRunCleanup(); });
                } else {
                    tekChainThrowThen(vectorSetItem(&entities, state.object_id, &dummy_entity), { tekRunCleanup(); });
                }
                TekEntity* create_entity = 0;
                vec3 default_scale = { 1.0f, 1.0f, 1.0f };
                tekChainThrowThen(vectorGetItemPtr(&entities, state.object_id, &create_entity), { tekRunCleanup(); });
                tekChainThrowThen(tekCreateEntity(state.data.entity.mesh_filename, state.data.entity.material_filename, state.data.entity.position, state.data.entity.rotation, default_scale, create_entity), { tekRunCleanup(); });
                break;
            case ENTITY_UPDATE_STATE:
                TekEntity* update_entity = 0;
                tekChainThrowThen(vectorGetItemPtr(&entities, state.object_id, &update_entity), { tekRunCleanup(); });
                tekUpdateEntity(update_entity, state.data.entity_update.position, state.data.entity_update.rotation);
                break;
            case ENTITY_DELETE_STATE:
                TekEntity* delete_entity;
                tekChainThrowThen(vectorGetItemPtr(&entities, state.object_id, &delete_entity), { tekRunCleanup(); });
                memset(delete_entity, 0, sizeof(TekEntity));
                break;
            }

            if (force_exit) break;
        }

        if (force_exit) break;

        switch (mode) {
        case MODE_MAIN_MENU:
            break;
        case MODE_RUNNER:
        case MODE_BUILDER:
            for (uint i = 0; i < entities.length; i++) {
                TekEntity* entity;
                tekChainThrowThen(vectorGetItemPtr(&entities, i, &entity), { tekRunCleanup(); });
                if (entity->mesh == 0) continue;
                tekChainThrowThen(tekDrawEntity(entity, &camera), { tekRunCleanup(); });
            }
            break;
        default:
            break;
        }

        tekChainThrowThen(tekDrawMenu(
            &gui
        ), {
            tekRunCleanup();
        });

        tekChainThrow(tekUpdate());

        clock_gettime(CLOCK_MONOTONIC, &curr_time);
        delta_time = (float)(curr_time.tv_sec - prev_time.tv_sec) + (float)(curr_time.tv_nsec - prev_time.tv_nsec) / (float)BILLION;
        memcpy(&prev_time, &curr_time, sizeof(struct timespec));

        if (mouse_moved) {
            if (mouse_right) {
                camera_rotation[0] = (float)(fmod(camera_rotation[0] + mouse_dx * delta_time * MOUSE_SENSITIVITY + M_PI, 2.0 * M_PI) - M_PI);
                camera_rotation[1] = (float)fmin(fmax(camera_rotation[1] - mouse_dy * delta_time * MOUSE_SENSITIVITY, -M_PI_2), M_PI_2);
            }
            mouse_moved = 0;
        }

        const double pitch = camera_rotation[1], yaw = camera_rotation[0];
        if (w_pressed) {
            camera_position[0] += (float)(cos(yaw) * cos(pitch) * MOVE_SPEED * delta_time);
            camera_position[1] += (float)(sin(pitch) * MOVE_SPEED * delta_time);
            camera_position[2] += (float)(sin(yaw) * cos(pitch) * MOVE_SPEED * delta_time);
        }
        if (a_pressed) {
            camera_position[0] += (float)(cos(yaw + M_PI_2) * -MOVE_SPEED * delta_time);
            camera_position[2] += (float)(sin(yaw + M_PI_2) * -MOVE_SPEED * delta_time);
        }
        if (s_pressed) {
            camera_position[0] += (float)(cos(yaw) * cos(pitch) * -MOVE_SPEED * delta_time);
            camera_position[1] += (float)(sin(pitch) * -MOVE_SPEED * delta_time);
            camera_position[2] += (float)(sin(yaw) * cos(pitch) * -MOVE_SPEED * delta_time);
        }
        if (d_pressed) {
            camera_position[0] += (float)(cos(yaw + M_PI_2) * MOVE_SPEED * delta_time);
            camera_position[2] += (float)(sin(yaw + M_PI_2) * MOVE_SPEED * delta_time);
        }

        tekSetCameraPosition(&camera, camera_position);
        tekSetCameraRotation(&camera, camera_rotation);

    }

    tekRunCleanup();

    return SUCCESS;
}

exception test() {
    // no collision, flat above
    vec3 triangle_a[] = {
        -3.899998903274536f, 1.0f, 0.4000000059604645f,
        -3.899998903274536f, -1.0f, -1.600000023841858f,
        -3.899998903274536f, -1.0f, 0.4000000059604645f
    };
    vec3 triangle_b[] = {
        -8.67393684387207f, 1.3345527648925781f, 1.2968101501464844f,
        -2.1538352966308594f, 1.7920713424682617f, 0.4208393096923828f,
        -3.4732666015625f, 1.5256071090698242f, -3.1470260620117188f
    };

    int collision = tekTriangleTest(triangle_a, triangle_b);

    printf("Triangles %s collide.\n", collision ? "do" : "do not");
    //
    // vec3 point;
    // tekProjectOriginOnTriangle(triangle_b, point);
    // printf("Point: %f %f %f\n", EXPAND_VEC3(point));

    return SUCCESS;
}

int main(void) {
    tekInitExceptions();
    tekLog(run());
    tekCloseExceptions();
}
