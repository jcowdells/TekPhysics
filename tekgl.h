#pragma once

typedef char flag;
typedef unsigned char byte;
typedef unsigned int uint;

/**
 * Take a 3 component vector and expand it, used for printing mostly.
 */
#define EXPAND_VEC3(vector) vector[0], vector[1], vector[2]

/**
 * Take a 4 component vector and expand it, used for printing mostly.
 */
#define EXPAND_VEC4(vector) vector[0], vector[1], vector[2], vector[3]

// different modes that the simulation can be in
#define MODE_MAIN_MENU 0
#define MODE_BUILDER   1
#define MODE_RUNNER    2
#define MODE_SAVE      3
#define MODE_LOAD      4

// in case i accidentally type the wrong amount of zeroes.
#define BILLION 1000000000