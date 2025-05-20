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

#include "core/queue.h"
#include "core/threadqueue.h"
#include "core/vector.h"
#include "tekphys/engine.h"
#include "tekphys/body.h"

#define printException(x) tekLog(x)

float width, height;
mat4 perp_projection;
ThreadQueue event_queue = {};
TekEntity* last_entity = 0;

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

// int render() {
//     tekChainThrow(tekInit("TekPhysics", 640, 480));
//     width = 640.0f;
//     height = 480.0f;
//
//     tekAddFramebufferCallback(tekMainFramebufferCallback);
//
//     TekBitmapFont verdana;
//     printException(tekCreateFreeType());
//
//     tekChainThrow(tekCreateBitmapFont("../res/verdana.ttf", 0, 64, &verdana));
//
//     TekText text;
//     tekChainThrow(tekCreateText("TekPhysics Alpha v1.0", 20, &verdana, &text));
//
//     tekDeleteFreeType();
//
//     glEnable(GL_DEPTH_TEST);
//     glDepthFunc(GL_LESS);
//
//     glEnable(GL_BLEND);
//     glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
//
//     TekMesh walls = {};
//     tekChainThrow(tekReadMesh("../res/wall.tglo", &walls));
//
//     TekMesh cube = {};
//     tekChainThrow(tekReadMesh("../res/cube.tmsh", &cube));
//
//     vec3 camera_position = {5.0f, 3.0f, 5.0f};
//     vec3 light_color = {0.4f, 0.4f, 0.4f};
//     vec3 light_position = {4.0f, 12.0f, 0.0f};
//
//     uint shader_program;
//     tekChainThrow(tekCreateShaderProgramVF("../shader/vertex.glvs", "../shader/fragment.glfs", &shader_program));
//
//     tekBindShaderProgram(shader_program);
//
//     tekShaderUniformVec3(shader_program, "light_color", light_color);
//     tekShaderUniformVec3(shader_program, "light_position", light_position);
//     tekShaderUniformVec3(shader_program, "camera_pos", camera_position);
//
//     mat4 wall_model;
//     mat4 view;
//
//     glm_mat4_identity(wall_model);
//
//     vec3 center = {0.0f, 0.0f, 0.0f};
//     vec3 up = {0.0f, 1.0f, 0.0f};
//     glm_lookat(camera_position, center, up, view);
//
//     glm_perspective(1.2f, width / height, 0.1f, 100.0f, perp_projection);
//
//     tekShaderUniformMat4(shader_program, "view", view);
//
//     vec3 cube_position = {3.0f, 1.0f, 0.0f};
//     mat4 cube_model;
//
//     struct timespec t;
//     clock_gettime(CLOCK_REALTIME, &t);
//     long time_ns = t.tv_sec * 1000000000 + t.tv_nsec;
//     double delta = 0.0;
//     double multiplier = -1.0;
//
//     while (tekRunning()) {
//         tekSetMouseMode(MOUSE_MODE_CAMERA);
//
//         clock_gettime(CLOCK_REALTIME, &t);
//         long delta_ns = (t.tv_sec * 1000000000 + t.tv_nsec) - time_ns;
//         delta = ((double)delta_ns) / 1000000000.0;
//         time_ns = t.tv_sec * 1000000000 + t.tv_nsec;
//
//         glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
//
//
//         tekBindShaderProgram(shader_program);
//         tekChainThrow(tekShaderUniformMat4(shader_program, "projection", perp_projection));
//
//         tekChainThrow(tekShaderUniformMat4(shader_program, "model", wall_model));
//         tekDrawMesh(&walls);
//
//         cube_position[0] += delta * multiplier;
//         if (cube_position[0] < 1.0f) {
//             cube_position[0] = 1.0f;
//             multiplier = 1.0;
//         }
//
//         glm_translate_make(cube_model, cube_position);
//         tekChainThrow(tekShaderUniformMat4(shader_program, "model", cube_model));
//         tekDrawMesh(&cube);
//
//         printException(tekDrawText(&text, width - 185.0f, 5.0f));
//         tekChainThrow(tekUpdate());
//     }
//
//     tekDeleteShaderProgram(shader_program);
//     tekDeleteMesh(&walls);
//     tekDeleteTextEngine();
//     tekDeleteText(&text);
//     tekDelete();
//     return SUCCESS;
// }

