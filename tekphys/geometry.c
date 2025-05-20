#include "geometry.h"

#include "../tekgl.h"
#include <cglm/vec4.h>
#include <cglm/mat4.h>

float tetrahedronSignedVolume(vec3 point_a, vec3 point_b, vec3 point_c, vec3 point_d) {
    // volume of a tetrahedron can be computed using a matrix
    // https://en.wikipedia.org/wiki/Tetrahedron#Volume

    mat4 matrix;
    for (uint i = 0; i < 3; i++) {
        matrix[0][i] = point_a[i];
        matrix[1][i] = point_b[i];
        matrix[2][i] = point_c[i];
        matrix[3][i] = point_d[i];
    }
    for (uint i = 0; i < 4; i++) {
        matrix[i][3] = 1.0f;
    }
    float determinant = glm_mat4_det(matrix);
    return determinant / 6.0f;
}
