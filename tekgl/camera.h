#include "../tekgl.h"

#include <cglm/vec3.h>
#include <cglm/mat4.h>

typedef struct TekCamera {
    mat4 translation;
    mat4 rotation;
    mat4 transformation;
} TekCamera;

void tekCreateCamera(TekCamera* camera, vec3 position, vec3 rotation);
void tekSetCameraPosition(TekCamera* camera, vec3 position);
void tekSetCameraRotation(TekCamera* camera, vec3 rotation); 
