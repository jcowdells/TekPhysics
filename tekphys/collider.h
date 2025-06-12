#pragma once

#include "../tekgl.h"
#include "../core/exception.h"

#include <cglm/vec3.h>

#define COLLIDER_LEAF 0
#define COLLIDER_NODE 1

struct TekBody;
typedef struct TekBody TekBody;

typedef struct  TekColliderNode {
    flag type;
    uint id;
    vec3 centre;
    float radius;
    union {
        struct {
            struct TekColliderNode* left;
            struct TekColliderNode* right;
        } node;
        struct {
            uint indices[3];
        } leaf;
    } data;
} TekColliderNode;

typedef TekColliderNode* TekCollider;

exception tekCreateCollider(TekBody* body, TekCollider* collider);
void tekDeleteCollider(TekCollider* collider);
