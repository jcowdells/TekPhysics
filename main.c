#include <stdio.h>
#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <cglm/mat4.h>
#include "core/exception.h"
#include "tekgl/font.h"
#include "tekgl/text.h"

#include <cglm/cam.h>

#include "core/list.h"
#include "tekgl/manager.h"
#include "tekgui/primitives.h"

#include "tekgl/camera.h"

#include <time.h>
#include <math.h>
#include <unistd.h>

#include "core/file.h"
#include "core/threadqueue.h"
#include "core/vector.h"
#include "tekphys/engine.h"
#include "tekphys/body.h"
#include "tekphys/collisions.h"

#include "tekgui/tekgui.h"
#include "tekgui/window.h"
#include "tekgui/button.h"
#include "tekgui/list_window.h"
#include "tekgui/text_button.h"
#include "tekgui/option_window.h"
#include "tekphys/scenario.h"
#include "tests/unit_test.h"

#define WINDOW_WIDTH  1280
#define WINDOW_HEIGHT 720

#define START_BUTTON_WIDTH  100
#define START_BUTTON_HEIGHT 50

#define LOGO_WIDTH 300.0f
#define LOGO_HEIGHT 50.0f

// indices of possible options
#define SAVE_OPTION 0
#define LOAD_OPTION 1
#define RUN_OPTION  2
#define QUIT_OPTION 3
#define NUM_OPTIONS 4

#define MOUSE_SENSITIVITY 0.5f
#define MOVE_SPEED        2.0f

#define DEFAULT_RATE 100.0
#define DEFAULT_SPEED  1.0

struct TekScenarioOptions {
    float gravity;
    double rate;
    double speed;
    flag pause;
    vec3 sky_colour;
};

struct TekGuiComponents {
    TekGuiTextButton start_button;
    TekGuiImage logo;
    TekText version_text;
    TekText splash_text;
    TekGuiListWindow hierarchy_window;
    TekGuiOptionWindow editor_window;
    TekGuiListWindow action_window;
    TekGuiOptionWindow scenario_window;
    TekGuiOptionWindow save_window;
    TekGuiOptionWindow load_window;
    TekGuiOptionWindow runner_window;
    TekGuiWindow inspect_window;
    TekText inspect_text;
};

static flag mode = MODE_MAIN_MENU;
static flag next_mode = -1;
static int hierarchy_index = -1, inspect_index = -1;

ThreadQueue event_queue = {};

double mouse_x = 0.0, mouse_y = 0.0;
float mouse_dx = 0.0f, mouse_dy = 0.0f;
flag mouse_moved = 0, mouse_right = 0;
flag w_pressed = 0, a_pressed = 0, s_pressed = 0, d_pressed = 0, up_pressed = 0, down_pressed = 0;
TekScenario active_scenario = {};

static exception tekPushInspectEvent(const uint inspect_id) {
    TekEvent event = {};
    event.type = INSPECT_EVENT;
    event.data.body.id = inspect_id;

    tekChainThrow(pushEvent(&event_queue, event));
    return SUCCESS;
}

/**
 * The main key callback of the program. Listens for the W, A, S and D keys to allow for camera to move.
 * @param key The keycode of the key that was pressed.
 * @param scancode The scancode of the key.
 * @param action The action of the key - pressed, released, repeated.
 * @param mods Any modifiers on the key e.g. shift.
 */
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

    if (key == GLFW_KEY_UP && action == GLFW_RELEASE) {
        if (inspect_index > -1) {
            inspect_index--;
            if (inspect_index == -1) tekPushInspectEvent(0);
            else tekPushInspectEvent((uint)inspect_index);
        }
    }

    if (key == GLFW_KEY_DOWN && action == GLFW_RELEASE) {
        if (inspect_index + 2 < active_scenario.names.length) {
            inspect_index++;
            tekPushInspectEvent((uint)inspect_index);
        }
    }

}

/**
 * The main mouse position callback of the program. Used to update the camera rotation.
 * @param x The new x position of the mouse relative to the window.
 * @param y The new y position of the mouse relative to the window.
 */
void tekMainMousePosCallback(const double x, const double y) {
    if (mouse_moved) return;
    mouse_dx = (float)(x - mouse_x);
    mouse_dy = (float)(y - mouse_y);
    mouse_x = x;
    mouse_y = y;
    mouse_moved = 1;
}

