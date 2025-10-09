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

#define WINDOW_WIDTH  640
#define WINDOW_HEIGHT 480

flag mode = MODE_MAIN_MENU;
flag next_mode = -1;

float width, height;
mat4 perp_projection;
ThreadQueue event_queue = {};
TekEntity* last_entity = 0;

uint depth = 0;
char depth_message[64];
flag text_init = 0;
TekText depth_text = {};
TekBitmapFont depth_font = {};

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

static void tekStartButtonCallback(TekGuiTextButton* button, TekGuiButtonCallbackData callback_data) {
    if (mode != MODE_MAIN_MENU)
        return;

    if (callback_data.type != TEK_GUI_BUTTON_MOUSE_BUTTON_CALLBACK)
        return;

    if (callback_data.data.mouse_button.action != GLFW_RELEASE || callback_data.data.mouse_button.button != GLFW_MOUSE_BUTTON_LEFT)
        return;

    next_mode = MODE_BUILDER;
}

static exception tekOptionsCallback(TekGuiOptionWindow* window, TekGuiOptionWindowCallbackData callback_data) {
    printf("Callback for %s\n", callback_data.name);
    return SUCCESS;
}

static exception tekCreateMainMenu(const int window_width, const int window_height, TekGuiTextButton* start_button, TekGuiImage* tekphysics_logo) {
    tekChainThrow(tekGuiCreateTextButton("Start", start_button));
    tekChainThrow(tekGuiSetTextButtonPosition(start_button, window_width / 2 - 100, window_height / 2 - 50));
    start_button->text_height = 50;
    start_button->callback = tekStartButtonCallback;
    const float centre_x = (float)window_width / 2;
    const float centre_y = 100.0f;
    const float half_width = 150;
    const float half_height = 25;
    tekChainThrow(tekGuiCreateImage(
        (vec2){centre_x - half_width, centre_y - half_height},
        (vec2){centre_x + half_width, centre_y + half_height},
        "../res/tekphysics.png",
        tekphysics_logo
    ));
    return SUCCESS;
}

static exception tekDrawMainMenu(const TekGuiTextButton* start_button, const TekGuiImage* tekphysics_logo) {
    tekGuiDrawTextButton(start_button);
    tekGuiDrawImage(tekphysics_logo);
    return SUCCESS;
}

static exception tekSwitchToMainMenu(const TekGuiTextButton* start_button, const TekGuiImage* tekphysics_logo) {
    tekSetWindowColour((vec3){0.3f, 0.3f, 0.3f});
    tekGuiBringButtonToFront(&start_button->button);
    return SUCCESS;
}

static void tekDeleteMainMenu(const TekGuiTextButton* start_button, const TekGuiImage* tekphysics_logo) {
    tekGuiDeleteTextButton(start_button);
    tekGuiDeleteImage(tekphysics_logo);
}

static exception tekCreateBuilderMenu(TekGuiWindow* hierarchy_window) {
    tekChainThrow(tekGuiCreateWindow(hierarchy_window));
    return SUCCESS;
}

static exception tekDrawBuilderMenu(const TekGuiWindow* hierarchy_window) {
    tekChainThrow(tekGuiDrawAllWindows());
    return SUCCESS;
}

static exception tekCreateMenu(TekText* version_text, TekGuiTextButton* start_button, TekGuiImage* tekphysics_logo, TekGuiWindow* hierarchy_window) {
    // create version font + text.
    TekBitmapFont* font;
    tekChainThrow(tekGuiGetDefaultFont(&font));
    tekChainThrow(tekCreateText("TekPhysics vI.D.K Alpha", 16, font, version_text));

    tekChainThrow(tekCreateMainMenu(WINDOW_WIDTH, WINDOW_HEIGHT, start_button, tekphysics_logo));
    tekChainThrow(tekCreateBuilderMenu(hierarchy_window));

    tekChainThrow(tekSwitchToMainMenu(start_button, tekphysics_logo));
    return SUCCESS;
}

