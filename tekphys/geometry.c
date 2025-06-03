#include "geometry.h"

#include "../tekgl.h"
#include <cglm/vec4.h>
#include <cglm/mat3.h>
#include <cglm/mat4.h>

/**
 * @brief Calculate the signed volume of a tetrahedron from its four vertices.
 *
 * @param point_a Vertex A of tetrahedron
 * @param point_b Vertex B of tetrahedron
 * @param point_c Vertex C of tetrahedron
 * @param point_d Vertex D of tetrahedron
 * @return The signed volume of the tetrahedron (e.g. can be either positive or negative volume)
 */
float tetrahedronSignedVolume(const vec3 point_a, const vec3 point_b, const vec3 point_c, const vec3 point_d) {
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
    const float determinant = glm_mat4_det(matrix);
    return determinant / 6.0f;
}

/**
 * @brief Find the outer product of two vectors, a⊗b
 * @param a Left-hand vector
 * @param b Right-hand vector
 * @param m Resultant matrix
 */
static void mat3OuterProduct(const vec3 a, const vec3 b, mat3 m) {
    for (uint i = 0; i < 3; i++) {
        for (uint j = 0; j < 3; j++) {
            m[i][j] = a[j] * b[i];
        }
    }
}

/**
 * @brief Find the sum of two matrices.
 *
 * @note Acceptable for a or b to be the same matrix as m.
 *
 * @param a Left-hand matrix
 * @param b Right-hand matrix
 * @param m Result
 */
void mat3Add(mat3 a, mat3 b, mat3 m) {
    for (uint i = 0; i < 3; i++) {
        for (uint j = 0; j < 3; j++) {
            m[i][j] = a[i][j] + b[i][j];
        }
    }
}

/**
 * @brief Find the difference between two matrices.
 *
 * @note Acceptable for a or b to be the same matrix as m.
 *
 * @param a Left-hand matrix
 * @param b Right-hand matrix
 * @param m Result
 */
static void mat3Subtract(mat3 a, mat3 b, mat3 m) {
    for (uint i = 0; i < 3; i++) {
        for (uint j = 0; j < 3; j++) {
            m[i][j] = a[i][j] - b[i][j];
        }
    }
}

static float momentOfInertia(const vec4 a, const vec4 b, const float mass) {
    // calculating moments of inertia e.g. values of inertia tensor along diagonal

    // formula found here: https://docsdrive.com/pdfs/sciencepublications/jmssp/2005/8-11.pdf
    // "Explicit and exact formulas for the 3-D tetrahedron inertia tensor in terms of its vertex coordinates"

    // density * det(jacobian) * (a₁² + a₁a₂ + a₂² + a₁a₃ + a₂a₃ + a₃² + a₁a₄ + a₂a₄ + a₃a₄ + a₄² + b₁² + b₁b₂ + b₂² + b₁b₃ + b₂b₃ + b₃² + b₁b₄ + b₂b₄ + b₃b₄ + b₄²) / 60
    // det(jacobian) = 6 * volume
    // density * 6 * volume * (...) / 60
    // mass * (...) / 10
    const float a1 = a[0], a2 = a[1], a3 = a[2], a4 = a[3];
    const float b1 = b[0], b2 = b[1], b3 = b[2], b4 = b[3];
    return 0.1f * mass * (a1 * a1 + a1 * a2 + a2 * a2 + a1 * a3 +
                          a2 * a3 + a3 * a3 + a1 * a4 + a2 * a4 +
                          a3 * a4 + a4 * a4 + b1 * b1 + b1 * b2 +
                          b2 * b2 + b1 * b3 + b2 * b3 + b3 * b3 +
                          b1 * b4 + b2 * b4 + b3 * b4 + b4 * b4);
}