/**
 * The main mouse button callback of the program. Used to track if the right mouse button is pressed, which would determine if the camera should rotate or not.
 * @param button The mouse button being pressed.
 * @param action The action - press or release.
 * @param mods Any modifiers on this action.
 */
void tekMainMouseButtonCallback(const int button, const int action, const int mods) {
    if (button == GLFW_MOUSE_BUTTON_RIGHT) {
        if (action == GLFW_PRESS) {
            mouse_right = 1;
        } else if (action == GLFW_RELEASE) {
            mouse_right = 0;
        }
    }
}

/**
 * Push a body create event to the event queue.
 * @param snapshot A pointer to a body snapshot.
 * @param snapshot_id The id of the body to update using this snapshot.
 * @note The body snapshot is copied into the event.
 * @throws MEMORY_EXCEPTION if the event could not be added to the queue.
 */
static exception tekBodyCreateEvent(const TekBodySnapshot* snapshot, const int snapshot_id) {
    TekEvent event = {};
    event.type = BODY_CREATE_EVENT;
    memcpy(&event.data.body.snapshot, snapshot, sizeof(TekBodySnapshot));
    event.data.body.id = snapshot_id;
    tekChainThrow(pushEvent(&event_queue, event));
    return SUCCESS;
}

/**
 * Create a body snapshot filled with default values and add it to a scenario. Also chooses a new ID for the created body.
 * @param scenario The scenario for which to add the new snapshot.
 * @param snapshot_id A pointer to where the newly created ID is to be written to.
 * @throws MEMORY_EXCEPTION if malloc() fails.
 */
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

/**
 * Push a body update event to the event queue. Uses the body snapshot data stored at the specified ID in the scenario.
 * @param scenario The scenario where the body is stored.
 * @param snapshot_id The ID of this body.
 * @throws MEMORY_EXCEPTION if malloc() fails.
 * @throws FAILURE if the body snapshot was not found in the scenario.
 */
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

/**
 * Delete a body snapshot from a scenario, and push a body delete event to the event queue.
 * @param scenario The scenario from which to delete the body.
 * @param snapshot_id The ID of the snapshot to be deleted.
 * @throws MEMORY_EXCEPTION if malloc() fails.
 * @throws FAILURE if a snapshot with that ID was not found.
 */
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

/**
 * Recreate a body from a body snapshot. This should be called if a large change happens to a body, such as changing the model so that a new collision structure can be made.
 * @param snapshot The body snapshot that should be used to recreate the body.
 * @param snapshot_id The ID of the snapshot that should be recreated.
 * @note Functionally this is the same as deleting the body and making a new one with the same ID.
 * @throws MEMORY_EXCEPTION if malloc() fails.
 */
static exception tekRecreateBodySnapshot(const TekBodySnapshot* snapshot, const int snapshot_id) {
    TekEvent event = {};
    event.type = BODY_DELETE_EVENT;
    event.data.body.id = (uint)snapshot_id;
    tekChainThrow(pushEvent(&event_queue, event));

    tekChainThrow(tekBodyCreateEvent(snapshot, snapshot_id));
    return SUCCESS;
}

/**
 * Change the model of a snapshot body. This needs a separate function as changing a model requires the physics body to be recreated.
 * @param scenario The scenario that contains the body snapshot that needs a new model.
 * @param snapshot_id The ID of the snapshot that needs to be changed.
 * @param model The filename of the new model.
 * @throws MEMORY_EXCEPTION if realloc() fails.
 * @throws FAILURE if a snapshot with that ID could not be found.
 */
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

/**
 * Change the material of a snapshot body. This requires a separate function because the buffer containing the material filename needs to be reallocated.
 * @param scenario The scenario that contains the body snapshot to be updated.
 * @param snapshot_id The ID of the snapshot to be updated.
 * @param material The filename of the material to be used.
 * @throws MEMORY_EXCEPTION if realloc() fails.
 * @throws FAILURE if a body with that ID could not be found.
 */
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

/**
 * Reset the scenario so that there are no bodies left, and it is back to its original state.
 * @param scenario The scenario to reset.
 * @throws MEMORY_EXCEPTION if malloc() fails.
 */
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

/**
 * Restart a scenario, updating all bodies to the values specified by the scenario.
 * @param scenario The scenario to use to reset the bodies back to their original positions.
 * @throws MEMORY_EXCEPTION if malloc() fails.
 */
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