static exception tekChangeMenuMode(const TekText* version_text, const TekGuiTextButton* start_button, const TekGuiImage* tekphysics_logo) {
    TekEvent event = {};
    event.type = MODE_CHANGE_EVENT;
    event.data.mode = next_mode;
    tekChainThrow(pushEvent(&event_queue, event));

    switch (next_mode) {
    case MODE_MAIN_MENU:
        tekChainThrow(tekSwitchToMainMenu(start_button, tekphysics_logo));
        break;
    case MODE_BUILDER:
        break;
    case MODE_RUNNER:
        break;
    default:
        // if the mode is not recognised, then dont do anything.
        next_mode = -1;
        return SUCCESS;
    }

    mode = next_mode;
    return SUCCESS;
}

static exception tekDrawMenu(const TekText* version_text, const TekGuiTextButton* start_button, const TekGuiImage* tekphysics_logo, const TekGuiWindow* hierarchy_window) {
    int window_width, window_height;
    tekGetWindowSize(&window_width, &window_height);

    if (next_mode != -1) {
        tekChangeMenuMode(version_text, start_button, tekphysics_logo);
    }

    tekSetDrawMode(DRAW_MODE_GUI);

    switch (mode) {
    case MODE_MAIN_MENU:
        tekChainThrow(tekDrawMainMenu(start_button, tekphysics_logo));
        break;
    case MODE_RUNNER:
        break;
    case MODE_BUILDER:
        tekChainThrow(tekDrawBuilderMenu(hierarchy_window));
        break;
    default:
        break;
    }

    tekChainThrow(tekDrawText(version_text, (float)window_width - 185.0f, (float)window_height - 20.0f));

    tekSetDrawMode(DRAW_MODE_NORMAL);

    return SUCCESS;
}

static void tekDeleteMenu(const TekText* version_text, const TekGuiTextButton* start_button, const TekGuiImage* tekphysics_logo) {
    tekDeleteText(version_text);
    tekDeleteMainMenu(start_button, tekphysics_logo);
}

#define tekRunCleanup() \
pushEvent(&event_queue, quit_event); \
tekDeleteMenu(&version_text, &start_button, &tekphysics_logo); \
vectorDelete(&entities); \
tekDelete() \

static void text_callback(TekGuiTextInput* input, const char* text, uint len_text) {
    printf("Callback: %s\n", text);
    tekGuiSetTextInputText(input, "The Input Has Been Changed.");
}

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

    // initialise the engine.
    TekEvent quit_event;
    quit_event.type = QUIT_EVENT;
    quit_event.data.message = 0;
    tekChainThrowThen(tekInitEngine(&event_queue, &state_queue, 1.0 / 30.0), {
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

    TekText version_text = {};
    TekGuiTextButton start_button = {};
    TekGuiImage tekphysics_logo = {};

    TekGuiWindow hierarchy_window = {};

    TekGuiOptionWindow option_window = {};
    tekChainThrow(tekGuiCreateOptionWindow("../res/windows/test_window.yml", &option_window));
    option_window.callback = tekOptionsCallback;

    tekChainThrowThen(tekCreateMenu(
        &version_text,
        &start_button, &tekphysics_logo,
        &hierarchy_window
    ), {
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
                threadQueueDelete(&event_queue);
                threadQueueDelete(&state_queue);
                force_exit = 1;
                tekChainThrowThen(state.data.exception, { tekRunCleanup(); });
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
            for (uint i = 0; i < entities.length; i++) {
                TekEntity* entity;
                tekChainThrowThen(vectorGetItemPtr(&entities, i, &entity), { tekRunCleanup(); });
                if (entity->mesh == 0) continue;
                tekChainThrowThen(tekDrawEntity(entity, &camera), { tekRunCleanup(); });
            }
            break;
        case MODE_BUILDER:
            break;
        default:
            break;
        }

        tekChainThrowThen(tekDrawMenu(
            &version_text,
            &start_button, &tekphysics_logo,
            &hierarchy_window
        ), {
            tekRunCleanup();
        });

        tekChainThrow(tekUpdate());
    }

    tekRunCleanup();

    return SUCCESS;
}

exception test() {
    TekGuiOptionWindow option_window = {};
    tekChainThrow(tekGuiCreateOptionWindow("../res/windows/test_window.yml", &option_window));

    return SUCCESS;
}

int main(void) {
    tekInitExceptions();
    tekLog(run());
    tekCloseExceptions();
}
