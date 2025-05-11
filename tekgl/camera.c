#include "camera.h"

#include <string.h>
#include <cglm/affine.h>
#include "manager.h"
#include "../core/list.h"
#include <cglm/cam.h>

static List cameras;

static void tekCameraFramebufferCallback(const int framebuffer_width, const int framebuffer_height) {
    const float aspect_ratio = (float)framebuffer_width / (float)framebuffer_height;
    const ListItem* item;
    foreach(item, (&cameras), {
        TekCamera* camera = (TekCamera*)item->data;
        glm_perspective(camera->fov, aspect_ratio, camera->near, camera->far, camera->projection);
    });
}

static void tekCameraDeleteFunc() {
    listDelete(&cameras);
}

tek_init tekCameraInit() {
    tekAddFramebufferCallback(tekCameraFramebufferCallback);
    tekAddDeleteFunc(tekCameraDeleteFunc);
    listCreate(&cameras);
}

void tekUpdateCameraView(TekCamera* camera) {
    const double pitch = (double)camera->rotation[1], yaw = (double)camera->rotation[0];
    vec3 look_at = {};
    vec3 up = { 0.0f, 1.0f, 0.0f };
    look_at[0] = (float)(cos(yaw) * cos(pitch)) + camera->position[0];
    look_at[1] = (float)sin(pitch) + camera->position[1];
    look_at[2] = (float)(sin(yaw) * cos(pitch)) + camera->position[2];
    glm_lookat(camera->position, look_at, up, camera->view);
}

void tekSetCameraPosition(TekCamera* camera, vec3 position) {
    memcpy(camera->position, position, sizeof(vec3));
    tekUpdateCameraView(camera);
}

void tekSetCameraRotation(TekCamera* camera, vec3 rotation) {
    memcpy(camera->rotation, rotation, sizeof(vec3));
    tekUpdateCameraView(camera);
}

exception tekCreateCamera(TekCamera* camera, vec3 position, vec3 rotation, const float fov, const float near, const float far) {
    tekChainThrow(listAddItem(&cameras, camera));
    memcpy(camera->position, position, sizeof(vec3));
    memcpy(camera->rotation, rotation, sizeof(vec3));
    camera->fov = fov;
    camera->near = near;
    camera->far = far;
    tekUpdateCameraView(camera);
    return SUCCESS;
}