/**
 * Push a time event to the event queue, which will change the rate and speed of the simulation.
 * @param rate The rate of the simulation, the number of updates per second.
 * @param speed The speed of the simulation relative to real life.
 * @param runner_window The runner window that should be updated with the new values.
 * @throws MEMORY_EXCEPTION if malloc() fails.
 */
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

/**
 * Push a gravity event to the event queue, which will change the acceleration due to gravity.
 * @param gravity The new acceleration due to gravity.
 * @param runner_window The runner window to update with the new value.
 * @throws MEMORY_EXCEPTION if malloc() fails.
 */
static exception tekGravityEvent(const double gravity, TekGuiOptionWindow* runner_window) {
    TekEvent event = {};
    event.type = GRAVITY_EVENT;
    event.data.gravity = (float)gravity;
    tekChainThrow(pushEvent(&event_queue, event));

    tekChainThrow(tekGuiWriteNumberOption(runner_window, "gravity", gravity));

    return SUCCESS;
}

/**
 * Push a pause event to the event queue, which will stop the simulation temporarily.
 * @param paused 1 if paused, 0 if not
 * @throws MEMORY_EXCEPTION if malloc() fails.
 */
static exception tekPauseEvent(const flag paused) {
    TekEvent event = {};
    event.type = PAUSE_EVENT;
    event.data.paused = paused;
    tekChainThrow(pushEvent(&event_queue, event));

    return SUCCESS;
}

/**
 * Hide all the possible windows from each menu mode.
 * @param gui The gui components.
 */
static void tekHideAllWindows(struct TekGuiComponents* gui) {
    gui->hierarchy_window.window.visible = 0;
    gui->editor_window.window.visible = 0;
    gui->action_window.window.visible = 0;
    gui->scenario_window.window.visible = 0;
    gui->save_window.window.visible = 0;
    gui->load_window.window.visible = 0;
    gui->runner_window.window.visible = 0;
    gui->inspect_window.visible = 0;
}

/**
 * Switches the menu mode into the main menu mode. Brings the start button to the front.
 * @param gui The gui components.
 * @return
 */
static exception tekSwitchToMainMenu(struct TekGuiComponents* gui) {
    tekChainThrow(tekGuiBringButtonToFront(&gui->start_button.button));
    tekHideAllWindows(gui);
    return SUCCESS;
}

/**
 * Switches the menu mode into the builder menu mode. Makes all relevant windows visible and brings them to the front.
 * @param gui The gui components.
 * @throws MEMORY_EXCEPTION if malloc() fails.
 */
static exception tekSwitchToBuilderMenu(struct TekGuiComponents* gui) {
    tekHideAllWindows(gui);
    gui->hierarchy_window.window.visible = 1;
    gui->action_window.window.visible = 1;
    gui->scenario_window.window.visible = 1;
    tekChainThrow(tekGuiBringWindowToFront(&gui->hierarchy_window.window));
    tekChainThrow(tekGuiBringWindowToFront(&gui->action_window.window));
    tekChainThrow(tekGuiBringWindowToFront(&gui->editor_window.window));
    tekChainThrow(tekGuiBringWindowToFront(&gui->scenario_window.window));
    tekChainThrow(tekRestartScenario(&active_scenario));

    flag begin_paused;
    tekChainThrow(tekGuiReadBooleanOption(&gui->scenario_window, "pause", &begin_paused));
    tekChainThrow(tekPauseEvent(begin_paused));

    return SUCCESS;
}

/**
 * Switches the menu mode to the save menu mode. Brings the save window to the front and re-centres it.
 * @param gui The gui components.
 * @throws MEMORY_EXCEPTION if malloc() fails.
 */
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

/**
 * Switches the menu mode to the load menu mode. Brings the load window to the front and re-centres it.
 * @param gui The gui components.
 * @throws MEMORY_EXCEPTION if malloc() fails.
 */
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

/**
 * Switches the menu mode to the runner menu mode. Displays the runner window and brings it to the front.
 * @param gui The gui components.
 * @throws LIST_EXCEPTION if there was a cosmic bit flip that changed a variable somewhere.
 */
