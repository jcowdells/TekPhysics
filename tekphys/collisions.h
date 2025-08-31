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

#define NUM_ITERATIONS 10

#define BAUMGARTE_BETA 0.20f

typedef struct TekCollisionManifold {
    vec3 contact_points[2];
    vec3 contact_normal;
    vec3 tangent_vectors[2];
    vec3 r_ac;
    vec3 r_cb;
    float penetration_depth;
    float baumgarte_stabilisation;
    float impulses[NUM_CONSTRAINTS];
} TekCollisionManifold;

exception tekGetCollisionManifold(TekBody* body_a, TekBody* body_b, flag* collision, TekCollisionManifold* manifold);
exception tekApplyCollision(TekBody* body_a, TekBody* body_b, const Vector* contact_manifolds, float phys_period);
exception tekSolveCollisions(const Vector* bodies, float phys_period);