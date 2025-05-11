#pragma once

#include "../tekgl.h"
#include "../core/exception.h"

#include <cglm/vec3.h>
#include <cglm/mat4.h>

typedef struct TekCamera {
    vec3 position;
    vec3 rotation;
    mat4 view;
    mat4 projection;
    float fov;
    float near;
    float far;
} TekCamera;

exception tekCreateCamera(TekCamera* camera, vec3 position, vec3 rotation, float fov, float near, float far);
void tekSetCameraPosition(TekCamera* camera, vec3 position);
void tekSetCameraRotation(TekCamera* camera, vec3 rotation); 