static exception tekSwitchToRunnerMenu(struct TekGuiComponents* gui) {
    tekHideAllWindows(gui);
    gui->runner_window.window.visible = 1;
    gui->inspect_window.visible = 1;
    tekChainThrow(tekGuiBringWindowToFront(&gui->runner_window.window));
    tekChainThrow(tekGuiBringWindowToFront(&gui->inspect_window));
    inspect_index = -1;
    tekChainThrow(tekPushInspectEvent(0));

    // restart again because sometimes the velocities remain.
    tekChainThrow(tekRestartScenario(&active_scenario));

    return SUCCESS;
}

/**
 * Switches the menu mode based on what the next menu mode is specified to be. Resets the next mode to -1.
 * @param gui The gui components.
 * @note Uses the static variables mode and next_mode, this allows different callbacks to edit the mode.
 * @throws MEMORY_EXCEPTION if malloc() fails.
 */
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

/**
 * The callback for the start button. If the button was left-clicked, the mode is changed to builder.
 * @param button The start button / button being pressed.
 * @param callback_data Callback data from this button such as mouse position.
 */
static void tekStartButtonCallback(TekGuiTextButton* button, TekGuiButtonCallbackData callback_data) {
    if (mode != MODE_MAIN_MENU)
        return;

    if (callback_data.type != TEK_GUI_BUTTON_MOUSE_BUTTON_CALLBACK)
        return;

    if (callback_data.data.mouse_button.action != GLFW_RELEASE || callback_data.data.mouse_button.button != GLFW_MOUSE_BUTTON_LEFT)
        return;

    next_mode = MODE_BUILDER;
}

/**
 * Create the main menu, this includes creating the logo, the start button and the splash text.
 * @param window_width The width of the window.
 * @param window_height The height of the window.
 * @param gui The gui components.
 * @throws MEMORY_EXCEPTION if malloc() fails.
 * @throws FILE_EXCEPTION if the logo couldn't be found.
 */
static exception tekCreateMainMenu(const int window_width, const int window_height, struct TekGuiComponents* gui) {
    tekChainThrow(tekGuiCreateTextButton("", &gui->start_button));
    gui->start_button.text_height = 25;
    tekChainThrow(tekGuiSetTextButtonText(&gui->start_button, "Start"));
    tekChainThrow(tekGuiSetTextButtonSize(&gui->start_button, START_BUTTON_WIDTH, START_BUTTON_HEIGHT));
    tekChainThrow(tekGuiSetTextButtonPosition(&gui->start_button, (window_width - START_BUTTON_WIDTH) / 2 , (window_height - START_BUTTON_HEIGHT) / 2));
    gui->start_button.text_height = 50;
    gui->start_button.callback = tekStartButtonCallback;
    tekChainThrow(tekGuiCreateImage(
        LOGO_WIDTH, LOGO_HEIGHT,
        "../res/tekphysics.png",
        &gui->logo
    ));
    return SUCCESS;
}

/**
 * Draw the main menu. Draws the logo, start button and splash text.
 * @param gui The gui components.
 * @throws MEMORY_EXCEPTION if malloc() fails.
 * @throws OPENGL_EXCEPTION if OpenGL fails.
 * @throws VECTOR_EXCEPTION if a vector is written out of range.
 */
static exception tekDrawMainMenu(struct TekGuiComponents* gui) {
    int window_width, window_height;
    tekGetWindowSize(&window_width, &window_height);
    tekChainThrow(tekGuiDrawTextButton(&gui->start_button));

    const float x = ((float)window_width - LOGO_WIDTH) / 2.0f;
    const float y = (float)window_height / 4.0f - LOGO_HEIGHT / 2.0f;

    tekChainThrow(tekGuiDrawImage(&gui->logo, x, y));
    tekChainThrow(tekGuiSetTextButtonPosition(&gui->start_button, (window_width - START_BUTTON_WIDTH) / 2 , (window_height - START_BUTTON_HEIGHT) / 2));

    struct timespec curr_time;
    clock_gettime(CLOCK_MONOTONIC, &curr_time);

    const double progression = (double)(curr_time.tv_sec % 4) + (double)curr_time.tv_nsec / (double)BILLION;
    const double angle = progression * CGLM_PI_2;
    const double sin_angle = sin(angle);
    const double max_angle = CGLM_PI / 12.0;
    tekChainThrow(tekDrawColouredRotatedText(
        &gui->splash_text,
        x + LOGO_WIDTH - gui->splash_text.width / 2.0f, y + LOGO_HEIGHT - gui->splash_text.height / 2.0,
        (vec4){1.0f, 1.0f, 0.0f, 1.0f},
        x + LOGO_WIDTH,
        y + LOGO_HEIGHT,
        sin_angle * max_angle
        ));

    return SUCCESS;
}