exception run() {
    tekChainThrow(tekInit("TekPhysics", 640, 480));
    tekChainThrow(tekAddKeyCallback(tekMainKeyCallback));
    tekChainThrow(tekAddMousePosCallback(tekMainMousePosCallback))
    ThreadQueue state_queue = {};
    tekChainThrow(threadQueueCreate(&event_queue, 4096));
    tekChainThrowThen(threadQueueCreate(&state_queue, 4096), {
        threadQueueDelete(&event_queue);
    });

    Vector entities = {};
    tekChainThrow(vectorCreate(0, sizeof(TekEntity), &entities));

    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LESS);

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glEnable(GL_CULL_FACE);

    vec3 camera_position = {0.0f, 0.0f, 0.0f};
    vec3 camera_rotation = {0.0f, 0.0f, 0.0f};
    const vec3 light_color = {0.4f, 0.4f, 0.4f};
    const vec3 light_position = {4.0f, 12.0f, 0.0f};

    TekCamera camera = {};
    tekChainThrow(tekCreateCamera(&camera, camera_position, camera_rotation, 1.2f, 0.1f, 100.0f));

    TekMesh cube_mesh = {};
    tekChainThrow(tekReadMesh("../res/cube.tmsh", &cube_mesh))

    uint shader_program;
    tekChainThrow(tekCreateShaderProgramVF("../shader/vertex.glvs", "../shader/fragment.glfs", &shader_program));

    TekMaterial material = {};
    tekChainThrow(tekCreateMaterial("../res/material.tmat", &material));

    tekBindShaderProgram(shader_program);

    tekChainThrow(tekShaderUniformVec3(shader_program, "light_color", light_color));
    tekChainThrow(tekShaderUniformVec3(shader_program, "light_position", light_position));
    tekChainThrow(tekShaderUniformVec3(shader_program, "camera_pos", camera_position));

    mat4 model;
    glm_mat4_identity(model);

    tekSetMouseMode(MOUSE_MODE_CAMERA);

    tekChainThrow(tekInitEngine(&event_queue, &state_queue, 1.0 / 30.0));
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
                tekChainThrow(state.data.exception);
                break;
            case ENTITY_CREATE_STATE:
                TekEntity dummy_entity = {};
                if (state.object_id == entities.length) {
                    tekChainThrow(vectorAddItem(&entities, &dummy_entity));
                } else {
                    tekChainThrow(vectorSetItem(&entities, state.object_id, &dummy_entity));
                }
                TekEntity* create_entity = 0;
                vec3 default_scale = { 1.0f, 1.0f, 1.0f };
                tekChainThrow(vectorGetItemPtr(&entities, state.object_id, &create_entity));
                tekChainThrow(tekCreateEntity(state.data.entity.mesh_filename, state.data.entity.material_filename, state.data.entity.position, state.data.entity.rotation, default_scale, create_entity));
                last_entity = create_entity;
                break;
            case ENTITY_UPDATE_STATE:
                TekEntity* update_entity = 0;
                tekChainThrow(vectorGetItemPtr(&entities, state.object_id, &update_entity));
                tekUpdateEntity(update_entity, state.data.entity_update.position, state.data.entity_update.rotation);
                break;
            case ENTITY_DELETE_STATE:
                TekEntity* delete_entity;
                tekChainThrow(vectorGetItemPtr(&entities, state.object_id, &delete_entity));
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
            default:
                break;
            }
        }

        for (uint i = 0; i < entities.length; i++) {
            TekEntity* entity;
            tekChainThrow(vectorGetItemPtr(&entities, i, &entity));
            if (entity->mesh == 0) continue;
            tekChainThrow(tekDrawEntity(entity, &camera));
        }

        tekChainThrow(tekBindMaterial(&material));
        tekChainThrow(tekBindMaterialMatrix(&material, camera.projection, PROJECTION_MATRIX_DATA));
        tekChainThrow(tekBindMaterialMatrix(&material, camera.view, VIEW_MATRIX_DATA));
        tekChainThrow(tekBindMaterialMatrix(&material, model, MODEL_MATRIX_DATA));
        tekChainThrow(tekBindMaterialVec3(&material, camera.position, CAMERA_POSITION_DATA));
        tekDrawMesh(&cube_mesh);
        // printf("Camera position: %f %f %f, rotation: %f %f\n", camera.position[0], camera.position[1], camera.position[2],
        //     camera.rotation[0], camera.rotation[1]);
        tekChainThrow(tekUpdate());
    }
    TekEvent quit_event;
    quit_event.type = QUIT_EVENT;
    quit_event.data.message = 0;
    pushEvent(&event_queue, quit_event);
    tekChainThrow(tekDelete());
    return SUCCESS;
}

int main(void) {
    tekInitExceptions();
    tekLog(run());
    tekCloseExceptions();
}
