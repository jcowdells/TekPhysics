#pragma once

#include "../tekgl.h"
#include "../core/exception.h"

#include <cglm/vec3.h>

#define COLLIDER_LEAF 0
#define COLLIDER_NODE 1

struct TekBody;
typedef struct TekBody TekBody;

struct OBB {
    vec3 centre;
    vec3 axes[3];
    float half_extents[3];
};

typedef struct TekColliderNode {
    flag type;
    uint id;
    struct OBB obb;
    union {
        struct {
            uint num_indices;
            uint* indices;
            struct TekColliderNode* left;
            struct TekColliderNode* right;
        } node;
        struct {
            vec3* vertices;
            uint num_vertices;
        } leaf;
    } data;
} TekColliderNode;

typedef TekColliderNode* TekCollider;

exception tekCreateCollider(TekBody* body, TekCollider* collider);
void tekDeleteCollider(TekCollider* collider);