/**
 * Delete the main menu, clearing any memory from the gui components used.
 * @param gui The gui components.
 */
static void tekDeleteMainMenu(const struct TekGuiComponents* gui) {
    tekGuiDeleteTextButton(&gui->start_button);
    tekGuiDeleteImage(&gui->logo);
    tekDeleteText(&gui->splash_text);
}

/**
 * Update the editor window with the current values of a body snapshot.
 * @param editor_window The editor window which to update.
 * @param scenario The scenario that contains the body snapshot.
 * @note Uses the currently selected object in the hierarchy window as the ID.
 * @throws LIST_EXCEPTION if the currently selected ID doesn't exist
 * @throws HASHTABLE_EXCEPTION if one of the option keys doesn't exist.
 */
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

/**
 * The callback for any updates to the hierarchy window. Updates the currently selected body snapshot.
 * @param hierarchy_window The list window that was updated / the hierarchy window.
 */
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

/**
 * Set a string input to display an error.
 * @param window The option window to apply the function to.
 * @param key The name of the option.
 * @param error_message The error message to write into the option.
 * @throws MEMORY_EXCEPTION if malloc() fails.
 */
static exception tekDisplayOptionError(TekGuiOptionWindow* window, const char* key, const char* error_message) {
    tekChainThrow(tekGuiWriteStringOption(window, key, error_message, strlen(error_message) + 1))
    return SUCCESS;
}

/**
 * Read a filename for a new, unwritten file. Checks to make sure the input is not empty, but will allow any filename otherwise.
 * If the inputted filename ends with the requested extension, it is not appended.
 * @param window The window to read the input from.
 * @param key The key of where to find the input data.
 * @param directory The base directory of where the file should be stored.
 * @param extension The extension that should be added to the file if not already added.
 * @param filepath The outputted filepath
 * @note "filepath" is allocated, so needs to be freed by the caller.
 * @throws MEMORY_EXCEPTION if malloc() fails.
 */
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

/**
 * Get the name of an existing file. The function will reject any inputs if the file specified does not exist, or if the input string is empty.
 * If a filename exists with the same name but the real file includes the specified extension, then this input is accepted.
 * @param window The window which contains the file input.
 * @param key The name of the option which has the input.
 * @param directory The base directory of the file.
 * @param extension The extension to be added to the file.
 * @param filepath The outputted filepath.
 * @note The filepath is allocated, and needs to be freed by the caller.
 * @throws MEMORY_EXCEPTION if malloc() fails.
 */
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

/**
 * The callback for the editor window. This is called whenever a user updates a body snapshot's properties or deletes it.
 * @param window The option window / editor window in this case.
 * @param callback_data The callback data containing the name of the option and its type + data.
 * @throws MEMORY_EXCEPTION if malloc() fails.
 */
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

/**
 * The callback for the action window. Called whenever the user selects an option like "save", "load" "run" or "quit".
 * @param window The list window / action window.
 */
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

/**
 * Create the list of possible actions for the action window.
 * @param actions_list A pointer to a list of actions that will be allocated. This needs to be freed.
 * @throws MEMORY_EXCEPTION if malloc() fails.
 */
