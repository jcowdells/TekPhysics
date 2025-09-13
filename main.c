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

static void updateDepthMessage() {
    if (!text_init) return;
    if (depth_text.font) tekDeleteText(&depth_text);
    sprintf(depth_message, "View depth: %03d", depth);
    memset(&depth_text, 0, sizeof(TekText));
    tekLog(tekCreateText(depth_message, 30, &depth_font, &depth_text));
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

    tekChainThrow(tekCreateFreeType());
    tekChainThrow(tekCreateBitmapFont("../res/verdana_bold.ttf", 0, 64, &depth_font));
    text_init = 1;
    updateDepthMessage();

    TekText version_text;
    tekCreateText("TekPhysics v1.0 Alpha", 16, &depth_font, &version_text);

    Vector entities = {};
    tekChainThrow(vectorCreate(0, sizeof(TekEntity), &entities));

    Vector colliders = {};
    tekChainThrow(vectorCreate(0, sizeof(TekCollider), &colliders));

    Vector collider_boxes = {};
    tekChainThrow(vectorCreate(0, 5 * sizeof(vec3), &collider_boxes));

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
    // tekChainThrow(tekShaderUniformVec3(shader_program, "light_position", light_position));
    // tekChainThrow(tekShaderUniformVec3(shader_program, "camera_pos", camera_position));

    mat4 model;
    glm_mat4_identity(model);

    // tekSetMouseMode(MOUSE_MODE_CAMERA);

    TekEntity sphere;
    tekChainThrow(tekCreateEntity("../res/rad1.tmsh", "../res/translucent.tmat", (vec3){0.0f, 0.0f, 0.0f}, (vec4){0.0f, 0.0f, 0.0f, 1.0f}, (vec3){1.0f, 1.0f, 1.0f}, &sphere));

    TekMaterial translucent = {};
    tekChainThrow(tekCreateMaterial("../res/translucent.tmat", &translucent));

    Vector collider_stack = {};
    vectorCreate(1, sizeof(TekColliderNode*), &collider_stack);

    uint cdr_vertex_array;
    glGenVertexArrays(1, &cdr_vertex_array);
    glBindVertexArray(cdr_vertex_array);

    uint cdr_vertex_buffer;
    glGenBuffers(1, &cdr_vertex_buffer);
    glBindBuffer(GL_ARRAY_BUFFER, cdr_vertex_buffer);

    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 15 * sizeof(float), 0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 15 * sizeof(float), (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, 15 * sizeof(float), (void*)(6 * sizeof(float)));
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(3, 3, GL_FLOAT, GL_FALSE, 15 * sizeof(float), (void*)(9 * sizeof(float)));
    glEnableVertexAttribArray(3);
    glVertexAttribPointer(4, 3, GL_FLOAT, GL_FALSE, 15 * sizeof(float), (void*)(12 * sizeof(float)));
    glEnableVertexAttribArray(4);

    glBindVertexArray(0);

    uint translucent_shader;
    tekChainThrow(tekCreateShaderProgramVGF("../shader/translucent.glvs", "../shader/translucent.glgs", "../shader/translucent.glfs", &translucent_shader));

    tekChainThrow(tekInitEngine(&event_queue, &state_queue, 1.0 / 30.0));
    TekState state = {};

    uint trg_vertex_array;
    glGenVertexArrays(1, &trg_vertex_array);
    glBindVertexArray(trg_vertex_array);

    uint trg_vertex_buffer;
    glGenBuffers(1, &trg_vertex_buffer);
    glBindBuffer(GL_ARRAY_BUFFER, trg_vertex_buffer);

    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), 0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);

    vec3 triangle_buffer[12];
    glBindBuffer(GL_ARRAY_BUFFER, trg_vertex_buffer);
    glBufferData(GL_ARRAY_BUFFER, 12 * 3 * sizeof(float), (void*)triangle_buffer, GL_STATIC_DRAW);
    glBindVertexArray(0);

    flag collision = 0;

    Vector triangle_write_buffer;
    tekChainThrow(vectorCreate(1, 3 * sizeof(vec3), &triangle_write_buffer));

    Vector triangle_display_buffer;
    tekChainThrow(vectorCreate(1, 6 * sizeof(vec3), &triangle_display_buffer));

    TekGuiWindow window = {};
    tekGuiCreateWindow(&window);

    TekGuiWindow window2 = {};
    tekGuiCreateWindow(&window2);

    TekGuiButton button = {};
    tekGuiCreateButton(&button);

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
            case __COLLIDER_STATE:
                if (state.object_id == colliders.length) {
                    tekChainThrow(vectorAddItem(&colliders, &state.data.collider));
                } else {
                    tekChainThrow(vectorSetItem(&colliders, state.object_id, &state.data.collider));
                }

                TekColliderNode* node = 0;
                tekChainThrow(vectorGetItem(&colliders, state.object_id, &node));

                collider_stack.length = 0;
                vectorAddItem(&collider_stack, &node);
                uint num_iters = 1;
                while (vectorRemoveItem(&collider_stack, 0, &node) != VECTOR_EXCEPTION) {
                    if (num_iters++ >= 1000) {
                        break;
                    }
                    if (node->type == COLLIDER_NODE) {
                        vectorAddItem(&collider_stack, &node->data.node.left);
                        vectorAddItem(&collider_stack, &node->data.node.right);
                    }
                    vec3 vertex_data[5];
                    glm_vec3_copy(node->obb.centre, vertex_data[0]);
                    vertex_data[1][0] = node->obb.half_extents[0];
                    vertex_data[1][1] = node->obb.half_extents[1];
                    vertex_data[1][2] = node->obb.half_extents[2];
                    glm_vec3_copy(node->obb.axes[0], vertex_data[2]);
                    glm_vec3_copy(node->obb.axes[1], vertex_data[3]);
                    glm_vec3_copy(node->obb.axes[2], vertex_data[4]);
                    tekChainThrow(vectorAddItem(&collider_boxes, &vertex_data));
                    node = 0;
                }

                glBindVertexArray(cdr_vertex_array);
                glBindBuffer(GL_ARRAY_BUFFER, cdr_vertex_buffer);
                glBufferData(GL_ARRAY_BUFFER, (GLsizeiptr)(collider_boxes.length * 5 * 3 * sizeof(float)), collider_boxes.internal, GL_STATIC_DRAW);
                glBindVertexArray(0);

                // for (uint i = 0; i < collider_boxes.length; i++) {
                //     vec3* vertex_data;
                //     vectorGetItemPtr(&collider_boxes, i, &vertex_data);
                //     printf("Axis: %f %f %f\n", EXPAND_VEC3(vertex_data[2]));
                // }

                break;
            // case __TRI_PAIR_STATE:
            //     //printf("Triangle state: %u\n", state.object_id);
            //
            //     memcpy(triangle_buffer, state.data.triangle_pair.vertices, 3 * 3 * sizeof(float));
            //     memcpy(triangle_buffer + 6, (vec3*)state.data.triangle_pair.vertices + 3, 3 * 3 * sizeof(float));
            //
            //     for (uint i = 0; i < 6; i++) {
            //         memcpy(triangle_buffer + i * 2, (vec3*)state.data.triangle_pair.vertices + i, 3 * sizeof(float));
            //         glm_vec3_copy((vec3){0.0f, 1.0f, 0.0f}, triangle_buffer[2 * i + 1]);
            //     }
            //
            //     glBindVertexArray(trg_vertex_array);
            //     glBindBuffer(GL_ARRAY_BUFFER, trg_vertex_buffer);
            //     glBufferData(GL_ARRAY_BUFFER, (GLsizeiptr)(12 * 3 * sizeof(float)), triangle_buffer, GL_STATIC_DRAW);
            //     glBindVertexArray(0);
            //
            //     // for (uint i = 0; i < 12; i++) {
            //     //     float* vector = (vec3*)(state.data.collider) + i;
            //     //     for (uint j = 0; j < 3; j++) {
            //     //         printf("%f ", triangle_buffer[i][j]);
            //     //     }
            //     //     printf("\n");
            //     // }
            //
            //     collision = state.data.triangle_pair.collision;
            //
            //     break;
            case __TRI_BFFR_STATE:
                tekChainThrow(vectorAddItem(&triangle_write_buffer, state.data.triangle));
                break;
            case __TRI_BCLR_STATE:
                triangle_display_buffer.length = 0;
                vec3 tri_buffer[6];
                for (uint i = 0; i < triangle_write_buffer.length; i++) {
                    vec3* tri_ptr;
                    tekChainThrow(vectorGetItemPtr(&triangle_write_buffer, i, &tri_ptr));
                    vec3 normal, vector_a, vector_b;
                    glm_vec3_sub(tri_ptr[1], tri_ptr[0], vector_a);
                    glm_vec3_sub(tri_ptr[2], tri_ptr[0], vector_b);
                    glm_vec3_cross(vector_a, vector_b, normal);
                    for (uint j = 0; j < 3; j++) {
                        glm_vec3_copy(tri_ptr[j], tri_buffer[j * 2]);
                        glm_vec3_copy(normal, tri_buffer[j * 2 + 1]);
                    }
                    tekChainThrow(vectorAddItem(&triangle_display_buffer, tri_buffer));
                }
                triangle_write_buffer.length = 0;

                glBindVertexArray(trg_vertex_array);
                glBindBuffer(GL_ARRAY_BUFFER, trg_vertex_buffer);
                glBufferData(GL_ARRAY_BUFFER, (GLsizeiptr)(triangle_display_buffer.length * 6 * sizeof(float)), triangle_display_buffer.internal, GL_STATIC_DRAW);
                glBindVertexArray(0);

                break;
            default:
                break;
            }
        }

        //glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
        for (uint i = 0; i < entities.length; i++) {
            TekEntity* entity;
            tekChainThrow(vectorGetItemPtr(&entities, i, &entity));
            if (entity->mesh == 0) continue;
            tekChainThrow(tekDrawEntity(entity, &camera));
        }

        //glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
        tekBindShaderProgram(translucent_shader);
        tekChainThrow(tekShaderUniformMat4(translucent_shader, "projection", camera.projection));
        tekChainThrow(tekShaderUniformMat4(translucent_shader, "view", camera.view));
        mat4 collider_model;
        if (entities.length == 0) {
            glm_mat4_identity(collider_model);
        } else {
            TekEntity* first_entity;
            vectorGetItemPtr(&entities, 0, &first_entity);
            mat4 translation;

            glm_translate_make(translation, first_entity->position);
            mat4 rotation;
            glm_quat_mat4(first_entity->rotation, rotation);
            mat4 scale;
            glm_scale_make(scale, first_entity->scale);
            glm_mat4_mul(rotation, scale, collider_model);
            glm_mat4_mul(translation, collider_model, collider_model);
        }
        tekChainThrow(tekShaderUniformMat4(translucent_shader, "model", collider_model));
        tekChainThrow(tekShaderUniformVec4(translucent_shader, "light_color", (vec4){1.0f, 0.0f, 0.0f, 0.5f}));
        glBindVertexArray(cdr_vertex_array);
        const uint n = 1 << depth;
        uint count = (n << 1) - n;
        if (collider_boxes.length > 0)
           glDrawArrays(GL_POINTS, (GLint)(n - 1), (GLint)count);

        tekBindShaderProgram(shader_program);
        tekChainThrow(tekShaderUniformMat4(shader_program, "projection", camera.projection));
        tekChainThrow(tekShaderUniformMat4(shader_program, "view", camera.view));
        tekChainThrow(tekShaderUniformMat4(shader_program, "model", collider_model));
        tekChainThrow(tekShaderUniformVec3(shader_program, "light_color", collision ? (vec3){1.0f, 0.0f, 0.0f} : (vec3){0.0f, 1.0f, 0.0f}));
        // tekChainThrow(tekShaderUniformVec4(shader_program, "camera_pos", camera.position));
        // tekChainThrow(tekShaderUniformVec4(shader_program, "light_position", (vec4){0.0f, 3.0f, 0.0f}));

        glBindVertexArray(trg_vertex_array);
        glDrawArrays(GL_TRIANGLES, 0, (int)(triangle_display_buffer.length * 3));

        tekNotifyEntityMaterialChange();
        //glPolygonMode(GL_FRONT, GL_FILL);
        tekChainThrow(tekGuiDrawWindow(&window));
        tekChainThrow(tekGuiDrawWindow(&window2));

        // glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
        tekChainThrow(tekDrawText(&depth_text, 10, 10));

        int window_width, window_height;
        tekGetWindowSize(&window_width, &window_height);
        tekChainThrow(tekDrawText(&version_text, (float)(window_width - 170), 8.0f));

        // tekChainThrow(tekBindMaterial(&material));
        // tekChainThrow(tekBindMaterialMatrix(&material, camera.projection, PROJECTION_MATRIX_DATA));
        // tekChainThrow(tekBindMaterialMatrix(&material, camera.view, VIEW_MATRIX_DATA));
        // tekChainThrow(tekBindMaterialMatrix(&material, model, MODEL_MATRIX_DATA));
        // tekChainThrow(tekBindMaterialVec3(&material, camera.position, CAMERA_POSITION_DATA));
        // tekDrawMesh(&cube_mesh);
        // printf("Camera position: %f %f %f, rotation: %f %f\n", camera.position[0], camera.position[1], camera.position[2],
        //     camera.rotation[0], camera.rotation[1]);
        tekChainThrow(tekUpdate());
    }
    TekEvent quit_event;
    quit_event.type = QUIT_EVENT;
    quit_event.data.message = 0;
    pushEvent(&event_queue, quit_event);
    vectorDelete(&entities);
    vectorDelete(&colliders);
    vectorDelete(&collider_stack);
    tekDeleteMaterial(&translucent);
    tekDeleteFreeType();
    tekChainThrow(tekDelete());
    return SUCCESS;
}

exception test() {
    TekCollisionManifold manifold = {};

    vec3 triangle_a[] = {
        -3.899999f, 1.000000f, 0.400000f,
        -3.899999f, -1.000000f, -1.600000f,
        -3.899999f, -1.000000f, 0.400000f
    };

    vec3 triangle_b[] = {
        1.000000f, -1.000000f, 1.000000f,
        -1.000000f, -1.000000f, 1.000000f,
        1.000000f, -1.000000f, -1.000000f
    };

    flag collision = 0;

    tekCheckTriangleCollision(triangle_a, triangle_b, &collision, &manifold);

    printf("Collision: %s", collision ? "True" : "False");

    return SUCCESS;
}

int main(void) {
    tekInitExceptions();
    tekLog(run());
    tekCloseExceptions();
}
