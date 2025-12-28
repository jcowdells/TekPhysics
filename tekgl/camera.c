#include "camera.h"

#include <stdio.h>
#include <string.h>
#include <cglm/affine.h>
#include "manager.h"
#include "../core/list.h"
#include <cglm/cam.h>

static List cameras = {0, 0};
static float aspect_ratio = 1.0f;

/**
 * Update the projection matrix of the camera.
 * @param camera A pointer to the camera to update.
 */
static void tekUpdateCameraProjection(TekCamera* camera) {
    // glm provides method to create perspective matrix.
    glm_perspective(camera->fov, aspect_ratio, camera->near, camera->far, camera->projection);
}

/**
 * Update the view matrix of the camera.
 * @param camera A pointer to the camera to update.
 */
static void tekUpdateCameraView(TekCamera* camera) {
    const double pitch = (double)camera->rotation[1], yaw = (double)camera->rotation[0];
    vec3 look_at = {};
    vec3 up = { 0.0f, 1.0f, 0.0f };

    // create a vector based on pitch and yaw, that points in the same direction as the camera.
    look_at[0] = (float)(cos(yaw) * cos(pitch)) + camera->position[0];
    look_at[1] = (float)sin(pitch) + camera->position[1];
    look_at[2] = (float)(sin(yaw) * cos(pitch)) + camera->position[2];

    // then, look at this point. so we look in the same way as expected.
    glm_lookat(camera->position, look_at, up, camera->view);
}

/**
 * Called when the framebuffer size changes, e.g. when the user resizes the window.
 * @param framebuffer_width The new width of the framebuffer.
 * @param framebuffer_height The new height of the framebuffer.
 */
static void tekCameraFramebufferCallback(const int framebuffer_width, const int framebuffer_height) {
    // calculate aspect ratio
    aspect_ratio = (float)framebuffer_width / (float)framebuffer_height;

    // update every camera with the new aspect ratio.
    const ListItem* item;
    foreach(item, (&cameras), {
        TekCamera* camera = (TekCamera*)item->data;
        tekUpdateCameraProjection(camera);
    });
}

/**
 * Called when program exits, cleans up some structures.
 */
static void tekCameraDeleteFunc() {
    listDelete(&cameras); // delete list of cameras.
}

/**
 * Initialise some structures needed to have cameras.
 */
tek_init tekCameraInit() {
    // framebuffer callback to wait for when viewport changes
    tekAddFramebufferCallback(tekCameraFramebufferCallback);

    // call delete func when the program exits
    tekAddDeleteFunc(tekCameraDeleteFunc);

    // create empty list of cameras.
    listCreate(&cameras);
}

/**
 * Update the position of a camera and update any matrices this would affect.
 * @param camera The camera to update.
 * @param position The new position of the camera.
 */
void tekSetCameraPosition(TekCamera* camera, vec3 position) {
    memcpy(camera->position, position, sizeof(vec3)); // copy in position
    tekUpdateCameraView(camera); // update matrices.
}

/**
 * Set the rotation of the camera and upate the matrices it affects.
 * @param camera The camera to update.
 * @param rotation The new rotation of the camera.
 */
void tekSetCameraRotation(TekCamera* camera, vec3 rotation) {
    memcpy(camera->rotation, rotation, sizeof(vec3)); // copy rotation in
    tekUpdateCameraView(camera); // update
}

/**
 * Create a camera struct, a container for some information about a camera + matrices needed to render with this camera.
 * @param camera A pointer to an empty TekCamera struct.
 * @param position The position of the camera.
 * @param rotation The rotation of the camera.
 * @param fov The fov (field of view) of the camera in radians.
 * @param near The near clipping plane of the camera. (things closer than this will be invisible)
 * @param far The far clipping plane of the camera. (things further than this will be invisible)
 * @throws LIST_EXCEPTION if could not add camera to list of cameras.
 */
exception tekCreateCamera(TekCamera* camera, vec3 position, vec3 rotation, const float fov, const float near, const float far) {
    // when window is resized, need to update cameras to match new aspect ratio
    // so all cameras need to be recorded to be updated when needed.
    tekChainThrow(listAddItem(&cameras, camera));

    // set data
    memcpy(camera->position, position, sizeof(vec3));
    memcpy(camera->rotation, rotation, sizeof(vec3));
    camera->fov = fov;
    camera->near = near;
    camera->far = far;

    // generate matrices.
    tekUpdateCameraView(camera);
    tekUpdateCameraProjection(camera);
    return SUCCESS;
}