static exception tekCreateActionsList(List** actions_list) {
    *actions_list = (List*)malloc(sizeof(List));
    if (!*actions_list)
        tekThrow(MEMORY_EXCEPTION, "Failed to allocate memory for action list.");
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

/**
 * The callback for the scenario options window. Called when options such as gravity, sky colour and start paused are changed.
 * @param window The option window / scenario window.
 * @param callback_data The callback data including the option name that was changed and the type of data.
 * @throws MEMORY_EXCEPTION if malloc() fails.
 */
static exception tekScenarioCallback(TekGuiOptionWindow* window, TekGuiOptionWindowCallbackData callback_data) {
    if (!strcmp(callback_data.name, "gravity")) {
        double gravity;
        tekChainThrow(tekGuiReadNumberOption(window, "gravity", &gravity));
        if (gravity < 0.0) {
            gravity = 0.0;
        }
        if (gravity > 100.0) {
            gravity = 100.0;
        }
        tekChainThrow(tekGravityEvent(gravity, window));
        return SUCCESS;
    }

    if (!strcmp(callback_data.name, "pause")) {
        flag begin_paused;
        tekChainThrow(tekGuiReadBooleanOption(window, "pause", &begin_paused));
        tekChainThrow(tekPauseEvent(begin_paused));
    }

    if (!strcmp(callback_data.name, "sky_colour")) {
        vec3 sky_colour;
        tekChainThrow(tekGuiReadVec3Option(window, "sky_colour", sky_colour));
        tekSetWindowColour(sky_colour);
    }

    return SUCCESS;
}

/**
 * Create the builder menu, this includes creating all the windows related to the object hierarchy, the options menu, the editor window and the scenario window.
 * @param window_width The width of the window.
 * @param window_height The height of the window.
 * @param gui The gui components.
 * @throws MEMORY_EXCEPTION if malloc() fails.
 */
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
    tekGuiSetWindowPosition(&gui->action_window.window, 10, (int)(gui->hierarchy_window.window.height + gui->hierarchy_window.window.title_width + gui->action_window.window.title_width) + 20);

    tekChainThrow(tekGuiCreateOptionWindow("../res/windows/scenario.yml", &gui->scenario_window));
    tekChainThrow(tekGuiWriteNumberOption(&gui->scenario_window, "gravity", 9.81));
    tekChainThrow(tekGuiWriteBooleanOption(&gui->scenario_window, "pause", 0));

    vec3 sky_colour = {0.3f, 0.3f, 0.3f};
    tekChainThrow(tekGuiWriteVec3Option(&gui->scenario_window, "sky_colour", sky_colour));
    tekSetWindowColour(sky_colour);

    gui->scenario_window.callback = tekScenarioCallback;
    gui->scenario_window.data = &gui->runner_window;

    return SUCCESS;
}

/**
 * Draw the builder menu. Draws all visible windows, and hides the editor window if the hierarchy index is less than 0.
 * @param gui The gui components.
 * @throws OPENGL_EXCEPTION if there was a rendering error.
 */
static exception tekDrawBuilderMenu(struct TekGuiComponents* gui) {
    if (hierarchy_index < 0) {
        gui->editor_window.window.visible = 0;
    }
    tekChainThrow(tekGuiDrawAllWindows());
    return SUCCESS;
}

/**
 * Delete the builder menu, freeing any memory that was allocated by its gui components.
 * @param gui The gui components.
 */
static void tekDeleteBuilderMenu(struct TekGuiComponents* gui) {
    tekGuiDeleteListWindow(&gui->hierarchy_window);
    tekGuiDeleteOptionWindow(&gui->editor_window);
    listFreeAllData(gui->action_window.text_list);
    listDelete(gui->action_window.text_list);
    tekGuiDeleteListWindow(&gui->action_window);
    tekGuiDeleteOptionWindow(&gui->scenario_window);
}

/**
 * The callback for the save window. Called when the user updates the save location or presses the save button.
 * @param window The option window / save window.
 * @param callback_data The callback data containing the name of the edited option and its type.
 * @throws MEMORY_EXCEPTION if malloc() fails.
 */
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

/**
 * The callback for the load window. Called when the user updates the load location or presses the load button.
 * @param window The option window / load window.
 * @param callback_data The callback data containing the name of the edited option and its type.
 * @throws MEMORY_EXCEPTION if malloc() fails.
 */
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

/**
 * The callback for the runner window, called when user updates the speed or rate of the simulation, or when they press play, pause, stop or step.
 * @param window The option window / runner window.
 * @param callback_data The callback data containing the name of the option and its type.
 * @throws MEMORY_EXCEPTION if malloc() fails.
 */
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

/**
 * The window draw callback for the inspect window. Called every frame that the window is drawn.
 * @param window The window / inspect window.
 */
static exception tekInspectDrawCallback(TekGuiWindow* window) {
    TekText* inspect_text = window->data;
    tekChainThrow(tekDrawText(inspect_text, (float)(window->x_pos + 10), (float)(window->y_pos + 10)));

    return SUCCESS;
}


static int tekWriteInspectText(char* string, size_t max_length, const float time, const float fps, const char* name, const vec3 position, const vec3 velocity) {
    return snprintf(
        string, max_length,
        "Time: %.3f\nFPS: %.3f\n\nObject Name: %s\nPosition: (%.5f, %.5f, %.5f)\nVelocity: (%.5f, %.5f, %.5f)\n\nUse up and down arrows to switch.",
        time, fps, name, EXPAND_VEC3(position), EXPAND_VEC3(velocity)
    );
}

