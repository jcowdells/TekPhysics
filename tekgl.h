#pragma once

typedef char flag;
typedef unsigned char byte;
typedef unsigned int uint;

#define EXPAND_VEC3(vector) vector[0], vector[1], vector[2]
#define EXPAND_VEC4(vector) vector[0], vector[1], vector[2], vector[3]

#define MODE_MAIN_MENU 0
#define MODE_BUILDER   1
#define MODE_RUNNER    2
#define MODE_SAVE      3
#define MODE_LOAD      4

#define BILLION 1000000000