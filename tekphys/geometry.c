#include "geometry.h"

#include "../tekgl.h"
#include <cglm/vec4.h>
#include <stdarg.h>
#include <time.h>
#include <cglm/mat3.h>
#include <cglm/mat4.h>
#include "../tekgl/manager.h"

/**
 * Called when program begins, initialise some things for geometry.
 */
tek_init tekInitGeometry() {
    // seed random number generator. (not cryptographically secure but that doesn't matter here)
    srand(time(0));
}

/**
 * @brief Find the sum of a number of vec3s.
 * @note Stops counting when a null pointer is reached. Doesn't check if values are actually vec3s.
 * @param dest Where to store the sum.
 * @param ... Vec3s to sum.
 */
void sumVec3VA(vec3 dest, ...) {
    // using variadic arguments to allow for any number of vectors
    // macro makes the final argument be nullptr
    va_list vec3_list;
    va_start(vec3_list, dest);
    float* vector_ptr;
    glm_vec3_zero(dest);
    while ((vector_ptr = va_arg(vec3_list, float*))) {
        glm_vec3_add(vector_ptr, dest, dest);
    }
}

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
    // outer product is just a matrix that has all pairs of multiplications between the vectors.
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
    // 3 tall, 3 wide, so need a nested loop
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
    // 3 tall, 3 wide, so need nested loop
    for (uint i = 0; i < 3; i++) {
        for (uint j = 0; j < 3; j++) {
            m[i][j] = a[i][j] - b[i][j];
        }
    }
}

/**
 * Calculate the moment of inertia of a tetrahedron, these make up the leading diagonal of a inertia tensor matrix.
 * @param a The set of coordinates in a single axis of each point (e.g. all x-coordinates)
 * @param b The matching coordinates in another axis (e.g. all y-coordinates)
 * @param mass The mass of the tetrahedron.
 * @return The moment of inertia.
 */
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

/**
 * Helper function to calculate "products of inertia" - used as elements in the inertia tensor.
 * @param a A component of each point on a tetrahedron in one axis. (e.g. all x-coordinates).
 * @param b The matching components of a different axis. (e.g. all y-coordinates)
 * @param mass The mass of the tetrahedron.
 * @return The "product of inertia"
 */
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

/**
 * Calculate the inertia tensor of a tetrahedron based on the four points that make it up.
 * @param point_a The first point of the tetrahedron.
 * @param point_b The second point of the tetrahedron.
 * @param point_c The third point of the tetrahedron.
 * @param point_d The fourth point of the tetrahedron.
 * @param mass The mass of the tetrahedron.
 * @param tensor The outputted inertia tensor.
 */
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

/**
 * Translate an inertia tensor.
 * @param tensor The inertia tensor to translate.
 * @param mass The mass being added to the object.
 * @param translate The displacement of the new mass / how far to translate the tensor.
 */
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

/**
 * Calculate the scalar triple product of three vectors
 * = cross product of b and c, dotted against a.
 * @param vector_a The first vector involved in the product
 * @param vector_b The second vector involved in the product
 * @param vector_c The third vector involved in the product
 * @return A floating point number, which is the scalar triple product.
 */
float scalarTripleProduct(vec3 vector_a, vec3 vector_b, vec3 vector_c) {
    vec3 cross_product;
    // first: b x c
    glm_vec3_cross(vector_b, vector_c, cross_product);
    // finally: (b x c) . a
    return glm_vec3_dot(vector_a, cross_product);
}

/**
 * Get the normal vector given the three points of a triangle.
 * @param[in] triangle[3] An array of 3 vec3s representing the triangle.
 * @param[out] normal The normalised normal vector of this triangle.
 */
void triangleNormal(vec3 triangle[3], vec3 normal) {
    // triangle normal = cross product of two of its edges.
    // cross product finds perpendicular vector, so if both edges are on the same plane, must get the normal
    vec3 vec_ab, vec_ac;
    glm_vec3_sub(triangle[1], triangle[0], vec_ab);
    glm_vec3_sub(triangle[2], triangle[0], vec_ac);
    glm_vec3_cross(vec_ab, vec_ac, normal);

    // normalize for consistent results.
    glm_vec3_normalize(normal);
}

/**
 * Generate a random floating point number within two bounds, min and max.
 * @param min The minimum possible number to be generated.
 * @param max The maximum possible number to be generated.
 * @return A random number in between min and max.
 */
float randomFloat(const float min, const float max) {
    // rannd() = random *integer* between 0 and RAND_MAX.
    // divide by RAND_MAX to get between 0.0 and 1.0
    // multiply by range and add min, to scale to min, max
    return ((max - min) * ((float)rand() / (float)RAND_MAX)) + min;
}