static exception tekUpdateInspectText(TekText* inspect_text, const float time, const float fps, const char* name, const vec3 position, const vec3 velocity) {
    const int len_buffer = tekWriteInspectText(NULL, 0, time, fps, name, position, velocity) + 1;
    char* buffer = alloca(len_buffer * sizeof(char));
    if (!buffer)
        tekThrow(MEMORY_EXCEPTION, "Failed to allocate memory for inspect text.");

    tekWriteInspectText(buffer, len_buffer, time, fps, name, position, velocity);
    buffer[len_buffer - 1] = 0;

    tekChainThrow(tekUpdateText(inspect_text, buffer, 16));

    return SUCCESS;
}

/**
 * Create the runner menu, which has options related to controlling the running of the scenario such as speed and time controls.
 * @param gui The gui components.
 * @throws MEMORY_EXCEPTION if malloc() fails.
 * @throws FILE_EXCEPTION if the runner window file does not exist.
 */
static exception tekCreateRunnerMenu(struct TekGuiComponents* gui) {
    tekChainThrow(tekGuiCreateOptionWindow("../res/windows/runner.yml", &gui->runner_window));
    tekChainThrow(tekSimulationSpeedEvent(DEFAULT_RATE, DEFAULT_SPEED, &gui->runner_window));
    gui->runner_window.callback = tekRunnerCallback;

    TekBitmapFont* font;
    tekChainThrow(tekGuiGetDefaultFont(&font));
    tekChainThrow(tekCreateText("", 16, font, &gui->inspect_text));

    tekChainThrow(tekGuiCreateWindow(&gui->inspect_window));
    tekChainThrow(tekGuiSetWindowTitle(&gui->inspect_window, "Inspect"));
    gui->inspect_window.draw_callback = tekInspectDrawCallback;
    gui->inspect_window.data = &gui->inspect_text;

    return SUCCESS;
}

/**
 * Draw the runner menu to the screen.
 * @param gui The gui components.
 * @throws OPENGL_EXCEPTION if there was a rendering error.
 */
static exception tekDrawRunnerMenu(struct TekGuiComponents* gui) {
    tekChainThrow(tekGuiDrawAllWindows());
    return SUCCESS;
}

/**
 * Delete the runner memory, freeing any allocated memory for this section of the gui.
 * @param gui The gui components.
 */
static void tekDeleteRunnerMenu(struct TekGuiComponents* gui) {
    tekGuiDeleteOptionWindow(&gui->runner_window);
    tekGuiDeleteWindow(&gui->inspect_window);
    tekDeleteText(&gui->inspect_text);
}

/**
 * Choose a random line from the splash.txt file and allocate a new buffer to store the line in.
 * @param buffer A pointer to where the new buffer is to be allocated.
 * @throws MEMORY_EXCEPTION if either of alloca() or malloc() fails.
 * @throws LIST_EXCEPTION if attempted to pick a splash out of range.
 */
static exception tekGetSplashText(char** buffer) {
    const char* splash_file = "../res/splash.txt";

    uint len_file;
    tekChainThrow(getFileSize(splash_file, &len_file));

    // i just discovered this "alloca", it automatically frees after function returns
    // so sick
    char* file_buffer = alloca(len_file * sizeof(char));
    if (!file_buffer)
        tekThrow(MEMORY_EXCEPTION, "Failed to allocate memory to read splash text file.");

    tekChainThrow(readFile(splash_file, len_file, file_buffer));

    // list to store the indices of the start of each line
    List line_start_indices = {};
    listCreate(&line_start_indices);

    // always going to be a line to start with.
    tekChainThrow(listAddItem(&line_start_indices, 0));
    for (uint i = 0; i < len_file; i++) {
        if (file_buffer[i] == '\n')
            tekChainThrowThen(listAddItem(&line_start_indices, (void*)(i + 1)), { listDelete(&line_start_indices); });
    }

    // add an index at the end of the file
    // because to get the line we are going to substring from current index to next index.
    // this will be the next index if the last line is chosen
    tekChainThrowThen(listAddItem(&line_start_indices, (void*)len_file), { listDelete(&line_start_indices); });

    // generate random number
    srand(time(0));
    const uint list_index = (uint)rand() % (line_start_indices.length - 1);

    // get the line that was randomly picked
    void* line_start_index, * line_end_index;
    tekChainThrowThen(listGetItem(&line_start_indices, list_index, &line_start_index), { listDelete(&line_start_indices); });
    tekChainThrowThen(listGetItem(&line_start_indices, list_index + 1, &line_end_index), { listDelete(&line_start_indices); });
    listDelete(&line_start_indices);

    const uint len_line = (uint)line_end_index - (uint)line_start_index;

    // make buffer for splash text.
    *buffer = malloc(len_line * sizeof(char));
    if (!*buffer)
        tekThrow(MEMORY_EXCEPTION, "Failed to allocate buffer for splash text line.");
    memcpy(*buffer, file_buffer + (uint)line_start_index, len_line);
    (*buffer)[len_line - 1] = 0;

    return SUCCESS;
}

