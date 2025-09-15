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
#include "tekgui/text_button.h"

#define printException(x) tekLog(x)

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

    if ((key == GLFW_KEY_EQUAL) && (action == GLFW_RELEASE) && (depth < 100)) {
        depth++;
        updateDepthMessage();
    }
    if ((key == GLFW_KEY_MINUS) && (action == GLFW_RELEASE) && (depth > 0)) {
        depth--;
        updateDepthMessage();
    }

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

#define tekRunCleanup() \
pushEvent(&event_queue, quit_event); \
vectorDelete(&entities); \
tekDeleteText(&version_text); \
tekDelete() \

exception run() {
    // set up GLFW window and other utilities.
    tekChainThrow(tekInit("TekPhysics", 640, 480));

    // set up callbacks for window.
    tekChainThrowThen(tekAddKeyCallback(tekMainKeyCallback), {
        tekDelete();
    });
    tekChainThrowThen(tekAddMousePosCallback(tekMainMousePosCallback), {
        tekDelete();
    });

    // create version font + text.
    tekChainThrowThen(tekCreateBitmapFont("../res/verdana_bold.ttf", 0, 64, &depth_font), {
        tekDelete();
    });
    TekText version_text;
    tekChainThrowThen(tekCreateText("TekPhysics vI.D.K Alpha", 16, &depth_font, &version_text), {
        tekDelete();
    });

    // create state queue and thread queue, allow to communicate with thread.
    ThreadQueue state_queue = {};
    tekChainThrowThen(threadQueueCreate(&event_queue, 4096), {
        tekDeleteText(&version_text);
        tekDelete();
    });
    tekChainThrowThen(threadQueueCreate(&state_queue, 4096), {
        threadQueueDelete(&event_queue);
        tekDeleteText(&version_text);
        tekDelete();
    });

    // create vector to store all entities to be drawn.
    Vector entities = {};
    tekChainThrowThen(vectorCreate(0, sizeof(TekEntity), &entities), {
        threadQueueDelete(&state_queue);
        threadQueueDelete(&event_queue);
        tekDeleteText(&version_text);
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
        tekDeleteText(&version_text);
        tekDelete();
    });

    // some random but useful values
    vec3 camera_position = {0.0f, 0.0f, 0.0f};
    vec3 camera_rotation = {0.0f, 0.0f, 0.0f};
    const vec3 light_color = {0.4f, 0.4f, 0.4f};
    const vec3 light_position = {4.0f, 12.0f, 0.0f};

    // create the camera struct
    TekCamera camera = {};
    tekChainThrowThen(tekCreateCamera(&camera, camera_position, camera_rotation, 1.2f, 0.1f, 100.0f), {
        tekRunCleanup();
    });

    tekSetMouseMode(MOUSE_MODE_CAMERA);

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
        }

        for (uint i = 0; i < entities.length; i++) {
            TekEntity* entity;
            tekChainThrow(vectorGetItemPtr(&entities, i, &entity));
            if (entity->mesh == 0) continue;
            tekChainThrow(tekDrawEntity(entity, &camera));
        }

        int window_width, window_height;
        tekGetWindowSize(&window_width, &window_height);
        tekChainThrow(tekDrawText(&version_text, (float)(window_width - 185), (float)window_height - 8.0f));

        tekChainThrow(tekUpdate());
    }

    tekRunCleanup();

    return SUCCESS;
}

exception test() {
    Vector list = {};
    vectorCreate(1, sizeof(char), &list);

    char item = 'A';

    for (uint i = 0; i < 5; i++) {
        tekChainThrow(vectorAddItem(&list, &item));
        item++;
    }

    printf("Before inserting:\n[");
    for (uint i = 0; i < list.length; i++) {
        vectorGetItem(&list, i, &item);
        if (i != 0) printf(", ");
        printf("%c", item);
    }
    printf("]\n");

    item = '@';
    tekChainThrow(vectorInsertItem(&list, 1, &item));

    printf("After inserting:\n[");
    for (uint i = 0; i < list.length; i++) {
        vectorGetItem(&list, i, &item);
        if (i != 0) printf(", ");
        printf("%c", item);
    }
    printf("]\n");

    vectorDelete(&list);

    return SUCCESS;
}

int main(void) {
    tekInitExceptions();
    tekLog(run());
    tekCloseExceptions();
}
