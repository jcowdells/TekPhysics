#pragma once

#include <cglm/vec3.h>

#include "../tekgl.h"
#include "../core/exception.h"
#include "../core/vector.h"
#include "body.h"

#define NORMAL_CONSTRAINT 0
#define TANGENT_CONSTRAINT_1 1
#define TANGENT_CONSTRAINT_2 2
#define NUM_CONSTRAINTS 3

#define NUM_ITERATIONS 32

#define BAUMGARTE_BETA 0.10f

typedef struct TekCollisionManifold {
    TekBody* bodies[2];
    vec3 contact_points[2];
    vec3 contact_normal;
    vec3 tangent_vectors[2];
    vec3 r_ac;
    vec3 r_bc;
    float penetration_depth;
    float baumgarte_stabilisation;
    float impulses[NUM_CONSTRAINTS];
} TekCollisionManifold;

exception tekCheckTriangleCollision(vec3 triangle_a[3], vec3 triangle_b[3], flag* collision, TekCollisionManifold* manifold);
exception tekGetCollisionManifolds(TekBody* body_a, TekBody* body_b, flag* collision, Vector* manifold_vector);
exception tekApplyCollision(TekBody* body_a, TekBody* body_b, TekCollisionManifold* manifold);
exception tekSolveCollisions(const Vector* bodies, float phys_period);
