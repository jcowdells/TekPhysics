#pragma once

#include <cglm/vec3.h>

float tetrahedronSignedVolume(const vec3 point_a, const vec3 point_b, const vec3 point_c, const vec3 point_d);
void tetrahedronInertiaTensor(const vec3 point_a, const vec3 point_b, const vec3 point_c, const vec3 point_d, float mass, mat3 tensor);
void translateInertiaTensor(mat3 tensor, float mass, vec3 translate);

void mat3Add(mat3 a, mat3 b, mat3 m);