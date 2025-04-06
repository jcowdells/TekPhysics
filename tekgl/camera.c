#include "camera.h"

#include <cglm/affine.h>
#include <cglm/cam.h>

void tekSetCameraPosition(TekCamera* camera, vec3 position) {
    vec3 p = {11.0, 12.0, 13.0};
    glm_translate_make(camera->translation, p);
    for (uint i = 0; i < 16; i++) {
        if ((i != 0) && (i % 4 == 0)) printf("\n");
        printf("%f ", camera->translation[i / 4][i % 4]);
    }
}

void tekSetCameraRotation(TekCamera* camera, vec3 rotation) {

}

void tekCreateCamera(TekCamera* camera, vec3 position, vec3 rotation) {

}
