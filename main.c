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

#define printException(x) tekLog(x)

float width, height;
mat4 perp_projection;
ThreadQueue event_queue = {};

void tekMainFramebufferCallback(const int fb_width, const int fb_height) {
    width = (float)fb_width;
    height = (float)fb_height;
    glm_perspective(1.2f, width / height, 0.1f, 100.0f, perp_projection);
}

void keyCallback(const int key, const int scancode, const int action, const int mods) {
    TekEvent event = {};
    event.type = KEY_EVENT;
    event.data.key_input.key = key;
    event.data.key_input.scancode = scancode;
    event.data.key_input.action = action;
    event.data.key_input.mods = mods;
    pushEvent(&event_queue, event);
}

int render() {
    tekChainThrow(tekInit("TekPhysics", 640, 480));
    width = 640.0f;
    height = 480.0f;

    tekAddFramebufferCallback(tekMainFramebufferCallback);

    TekBitmapFont verdana;
    printException(tekCreateFreeType());

    tekChainThrow(tekCreateBitmapFont("../res/verdana.ttf", 0, 64, &verdana));

    TekText text;
    tekChainThrow(tekCreateText("TekPhysics Alpha v1.0", 20, &verdana, &text));

    tekDeleteFreeType();

    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LESS);

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    TekMesh walls = {};
    tekChainThrow(tekReadMesh("../res/wall.tglo", &walls));

    TekMesh cube = {};
    tekChainThrow(tekReadMesh("../res/cube.tmsh", &cube));

    vec3 camera_position = {5.0f, 3.0f, 5.0f};
    vec3 light_color = {0.4f, 0.4f, 0.4f};
    vec3 light_position = {4.0f, 12.0f, 0.0f};

    uint shader_program;
    tekChainThrow(tekCreateShaderProgramVF("../shader/vertex.glvs", "../shader/fragment.glfs", &shader_program));

    tekBindShaderProgram(shader_program);

    tekShaderUniformVec3(shader_program, "light_color", light_color);
    tekShaderUniformVec3(shader_program, "light_position", light_position);
    tekShaderUniformVec3(shader_program, "camera_pos", camera_position);

    mat4 wall_model;
    mat4 view;

    glm_mat4_identity(wall_model);

    vec3 center = {0.0f, 0.0f, 0.0f};
    vec3 up = {0.0f, 1.0f, 0.0f};
    glm_lookat(camera_position, center, up, view);

    glm_perspective(1.2f, width / height, 0.1f, 100.0f, perp_projection);

    tekShaderUniformMat4(shader_program, "view", view);

    vec3 cube_position = {3.0f, 1.0f, 0.0f};
    mat4 cube_model;

    struct timespec t;
    clock_gettime(CLOCK_REALTIME, &t);
    long time_ns = t.tv_sec * 1000000000 + t.tv_nsec;
    double delta = 0.0;
    double multiplier = -1.0;

    while (tekRunning()) {
        clock_gettime(CLOCK_REALTIME, &t);
        long delta_ns = (t.tv_sec * 1000000000 + t.tv_nsec) - time_ns;
        delta = ((double)delta_ns) / 1000000000.0;
        time_ns = t.tv_sec * 1000000000 + t.tv_nsec;

        glClearColor(0.0f, 0.0f, 0.0f, 1.0f);


        tekBindShaderProgram(shader_program);
        tekChainThrow(tekShaderUniformMat4(shader_program, "projection", perp_projection));

        tekChainThrow(tekShaderUniformMat4(shader_program, "model", wall_model));
        tekDrawMesh(&walls);

        cube_position[0] += delta * multiplier;
        if (cube_position[0] < 1.0f) {
            cube_position[0] = 1.0f;
            multiplier = 1.0;
        }

        glm_translate_make(cube_model, cube_position);
        tekChainThrow(tekShaderUniformMat4(shader_program, "model", cube_model));
        tekDrawMesh(&cube);

        printException(tekDrawText(&text, width - 185.0f, 5.0f));
        tekChainThrow(tekUpdate());
    }

    tekDeleteShaderProgram(shader_program);
    tekDeleteMesh(&walls);
    tekDeleteTextEngine();
    tekDeleteText(&text);
    tekDelete();
    return SUCCESS;
}

exception yeah() {
    TekMesh mesh = {};
    tekChainThrow(tekReadMesh("../res/mesh.tmsh", &mesh));
    return SUCCESS;
}

exception ymlTest() {
    YmlFile yml_file = {};
    tekChainThrow(ymlReadFile("../res/test.yml", &yml_file));
    tekChainThrow(ymlPrint(&yml_file));
    return SUCCESS;
}

#define THREAD_FUNC(func_name, start_val, print_format) \
void func_name(void* args) { \
    ThreadQueue* q = (ThreadQueue*)args; \
    uint sv = start_val; \
    while (1) { \
        if (sv) { \
            for (uint i = 10; i < 20; i++) { \
                tekLog(threadQueueEnqueue(q, i)); \
            } \
        } \
        while (!threadQueueIsEmpty(q)) { \
            void* data = 0; \
            tekLog(threadQueueDequeue(q, &data)); \
            printf(print_format, (uint)data); \
        } \
        sv = 1; \
    } \
} \

THREAD_FUNC(threadA, 0, "thread a: %u\n");
THREAD_FUNC(threadB, 1, "thread b: %u\n");

exception queueTest() {
    ThreadQueue q = {};
    threadQueueCreate(&q);

    pthread_t thread_a, thread_b;
    pthread_create(&thread_a, NULL, threadA, &q);
    pthread_create(&thread_b, NULL, threadB, &q);

    sleep(10);

    threadQueueDelete(&q);
    return SUCCESS;
}

exception run() {
    tekChainThrow(tekInit("TekPhysics", 640, 480));
    tekChainThrow(tekAddKeyCallback(keyCallback));
    ThreadQueue state_queue = {};
    tekChainThrow(threadQueueCreate(&event_queue));
    tekChainThrowThen(threadQueueCreate(&state_queue), {
        threadQueueDelete(&event_queue);
    });
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
            default:
                break;
            }
        }
        tekChainThrow(tekUpdate());
    }
    TekEvent quit_event;
    quit_event.type = QUIT_EVENT;
    quit_event.data.message = 0;
    pushEvent(&event_queue, quit_event);
    tekChainThrow(tekDelete());
    return SUCCESS;
}

exception vectorTest() {
    Vector vector = {};
    tekChainThrow(vectorCreate(0, sizeof(long), &vector));

    long test = 100;
    for (uint i = 0; i < 100; i++) {
        test += 1;
        printf("adding `%ld`\n", test);
        tekChainThrow(vectorAddItem(&vector, &test));
    }

    long output = 0;
    for (uint i = 0; i < vector.length; i++) {
        tekChainThrow(vectorGetItem(&vector, i, &output));
        printf("vector@%d = %ld\n", i, output);
    }

    while (vector.length) {
        tekChainThrow(vectorRemoveItem(&vector, 50, &output));
        printf("removed %ld\n", output);
    }

    vectorDelete(&vector);
    return SUCCESS;
}

int main(void) {
    tekInitExceptions();
    tekLog(vectorTest());
    tekCloseExceptions();
}