/**
 * Create the menus system. Will call all the sub menu creation functions.
 * @param gui The gui components
 * @throws MEMORY_EXCEPTION if malloc() fails.
 */
static exception tekCreateMenu(struct TekGuiComponents* gui) {
    // create version font + text.
    TekBitmapFont* font;
    tekChainThrow(tekGuiGetDefaultFont(&font));
    tekChainThrow(tekCreateText("TekPhysics v1.0 Release", 16, font, &gui->version_text));

    char* splash_text;
    tekChainThrow(tekGetSplashText(&splash_text));
    tekChainThrowThen(tekCreateText(splash_text, 20, font, &gui->splash_text), { free(splash_text); });
    free(splash_text);

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

/**
 * Draws the menu. Decides which draw function to call based on the current mode.
 * @param gui The gui components.
 * @throws OPENGL_EXCEPTION if there was a rendering error.
 */
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

    tekChainThrow(tekDrawText(&gui->version_text, (float)window_width - gui->version_text.width - 5.0f, (float)window_height - gui->version_text.height - 5.0f));

    tekSetDrawMode(DRAW_MODE_NORMAL);

    return SUCCESS;
}

/**
 * Delete the menu system and call the delete functions of the sub menus.
 * @param gui The gui components to delete.
 */
static void tekDeleteMenu(struct TekGuiComponents* gui) {
    tekDeleteText(&gui->version_text);
    tekDeleteMainMenu(gui);
    tekDeleteBuilderMenu(gui);
    tekGuiDeleteOptionWindow(&gui->save_window);
    tekGuiDeleteOptionWindow(&gui->load_window);
    tekDeleteRunnerMenu(gui);
}

/// Clean up everything that was allocated for the program.
#define tekRunCleanup() \
pushEvent(&event_queue, quit_event); \
tekAwaitEngineStop(engine_thread); \
tekDeleteMenu(&gui); \
tekDeleteScenario(&scenario); \
vectorDelete(&bodies); \
vectorDelete(&entities); \
tekDelete() \

/**
 * The actual main function of the program. Sets up the window, the rendering loop, the gui, the physics engine, and everything else.
 * @throws EXCEPTION Can throw all exceptions for many different reasons.
 */
static exception run() {
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

    struct timespec curr_time, prev_time;
    float delta_time = 0.0f, frame_time = 0.0f, fps = 0.0f;
    uint frame_count = 0;
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
            case INSPECT_STATE:
                char* inspect_name;
                vec3 position, velocity;
                if (inspect_index < 0) {
                    inspect_name = "<no body selected>";
                    glm_vec3_zero(position);
                    glm_vec3_zero(velocity);
                } else {
                    tekChainThrow(tekScenarioGetName(&active_scenario, (uint)inspect_index, &inspect_name));
                    glm_vec3_copy(state.data.inspect.position, position);
                    glm_vec3_copy(state.data.inspect.velocity, velocity);
                }
                tekChainThrow(tekUpdateInspectText(
                    &gui.inspect_text,
                    state.data.inspect.time, fps,
                    inspect_name, position, velocity
                ));
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
        frame_time += delta_time;

        if (frame_count == 10) {
            frame_count = 0;
            fps = 10.0f / frame_time;
            frame_time = 0.0f;
        }
        frame_count++;

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

/**
 * The entrypoint of the code. Mostly just a wrapper around the \ref run function.
 * @return The exception code, or 0 if there were no exceptions.
 */
int main(void) {
    tekInitExceptions();
    const exception tek_exception = tekUnitTest();
    tekLog(tek_exception);
    tekCloseExceptions();
    return tek_exception;
}
