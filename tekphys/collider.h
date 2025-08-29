#pragma once

#include "../tekgl.h"
#include "../core/exception.h"
#include "../core/vector.h"

#include <cglm/vec3.h>

#define COLLIDER_LEAF 0
#define COLLIDER_NODE 1

struct TekBody;
typedef struct TekBody TekBody;

struct OBB {
    vec3 centre;
    vec3 axes[3];
    float half_extents[3];

    vec3 w_centre;
    vec3 w_axes[3];
    float w_half_extents[3];
};

typedef struct TekColliderNode {
    flag type;
    uint id;
    struct OBB obb;
    uint* indices;
    uint num_indices;
    union {
        struct {
            struct TekColliderNode* left;
            struct TekColliderNode* right;
        } node;
        struct {
            vec3* vertices;
            vec3* w_vertices;
            uint num_vertices;
        } leaf;
    } data;
} TekColliderNode;

typedef TekColliderNode* TekCollider;

typedef struct TekCollisionManifold {
    vec3 contact_points[2];
    vec3 contact_normal;
    vec3 tangent_vectors[2];
    float penetration_depth;
} TekCollisionManifold;

exception tekCreateCollider(const TekBody* body, TekCollider* collider);
void tekDeleteCollider(TekCollider* collider);
exception tekTestForCollisions(TekBody* a, TekBody* b, flag* collision, Vector* contact_points);
flag testCollision(TekBody* a, TekBody* b);
void tekSwapTriangleVertices(vec3 vertices[3], uint swap_index);
exception tekCheckTriangleCollision(vec3 triangle_a[3], vec3 triangle_b[3], flag* collision, Vector* contact_points);
