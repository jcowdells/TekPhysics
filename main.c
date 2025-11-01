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

struct TekGuiComponents {
    TekGuiTextButton start_button;
    TekGuiImage logo;
    TekText version_text;
    TekGuiListWindow hierarchy_window;
    TekGuiOptionWindow editor_window;
    TekGuiListWindow action_window;
    TekGuiOptionWindow save_window;
    TekGuiOptionWindow load_window;
    List file_list;
};

flag mode = MODE_MAIN_MENU;
flag next_mode = -1;
int hierarchy_index = -1;

float width, height;
mat4 perp_projection;
ThreadQueue event_queue = {};
TekEntity* last_entity = 0;

uint depth = 0;
char depth_message[64];
flag text_init = 0;
TekText depth_text = {};
TekBitmapFont depth_font = {};

TekScenario active_scenario = {};

void tekMainFramebufferCallback(const int fb_width, const int fb_height) {
    width = (float)fb_width;
    height = (float)fb_height;
    glm_perspective(1.2f, width / height, 0.1f, 100.0f, perp_projection);
}

void tekMainKeyCallback(const int key, const int scancode, const int action, const int mods) {
    TekEvent event = {};
    event.type = KEY_EVENT;
    event.data.key_input.key = key;
    event.data.key_input.scancode = scancode;
    event.data.key_input.action = action;
    event.data.key_input.mods = mods;

    pushEvent(&event_queue, event);
}

void tekMainMousePosCallback(const double x, const double y) {
    TekEvent event = {};
    event.type = MOUSE_POS_EVENT;
    event.data.mouse_move_input.x = x;
    event.data.mouse_move_input.y = y;

    pushEvent(&event_queue, event);
}

static void tekHideAllWindows(struct TekGuiComponents* gui) {
    gui->hierarchy_window.window.visible = 0;
    gui->editor_window.window.visible = 0;
    gui->action_window.window.visible = 0;
    gui->save_window.window.visible = 0;
    gui->load_window.window.visible = 0;
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

    uint id;
    tekChainThrow(tekScenarioGetNextId(scenario, &id));
    tekChainThrow(tekScenarioPutSnapshot(scenario, &dummy, id, "New Object"));
    *snapshot_id = (int)id;

    TekEvent event = {};
    event.type = BODY_CREATE_EVENT;
    memcpy(&event.data.body.snapshot, &dummy, sizeof(TekBodySnapshot));
    event.data.body.id = id;
    tekChainThrow(pushEvent(&event_queue, event));

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

static exception tekEditorCallback(TekGuiOptionWindow* window, TekGuiOptionWindowCallbackData callback_data) {
    if (mode != MODE_BUILDER)
        return SUCCESS;

    if (hierarchy_index < 0)
        return SUCCESS;

    if (!strcmp(callback_data.name, "name")) {
        char* inputted_name;
        tekChainThrow(tekGuiReadStringOption(window, "name", &inputted_name));
        tekChainThrow(tekScenarioSetName(&active_scenario, (uint)hierarchy_index, inputted_name));
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
        printf("Run option.\n");
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

static exception tekDrawBuilderMenu() {
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
    if (callback_data.type == TEK_BUTTON_INPUT) {
        next_mode = MODE_MAIN_MENU;
    }

    char* filename;
    tekChainThrow(tekGuiReadStringOption(window, "filename", &filename));
    printf("Save Filename: %s\n", filename);

    return SUCCESS;
}

static exception tekLoadCallback(TekGuiOptionWindow* window, TekGuiOptionWindowCallbackData callback_data) {
    if (callback_data.type == TEK_BUTTON_INPUT) {
        next_mode = MODE_MAIN_MENU;
    }

    char* filename;
    tekChainThrow(tekGuiReadStringOption(window, "filename", &filename));
    printf("Load Filename: %s\n", filename);

    return SUCCESS;
}


static exception tekCreateMenu(struct TekGuiComponents* gui) {
    // create version font + text.
    TekBitmapFont* font;
    tekChainThrow(tekGuiGetDefaultFont(&font));
    tekChainThrow(tekCreateText("TekPhysics vI.D.K Alpha", 16, font, &gui->version_text));

    tekChainThrow(tekCreateMainMenu(WINDOW_WIDTH, WINDOW_HEIGHT, gui));
    tekChainThrow(tekCreateBuilderMenu(WINDOW_WIDTH, WINDOW_HEIGHT, gui));

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
        break;
    case MODE_BUILDER:
        tekChainThrow(tekDrawBuilderMenu());
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
                last_entity = create_entity;
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
            case CAMERA_MOVE_STATE:
                memcpy(camera_position, state.data.cam_position, sizeof(vec3));
                tekSetCameraPosition(&camera, camera_position);
                break;
            case CAMERA_ROTATE_STATE:
                memcpy(camera_rotation, state.data.cam_rotation, sizeof(vec3));
                tekSetCameraRotation(&camera, camera_rotation);
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
    }

    tekRunCleanup();

    return SUCCESS;
}

exception test() {
    TekScenario scenario = {};

    tekChainThrow(tekReadScenario("../res/scenario.tscn", &scenario));

    TekBodySnapshot snapshot = {};
    snapshot.friction = 100.0f;

    tekChainThrow(tekWriteScenario(&scenario, "../res/scenario.tscn"));

    tekDeleteScenario(&scenario);

    return SUCCESS;
}

int main(void) {
    tekInitExceptions();
    tekLog(run());
    tekCloseExceptions();
}