static float productOfInertia(const vec4 a, const vec4 b, const float mass) {
    // calculating products of inertia e.g. values of inertia not in the diagonal

    // formula found here: https://docsdrive.com/pdfs/sciencepublications/jmssp/2005/8-11.pdf
    // "Explicit and exact formulas for the 3-D tetrahedron inertia tensor in terms of its vertex coordinates"

    // density * det(jacobian) * (2a₁b₁ + a₂b₁ + a₃b₁ + a₄b₁ + a₁b₂ + 2a₂b₂ + a₃b₂ + a₄b₂ + a₁b₃ + a₂b₃ + 2a₃b₃ + a₄b₃ + a₁b₄ + a₂b₄ + a₃b₄ + 2a₄b₄) / 120
    // density * 6 * volume * (...) / 120
    // mass * (...) / 20
    const float a1 = a[0], a2 = a[1], a3 = a[2], a4 = a[3];
    const float b1 = b[0], b2 = b[1], b3 = b[2], b4 = b[3];
    return 0.05f * mass * (2.0f * a1 * b1 +        a2 * b1 +        a3 * b1 +        a4 * b1 +
                                  a1 * b2 + 2.0f * a2 * b2 +        a3 * b2 +        a4 * b2 +
                                  a1 * b3 +        a2 * b3 + 2.0f * a3 * b3 +        a4 * b3 +
                                  a1 * b4 +        a2 * b4 +        a3 * b4 + 2.0f * a4 * b4);
}

void tetrahedronInertiaTensor(const vec3 point_a, const vec3 point_b, const vec3 point_c, const vec3 point_d, const float mass, mat3 tensor) {
    // calculating the inertia tensor

    // formula found here: https://docsdrive.com/pdfs/sciencepublications/jmssp/2005/8-11.pdf
    // "Explicit and exact formulas for the 3-D tetrahedron inertia tensor in terms of its vertex coordinates"

    // splitting xs, ys and zs from each vertex to use more easily in calculations.
    const vec4 x_values = { point_a[0], point_b[0], point_c[0], point_d[0] };
    const vec4 y_values = { point_a[1], point_b[1], point_c[1], point_d[1] };
    const vec4 z_values = { point_a[2], point_b[2], point_c[2], point_d[2] };

    // calculating moments of inertia in all 3 axes
    const float am = momentOfInertia(y_values, z_values, mass);
    const float bm = momentOfInertia(x_values, z_values, mass);
    const float cm = momentOfInertia(x_values, y_values, mass);

    // calculating products of inertia
    const float ap = -productOfInertia(y_values, z_values, mass);
    const float bp = -productOfInertia(x_values, z_values, mass);
    const float cp = -productOfInertia(x_values, y_values, mass);

    // as stated in resource linked above:
    //
    //          |  a  -b' -c' |
    // Tensor = | -b'  b  -a' |
    //          | -c' -a'  c  |
    //
    // where a, b, c are moments of inertia
    //       a', b', c' are products of inertia

    mat3 tensor_temp = {
        { am, bp, cp },
        { bp, bm, ap },
        { cp, ap, cm }
    };

    glm_mat3_copy(tensor_temp, tensor);
}

void translateInertiaTensor(mat3 tensor, const float mass, vec3 translate) {
    // translating inertia tensor
    // using formula from: https://en.wikipedia.org/wiki/Parallel_axis_theorem#Tensor_generalization
    //
    // Translated Inertia Tensor = Iₜ + m[(R⋅R)I - R⊗R]
    //
    // where Iₜ is the original tensor, R is the vector from the new centre to the old centre, m is the mass, I is the identity matrix

    // calculating R⋅R
    const float correction_scalar = glm_vec3_dot(translate, translate);
    mat3 tensor_mod;

    // calculating (R⋅R)I
    glm_mat3_identity(tensor_mod);
    glm_mat3_scale(tensor_mod, correction_scalar);

    // calculating R⊗R
    mat3 outer_product;
    mat3OuterProduct(translate, translate, outer_product);

    // calculating (R⋅R)I - R⊗R
    mat3Subtract(tensor_mod, outer_product, tensor_mod);

    // calculating m[(R⋅R)I - R⊗R]
    glm_mat3_scale(tensor_mod, mass);

    // calculating Iₜ + m[(R⋅R)I - R⊗R]
    mat3Add(tensor, tensor_mod, tensor);
}
