from numpy import ndarray
from ursina import *
from dataclasses import dataclass
import numpy as np
from ursina.color import rgba
from ursina.shaders import lit_with_shadows_shader, basic_lighting_shader

EPSILON = 1e-6

@dataclass
class OBB:
    centre: np.ndarray
    half_extents: list[float]
    axes: list[np.ndarray]

@dataclass
class Triangle:
    vertices: list[np.ndarray]

def get_vector_from_string(vector_str: str) -> np.ndarray:
    split_str = vector_str.split(",")
    number_list: list[float] = list()
    for string in split_str:
        number_list.append(float(string))
    return np.array(number_list)

def read_log_file_vectors(log_file: str) -> list[tuple[np.ndarray, int]]:
    with open(log_file) as f:
        log_data = f.read()
    vectors: list[tuple[np.ndarray, int]] = list()
    for line_num, line in enumerate(log_data.split("\n")):
        if len(line) == 0:
            continue
        if line[0] in ("P", "F", "\n", "="):
            continue
        vectors.append((get_vector_from_string(line), line_num))
    return vectors

def read_log_file(log_file: str) -> list[tuple[Triangle, Triangle, int]]:
    with open(log_file) as f:
        log_data = f.read()
    line_number_list: list[int] = list()
    triangle_a_list: list[Triangle] = list()
    triangle_b_list: list[Triangle] = list()
    build_list: list[np.ndarray] = list()
    i = 0
    for line_num, line in enumerate(log_data.split("\n")):
        if len(line) == 0:
            continue
        if line[0] in ("P", "F", "\n", "="):
            continue
        if i == 0:
            line_number_list.append(line_num + 1)
            pass
        elif i % 6 == 0:
            triangle_b_list.append(Triangle(
                vertices=deepcopy(build_list)
            ))
            line_number_list.append(line_num + 1)
            build_list.clear()
        elif i % 3 == 0:
            triangle_a_list.append(Triangle(
                vertices=deepcopy(build_list)
            ))
            build_list.clear()
        build_list.append(get_vector_from_string(line))
        i += 1

    return list(zip(triangle_a_list, triangle_b_list, line_number_list))

def read_log_file_obb(log_file) -> list[tuple[OBB, OBB, int]]:
    vectors = read_log_file_vectors(log_file)
    i = 0
    build_list = list()
    obb_as = list()
    obb_bs = list()
    line_nums = list()
    for vector, line_num in vectors:
        if i == 0:
            pass
        elif i % 10 == 0:
            obb_bs.append(deepcopy(build_list))
            line_nums.append(line_num)
            build_list.clear()
        elif i % 5 == 0:
            obb_as.append(deepcopy(build_list))
            build_list.clear()
        build_list.append(vector)
        i += 1

    output_list = list()
    for obb_a_list, obb_b_list, line_num in zip(obb_as, obb_bs, line_nums):
        output_list.append((
        OBB(
            centre=obb_a_list[0],
            half_extents=[float(obb_a_list[1][0]), float(obb_a_list[1][1]), float(obb_a_list[1][2])],
            axes=[obb_a_list[2], obb_a_list[3], obb_a_list[4]]
        ),
        OBB(
            centre=obb_b_list[0],
            half_extents=[float(obb_b_list[1][0]), float(obb_b_list[1][1]), float(obb_b_list[1][2])],
            axes=[obb_b_list[2], obb_b_list[3], obb_b_list[4]]
        ),
        line_num
        ))
    return output_list

def read_log_file_obb_triangle(log_file):
    vectors = read_log_file_vectors(log_file)
    output_list = list()
    for i in range(len(vectors) // 8):
        cur_vectors = [v[0] for v in vectors[i * 8:i * 8 + 8]]
        output_list.append((
        OBB(
            centre=cur_vectors[0],
            half_extents=[float(cur_vectors[1][0]), float(cur_vectors[1][1]), float(cur_vectors[1][2])],
            axes=[cur_vectors[2], cur_vectors[3], cur_vectors[4]]
        ),
        Triangle(
            vertices=[cur_vectors[5], cur_vectors[6], cur_vectors[7]]
        ),
        vectors[i][1]
        ))
    return output_list

def create_obb_mesh(obb: OBB) -> Mesh:
    # cheeky function to map 0 => 1, 1 => -1
    sign = lambda x: (-1) ** x

    # basically create all the vertices of the OBB based on the dimensions of OBB
    vertices: list[np.ndarray] = [None for _ in range(8)]
    for i in range(2):
        for j in range(2):
            for k in range(2):
                vertices[i + 2 * j + 4 * k] = np.multiply(obb.axes[0], sign(i) * obb.half_extents[0])
                vertices[i + 2 * j + 4 * k] = np.add(vertices[i + 2 * j + 4 * k], np.multiply(obb.axes[1], sign(j) * obb.half_extents[1]))
                vertices[i + 2 * j + 4 * k] = np.add(vertices[i + 2 * j + 4 * k], np.multiply(obb.axes[2], sign(k) * obb.half_extents[2]))
                vertices[i + 2 * j + 4 * k] = np.add(vertices[i + 2 * j + 4 * k], obb.centre)

    # now turn np arrays into normal python arrays so ursina can use it
    for i in range(len(vertices)):
        vertices[i] = vertices[i].tolist()

    # build el mesh using triangle indices
    obb_mesh = Mesh(
        vertices=vertices,
        triangles=[
            1, 3, 7,
            1, 7, 5,
            0, 4, 6,
            0, 6, 2,
            2, 6, 7,
            2, 7, 3,
            0, 1, 5,
            0, 5, 4,
            4, 5, 7,
            4, 7, 6,
            0, 2, 3,
            0, 3, 1
        ]
    )

    # i assume this creates some kinda gpu buffer as it doesnt render without?
    obb_mesh.generate_normals()
    obb_mesh.generate()
    return obb_mesh

def create_triangle_mesh(triangle: Triangle) -> Mesh:
    vertices: list[np.ndarray] = list()
    for i in range(3):
        vertices.append(triangle.vertices[i].tolist())

    for i in range(3):
        vertices.append(triangle.vertices[2 - i].tolist())
        vertices[i] = np.add(vertices[i], np.array([0.0, EPSILON, 0.0]))

    triangle_mesh = Mesh(
        vertices=vertices
    )
    triangle_mesh.generate_normals()
    triangle_mesh.generate()
    return triangle_mesh

def create_cylinder_mesh(height: float, radius: float, num_divisions: int) -> Mesh:
    if num_divisions < 2:
        raise ValueError(f"Cannot divide less than 2 times! (requested {num_divisions} divisions).")

    num_faces = num_divisions + 1
    vertices: list[np.ndarray] = list()
    angular_width = (2 * math.pi) / num_faces
    curr_angle = 0

    for i in range(num_faces):
        face_vertices: list[np.ndarray] = list()
        for j in range(2):
            angle = curr_angle if j == 0 else curr_angle + angular_width
            x = radius * math.cos(angle)
            z = radius * math.sin(angle)

            for k in range(2):
                y = height / 2 if k == 0 else -height / 2
                face_vertices.append(np.array([x, y, z]))

        for index in (0, 1, 2):
            vertices.append(face_vertices[index])
        for index in (1, 3, 2):
            vertices.append(face_vertices[index])

        vertices.append(face_vertices[2])
        vertices.append(np.array([0.0, height / 2, 0.0]))
        vertices.append(face_vertices[0])

        vertices.append(face_vertices[1])
        vertices.append(np.array([0.0, -height / 2, 0.0]))
        vertices.append(face_vertices[3])

        curr_angle += angular_width

    print("Num Vertices:", len(vertices))

    cylinder_mesh = Mesh(
        vertices=vertices
    )
    cylinder_mesh.generate_normals()
    cylinder_mesh.generate()
    return cylinder_mesh

def check_obb_obb_collision(obb_a: OBB, obb_b: OBB):
    translate = np.subtract(obb_b.centre, obb_a.centre)

    dot_matrix = list()
    for i in range(3):
        dot_matrix.append(list())
        for j in range(3):
            dot_matrix[i].append(0)

    for i in range(3):
        for j in range(3):
            dot_matrix[i][j] = np.dot(obb_a.axes[i], obb_b.axes[j]) + EPSILON

    t_array = [0] * 3
    for i in range(3):
        t_array[i] = np.dot(translate, obb_a.axes[i])

    # Basic cases

    for i in range(3):
        mag_x = np.fabs(obb_b.half_extents[0] * dot_matrix[i][0])
        mag_y = np.fabs(obb_b.half_extents[1] * dot_matrix[i][1])
        mag_z = np.fabs(obb_b.half_extents[2] * dot_matrix[i][2])
        if np.fabs(t_array[i]) > obb_a.half_extents[i] + mag_x + mag_y + mag_z:
            print("Early return @ 1")
            return False

    # Intermediate cases

    for i in range(3):
        mag_x = np.fabs(obb_a.half_extents[0] * dot_matrix[0][i])
        mag_y = np.fabs(obb_a.half_extents[1] * dot_matrix[1][i])
        mag_z = np.fabs(obb_a.half_extents[2] * dot_matrix[2][i])
        dot_product = np.fabs(np.dot(translate, obb_b.axes[i]))
        if dot_product > obb_b.half_extents[i] + mag_x + mag_y + mag_z:
            print("Early return @ 2")
            return False

    # Difficult cases
    # Store the indices of t_array to access for i values.
    multis_indices = [
        (2, 1),
        (0, 2),
        (1, 0)
    ]

    # Store indices for direction (W H or D) for inner and outer loop cycles
    cyc_indices = [
        (1, 2),
        (0, 2),
        (0, 1)
    ]

    # axn = ["X", "Y", "Z"]
    # dep = ["W", "H", "D"]

    for i in range(3):
        for j in range(3):
            ti_a, ti_b = multis_indices[i]
            cmp_base = t_array[ti_a] * np.dot(obb_a.axes[ti_b], obb_b.axes[j])
            cmp_subt = t_array[ti_b] * np.dot(obb_a.axes[ti_a], obb_b.axes[j])
            cmp = np.fabs(cmp_base - cmp_subt)

            cyc_ll, cyc_lh = cyc_indices[i]
            cyc_sl, cyc_sh = cyc_indices[j]

            tst_a = np.fabs(obb_a.half_extents[cyc_ll] * dot_matrix[cyc_lh][j])
            tst_b = np.fabs(obb_a.half_extents[cyc_lh] * dot_matrix[cyc_ll][j])
            tst_c = np.fabs(obb_b.half_extents[cyc_sl] * dot_matrix[i][cyc_sh])
            tst_d = np.fabs(obb_b.half_extents[cyc_sh] * dot_matrix[i][cyc_sl])

            tst = tst_a + tst_b + tst_c + tst_d

            # print(f"|(T.A{axn[ti_a]})R{axn[ri_a]}{axn[j]}-(T.A{axn[ti_b]})R{axn[ri_b]}{axn[j]}| > |{dep[cyc_ll]}AR{axn[cyc_lh]}{axn[j]}| + |{dep[cyc_lh]}AR{axn[cyc_ll]}{axn[j]}| + |{dep[cyc_sl]}BR{axn[i]}{axn[cyc_sh]}| + |{dep[cyc_sh]}BR{axn[i]}{axn[cyc_sl]}|")

            if cmp > tst:
                print("Early return @ 3")
                return False

    return True

def create_obb_transform(obb: OBB) -> np.ndarray:
    transform = np.array([
        obb.axes[0][0], obb.axes[1][0], obb.axes[2][0], obb.centre[0],
        obb.axes[0][1], obb.axes[1][1], obb.axes[2][1], obb.centre[1],
        obb.axes[0][2], obb.axes[1][2], obb.axes[2][2], obb.centre[2],
        0.0, 0.0, 0.0, 1.0
    ])
    transform.shape = 4,4
    return np.linalg.inv(transform)

def check_aabb_triangle_collision(half_extents: list[float], triangle: Triangle) -> bool:
    face_vectors: list[np.ndarray] = list()
    for i in range(3):
        face_vectors.append(np.subtract(triangle.vertices[(i + 1) % 3], triangle.vertices[i]))

    xyz_axes: list[np.ndarray] = [
        np.array([1.0, 0.0, 0.0]),
        np.array([0.0, 1.0, 0.0]),
        np.array([0.0, 0.0, 1.0])
    ]

    face_normal = np.cross(face_vectors[0], face_vectors[1])
    face_normal = np.multiply(face_normal, 1.0 / float(np.linalg.norm(face_normal)))

    tst = np.fabs(np.dot(triangle.vertices[0], face_normal))
    cmp = 0.0
    for i in range(3):
        cmp += half_extents[i] * np.fabs(face_normal[i])
    if tst > cmp:
        print("Failed 1")
        return False

    for i in range(3):
        dots: list[float] = list()
        for j in range(3):
            dots.append(np.dot(triangle.vertices[j], xyz_axes[i]))

        tri_min = min(dots[0], dots[1], dots[2])
        tri_max = max(dots[0], dots[1], dots[2])

        if (tri_min > half_extents[i]) or (tri_max < -half_extents[i]):
            print("Failed 2")
            return False

    for i in range(3):
        for j in range(3):
            dots: list[float] = list()
            curr_axis: np.ndarray = np.cross(xyz_axes[i], face_vectors[j])
            projection: float = 0.0
            for k in range(3):
                projection += half_extents[k] * np.fabs(np.dot(xyz_axes[k], curr_axis))
                dots.append(np.dot(triangle.vertices[k], curr_axis))

            tri_min = min(dots[0], dots[1], dots[2])
            tri_max = max(dots[0], dots[1], dots[2])
            if (tri_min > projection) or (tri_max < -projection):
                print("Failed 3")
                return False

    print("Passed")
    return True

def check_obb_triangle_collision(obb: OBB, triangle: Triangle) -> bool:
    transform = create_obb_transform(obb)
    transformed_vertices = list()
    for i in range(3):
        vertex = np.array([triangle.vertices[i][0], triangle.vertices[i][1], triangle.vertices[i][2], 1.0])
        product = np.matmul(transform, vertex)
        transformed_vertices.append(np.array([product[0], product[1], product[2]]))
    return check_aabb_triangle_collision(obb.half_extents, Triangle(vertices=transformed_vertices))

def scalar_triple_product(vec_a: np.ndarray, vec_b: np.ndarray, vec_c: np.ndarray) -> float:
    cross_product: np.ndarray = np.cross(vec_b, vec_c)
    return float(np.dot(vec_a, cross_product))

def calculate_orientation(vec_a: np.ndarray, vec_b: np.ndarray, vec_c: np.ndarray, vec_d: np.ndarray) -> float:
    sub_vec_a: np.ndarray = np.subtract(vec_a, vec_d)
    sub_vec_b: np.ndarray = np.subtract(vec_b, vec_d)
    sub_vec_c: np.ndarray = np.subtract(vec_c, vec_d)
    return scalar_triple_product(sub_vec_a, sub_vec_b, sub_vec_c)

def get_sign(number: float) -> int:
    if number > 0:
        return 1
    elif number < 0:
        return - 1
    else:
        return 0

def swap_vertices_left(triangle: Triangle) -> Triangle:
    return Triangle(vertices=[
        triangle.vertices[1],
        triangle.vertices[2],
        triangle.vertices[0]
    ])

def swap_vertices_right(triangle: Triangle) -> Triangle:
    return Triangle(vertices=[
        triangle.vertices[2],
        triangle.vertices[0],
        triangle.vertices[1]
    ])

"""Swap the vertices in a triangle so that vertex 0 is the only vertex on it's side of the halfspace"""
def find_and_swap_vertices(triangle: Triangle, sign_array: list[int]) -> Triangle:
    if sign_array[0] == sign_array[1]:
        # if 0 and 1 are the same sign, then 2 needs to be on its own, so permute right
        triangle = swap_vertices_right(triangle)
    elif sign_array[0] == sign_array[2]:
        # if 0 and 2 are on the same side, then 1 needs to be on its own
        triangle = swap_vertices_left(triangle)
    elif sign_array[1] != sign_array[2]:
        # if all three signs are different, need to place vertex 0 on positive side.
        if sign_array[1] > 0:
            triangle = swap_vertices_left(triangle)
        elif sign_array[2] > 0:
            triangle = swap_vertices_right(triangle)

    return triangle

def make_vertices_positive(triangle_a: Triangle, triangle_b: Triangle) -> Triangle:
    sign = get_sign(calculate_orientation(triangle_b.vertices[0], triangle_b.vertices[1], triangle_b.vertices[2], triangle_a.vertices[0]))
    if sign < 0:
        return Triangle(vertices=[
            triangle_b.vertices[0],
            triangle_b.vertices[2],
            triangle_b.vertices[1]
        ])
    else:
        return Triangle(vertices=[
            triangle_b.vertices[0],
            triangle_b.vertices[1],
            triangle_b.vertices[2]
        ])

def organise_triangles_for_collision(triangle_a: Triangle, triangle_b: Triangle) -> tuple[Triangle, Triangle] | None:
    sign_array_a = [
        get_sign(calculate_orientation(triangle_b.vertices[0], triangle_b.vertices[1], triangle_b.vertices[2], triangle_a.vertices[0])),
        get_sign(calculate_orientation(triangle_b.vertices[0], triangle_b.vertices[1], triangle_b.vertices[2], triangle_a.vertices[1])),
        get_sign(calculate_orientation(triangle_b.vertices[0], triangle_b.vertices[1], triangle_b.vertices[2], triangle_a.vertices[2]))
    ]

    if (sign_array_a[0] == sign_array_a[1]) and (sign_array_a[1] == sign_array_a[2]):
        # TODO: Check for coplanar intersection
        # print("Coplanar exit 1")
        return None

    sign_array_b = [
        get_sign(calculate_orientation(triangle_a.vertices[0], triangle_a.vertices[1], triangle_a.vertices[2], triangle_b.vertices[0])),
        get_sign(calculate_orientation(triangle_a.vertices[0], triangle_a.vertices[1], triangle_a.vertices[2], triangle_b.vertices[1])),
        get_sign(calculate_orientation(triangle_a.vertices[0], triangle_a.vertices[1], triangle_a.vertices[2], triangle_b.vertices[2]))
    ]

    if (sign_array_b[0] == sign_array_b[1]) and (sign_array_b[1] == sign_array_b[2]):
        # print("Coplanar exit 2")
        return None

    triangle_a = find_and_swap_vertices(triangle_a, sign_array_a)
    triangle_b = find_and_swap_vertices(triangle_b, sign_array_b)

    triangle_a = make_vertices_positive(triangle_b, triangle_a)
    triangle_b = make_vertices_positive(triangle_a, triangle_b)

    return triangle_a, triangle_b

def check_organised_triangle_triangle_collision(triangle_a: Triangle, triangle_b: Triangle):
    sign_a = get_sign(calculate_orientation(triangle_a.vertices[0], triangle_a.vertices[1], triangle_b.vertices[0], triangle_b.vertices[1]))
    if sign_a > 0:
        # print("Sign exit 1")
        return False

    sign_b = get_sign(calculate_orientation(triangle_a.vertices[0], triangle_a.vertices[2], triangle_b.vertices[2], triangle_b.vertices[0]))
    if sign_b > 0:
        # print("Sign exit 2")
        return False

    return True

def check_triangle_triangle_collision(triangle_a: Triangle, triangle_b: Triangle) -> bool:
    triangles = organise_triangles_for_collision(triangle_a, triangle_b)
    if triangles is None:
        return False

    triangle_a, triangle_b = triangles

    return check_organised_triangle_triangle_collision(triangle_a, triangle_b)

def get_triangle_normal(triangle: Triangle):
    edge_a = np.subtract(triangle.vertices[1], triangle.vertices[0])
    edge_b = np.subtract(triangle.vertices[2], triangle.vertices[0])
    normal = np.cross(edge_a, edge_b)
    magnitude = np.linalg.norm(normal)
    return np.multiply(normal, 1.0 / float(magnitude))

def get_triangle_centroid(triangle: Triangle) -> np.ndarray:
    sum = np.add(triangle.vertices[0], triangle.vertices[1])
    sum = np.add(sum, triangle.vertices[2])
    return np.multiply(sum, 1.0 / 3.0)

def get_triangle_edge_contact_normal(edge_a: np.ndarray, edge_b: np.ndarray) -> np.ndarray:

    contact_normal = np.cross(edge_a, edge_b)

    print(contact_normal)

    contact_magnitude = np.linalg.norm(contact_normal)
    return np.multiply(contact_normal, 1.0 / contact_magnitude)

def get_triangle_triangle_collision_normal(triangle_a: Triangle, triangle_b: Triangle) -> np.ndarray | None:
    triangles = organise_triangles_for_collision(triangle_a, triangle_b)
    if triangles is None:
        return None

    triangle_a, triangle_b = triangles

    if not check_organised_triangle_triangle_collision(triangle_a, triangle_b):
        return None

    sign_c = get_sign(calculate_orientation(triangle_a.vertices[0], triangle_a.vertices[2], triangle_b.vertices[1], triangle_b.vertices[0]))
    sign_d = get_sign(calculate_orientation(triangle_a.vertices[0], triangle_a.vertices[1], triangle_b.vertices[2], triangle_b.vertices[0]))

    normal_a = get_triangle_normal(triangle_a)
    normal_b = get_triangle_normal(triangle_b)

    if sign_c > 0:
        if sign_d > 0:
            print("case 1")
            edge_a = np.subtract(triangle_a.vertices[0], triangle_a.vertices[2])
            edge_b = np.subtract(triangle_b.vertices[0], triangle_b.vertices[2])
            return get_triangle_edge_contact_normal(edge_b, edge_a)
        else:
            print("case 2")
            print(triangle_a, triangle_b)
            return np.multiply(normal_b, -1.0)
    else:
        if sign_d > 0:
            print("case 3")
            return normal_a
        else:
            print("case 4")
            edge_a = np.subtract(triangle_a.vertices[0], triangle_a.vertices[1])
            edge_b = np.subtract(triangle_b.vertices[0], triangle_b.vertices[1])
            return get_triangle_edge_contact_normal(edge_a, edge_b)

def get_triangle_triangle_collision_sat(triangle_a: Triangle, triangle_b: Triangle) -> np.ndarray | None:
    axes = list()

    normal_a = get_triangle_normal(triangle_a)
    normal_b = get_triangle_normal(triangle_b)

    axes.append(normal_a)
    axes.append(normal_b)

    edges_a = list()
    edges_b = list()

    for i in range(3):
        edges_a.append(np.subtract(triangle_a.vertices[(i + 1) % 3], triangle_a.vertices[i]))
        edges_b.append(np.subtract(triangle_b.vertices[(i + 1) % 3], triangle_b.vertices[i]))

    for i in range(3):
        for j in range(3):
            axes.append(np.cross(edges_a[i], edges_b[j]))

    print(axes)

    min_overlap = float("inf")
    contact_axis = None

    for axis in axes:
        min_a, max_a = float("inf"), -float("inf")
        min_b, max_b = float("inf"), -float("inf")

        for vertex in triangle_a.vertices:
            projection = float(np.dot(vertex, axis))
            min_a = min(min_a, projection)
            max_a = max(max_a, projection)

        for vertex in triangle_b.vertices:
            projection = float(np.dot(vertex, axis))
            min_b = min(min_b, projection)
            max_b = max(max_b, projection)

        overlap = min(max_a, max_b) - max(min_a, min_b)
        if overlap < 0:
            print("Separated @", axis)
            return None

        if overlap < min_overlap:
            min_overlap = overlap
            contact_axis = axis

    # let the normal be the face normal if its kinda close (stops flickering)
    # if abs(np.dot(contact_axis, normal_a)) > 0.95:
    #     contact_axis = normal_a
    # elif abs(np.dot(contact_axis, normal_b)) > 0.95:
    #     contact_axis = normal_b

    return contact_axis

class VectorDisplayEntity(Entity):
    def __update_world_position(self):
        self.world_position = np.add(self.__position, np.multiply(self.__vector, 0.5))
        self.look_in_direction(self.__vector, Vec3(0.0, 1.0, 0.0))

    def __init__(self, position: np.ndarray, vector: np.ndarray, tube_radius: float=0.15, colour: Color=rgb(1.0, 1.0, 1.0)):
        self.__mesh = create_cylinder_mesh(radius=tube_radius, height=1.0, num_divisions=10)
        super().__init__(model=self.__mesh, scale=1.0, color=colour)
        self.__vector = vector
        self.__position = position
        self.__update_world_position()

    def get_vector(self) -> np.ndarray:
        return self.__vector

    def set_vector(self, vector: np.ndarray):
        self.__vector = vector
        magnitude = np.linalg.norm(self.__vector)
        self.scale = (1.0, magnitude, 1.0)
        self.__update_world_position()

    def get_position(self) -> np.ndarray:
        return self.__position

    def set_position(self, position: np.ndarray):
        self.__position = position
        self.__update_world_position()

class SphereControllerEntity(Entity):
    NORMAL_COLOUR = rgb(0.9, 0.9, 0.9)
    HOVERED_COLOUR = rgb(1.0, 1.0, 1.0)
    HOVERED_SCALE_MULTIPLIER = 1.2
    ANIMATION_DURATION = 0.2

    def __init__(self, position: np.ndarray, radius: float=0.25, update_callback=None):
        super().__init__(model="sphere", scale=radius, color=SphereControllerEntity.NORMAL_COLOUR, collider="sphere")
        self.always_on_top_setter(True)
        self.world_position = position
        self.__radius = radius
        self.__hovering = False
        self.selected = False
        self.__cam_vector = Vec3(0, 0, 0)
        self.__flicker = 0
        self.__update_callback = update_callback

    def update(self):
        if self.__hovering and mouse.left:
            self.selected = True

        if not mouse.left:
            self.selected = False

        if self.selected:
            mouse_vector = self.world_position - camera.world_position
            plane_distance = camera.forward.dot(mouse_vector)

            scale_factor = math.tan(math.radians(camera.fov / 2))
            side_vector = camera.forward.cross(camera.up) * scale_factor
            up_vector = camera.up * (scale_factor / camera.aspect_ratio_getter())

            mouse_x = ((mouse.x / camera.aspect_ratio_getter()) + 0.5)
            mouse_y = (0.5 - mouse.y)

            base_position = (side_vector + up_vector + camera.forward)
            x_vector = side_vector * (-2 * mouse_x)
            y_vector = up_vector * (-2 * mouse_y)

            self.world_position = camera.world_position + (base_position + x_vector + y_vector) * plane_distance

        if self.hovered and not self.__hovering:
            self.animate_color(SphereControllerEntity.HOVERED_COLOUR, duration=SphereControllerEntity.ANIMATION_DURATION, curve=curve.in_out_expo)
            self.animate_scale(SphereControllerEntity.HOVERED_SCALE_MULTIPLIER * self.__radius, duration=SphereControllerEntity.ANIMATION_DURATION, curve=curve.in_out_expo)
            self.__hovering = True
        elif not self.hovered and self.__hovering:
            self.animate_color(SphereControllerEntity.NORMAL_COLOUR, duration=SphereControllerEntity.ANIMATION_DURATION, curve=curve.in_out_expo)
            self.animate_scale(self.__radius, duration=SphereControllerEntity.ANIMATION_DURATION, curve=curve.in_out_expo)
            self.__hovering = False

        if self.__update_callback is not None:
            self.__update_callback()

class SphereController:
    def __init__(self, position: np.ndarray, radius: float=0.25, update_callback=None):
        self.__entity = SphereControllerEntity(position, radius, update_callback)

    def get_position(self) -> np.ndarray:
        sphere_position = self.__entity.world_position
        return np.array([sphere_position.x, sphere_position.y, sphere_position.z])

    def set_position(self, position: np.ndarray):
        self.__entity.world_position = position

    def is_selected(self) -> bool:
        return self.__entity.selected

    def visible_setter(self, value):
        self.__entity.visible_setter(value)

class OBBController:
    def __init__(self, centre: np.ndarray, radius: float=0.25):
        self.__centre_sphere = SphereController(centre, radius, update_callback=self.__update)
        self.__axis_spheres: list[SphereController] = list()
        self.__axes: list[np.ndarray] = list()
        self.__prev_axes: list[np.ndarray] = list()
        for axis in (np.array([1, 0, 0]), np.array([0, 1, 0]), np.array([0, 0, 1])):
            final_axis = np.multiply(axis, 2 * radius)
            final_position = np.add(final_axis, centre)
            self.__axis_spheres.append(SphereController(final_position, radius))
            self.__prev_axes.append(final_axis)
            self.__axes.append(final_axis)

    def __update(self):
        if self.__centre_sphere.is_selected():
            for i in range(3):
                self.__axis_spheres[i].set_position(np.add(self.__centre_sphere.get_position(), self.__axes[i]))

        for i in range(3):
            if self.__axis_spheres[i].is_selected():
                cur_axis = self.__axes[i]
                prev_axis = self.__prev_axes[i]

                rotation_axis = np.cross(prev_axis, cur_axis)
                len_r = float(np.linalg.norm(rotation_axis))
                if len_r < 0.00001:
                    break
                rotation_axis = np.multiply(rotation_axis, 1 / len_r)

                position_delta = np.subtract(cur_axis, prev_axis)
                len_a = float(np.linalg.norm(position_delta))

                len_b = float(np.linalg.norm(cur_axis))
                len_c = float(np.linalg.norm(prev_axis))

                rotation_angle = math.acos((len_a ** 2 - len_b ** 2 - len_c ** 2) / (-2.0 * len_b * len_c))

                for j in range(3):
                    if i == j:
                        continue

                    # rodriguez formula
                    exv = np.cross(rotation_axis, self.__axes[j])
                    exexv = np.cross(rotation_axis, exv)

                    vec_a = np.multiply(exv, math.sin(rotation_angle))
                    vec_b = np.multiply(exexv, 1 - math.cos(rotation_angle))

                    vec_final = np.add(self.__axes[j], vec_a)
                    vec_final = np.add(vec_final, vec_b)

                    self.__axes[j] = vec_final
                    self.__axis_spheres[j].set_position(np.add(self.__centre_sphere.get_position(), self.__axes[j]))
                break

        # update previous values
        for i in range(3):
            self.__prev_axes[i] = self.__axes[i]
            self.__axes[i] = np.subtract(self.__axis_spheres[i].get_position(), self.__centre_sphere.get_position())

    def get_obb(self) -> OBB:
        half_extents = list()
        axes = list()
        for i in range(3):
            half_extents.append(np.linalg.norm(self.__axes[i]))
            axes.append(np.multiply(self.__axes[i], 1 / half_extents[i]))
        return OBB(
            centre=self.__centre_sphere.get_position(),
            half_extents=half_extents,
            axes=axes
        )

class InteractiveOBB(Entity):
    def __init__(self, centre: np.ndarray):
        self.__obb_controller: OBBController = OBBController(centre=centre, radius=0.25)
        self.__obb: OBB = self.__obb_controller.get_obb()
        self.__obb_mesh = create_obb_mesh(self.__obb)
        super().__init__(model=self.__obb_mesh, color=rgb(0.9, 0.9, 0.9), scale=1, shader=basic_lighting_shader)

    def update(self):
        self.__obb: OBB = self.__obb_controller.get_obb()
        self.__obb_mesh = create_obb_mesh(self.__obb)
        self.model_setter(self.__obb_mesh)

    def get_obb(self) -> OBB:
        return self.__obb

class TriangleController:
    def __init__(self, vertex_a: np.ndarray, vertex_b: np.ndarray, vertex_c: np.ndarray, radius: float=0.25):
        vertices = (vertex_a, vertex_b, vertex_c)
        self.__vertices: list[SphereController] = [SphereController(vertices[i], radius=radius) for i in range(3)]
    def get_triangle(self) -> Triangle:
        vertices = [self.__vertices[i].get_position() for i in range(3)]
        return Triangle(
            vertices=vertices
        )

    def visible_setter(self, value):
        for vertex in self.__vertices:
            vertex.visible_setter(value)

class InteractiveTriangle(Entity):
    def __init__(self, vertex_a: np.ndarray, vertex_b: np.ndarray, vertex_c: np.ndarray):
        self.__triangle_controller: TriangleController = TriangleController(vertex_a, vertex_b, vertex_c, radius=0.25)
        self.__triangle: Triangle = self.__triangle_controller.get_triangle()
        self.__triangle_mesh = create_triangle_mesh(self.__triangle)
        super().__init__(model=self.__triangle_mesh, color=rgb(0.9, 0.9, 0.9), scale=1, shader=basic_lighting_shader)

    def update(self):
        self.__triangle = self.__triangle_controller.get_triangle()
        self.__triangle_mesh = create_triangle_mesh(self.__triangle)
        self.model_setter(self.__triangle_mesh)

    def get_triangle(self) -> Triangle:
        return self.__triangle

    def visible_setter(self, value):
        super().visible_setter(value)
        self.__triangle_controller.visible_setter(value)


NORMAL_COLOUR = rgb(0.0, 0.9, 0.0)
TOUCH_COLOUR  = rgb(0.9, 0.0, 0.0)

class OBBCollisionTest(Entity):
    def __init__(self, centre_a: np.ndarray, centre_b: np.ndarray):
        self.obb_a = InteractiveOBB(centre_a)
        self.obb_a.color_setter(NORMAL_COLOUR)
        self.obb_b = InteractiveOBB(centre_b)
        self.obb_b.color_setter(NORMAL_COLOUR)
        super().__init__()

    def update(self):
        collision = check_obb_obb_collision(self.obb_a.get_obb(), self.obb_b.get_obb())
        if collision:
            self.obb_a.color_setter(TOUCH_COLOUR)
            self.obb_b.color_setter(TOUCH_COLOUR)
        else:
            self.obb_a.color_setter(NORMAL_COLOUR)
            self.obb_b.color_setter(NORMAL_COLOUR)

class OBBTriangleCollisionTest(Entity):
    def __init__(self, obb_centre: np.ndarray, triangle_a: np.ndarray, triangle_b: np.ndarray, triangle_c: np.ndarray):
        self.obb = InteractiveOBB(obb_centre)
        self.obb.color_setter(NORMAL_COLOUR)
        self.triangle = InteractiveTriangle(triangle_a, triangle_b, triangle_c)
        self.triangle.color_setter(NORMAL_COLOUR)

        self.display_triangle = Triangle(vertices=[triangle_a, triangle_b, triangle_c])
        self.display_mesh = create_triangle_mesh(self.display_triangle)
        self.display_entity = Entity(model=self.display_mesh, color=rgba(0.0, 0.0, 1.0, 0.5), shader=basic_lighting_shader)

        self.static_obb = OBB(centre=np.array([0, 0, 0]),
                              half_extents=self.obb.get_obb().half_extents,
                              axes=[
                                  np.array([1, 0 ,0]), np.array([0, 1, 0]), np.array([0, 0, 1])
                              ])
        self.static_mesh = create_obb_mesh(self.static_obb)
        self.static_entity = Entity(model=self.static_mesh, color=rgba(0.0, 0.0, 1.0, 0.5), shader=basic_lighting_shader)

        super().__init__()

    def update(self):
        triangle = self.triangle.get_triangle()
        obb = self.obb.get_obb()
        collision = check_obb_triangle_collision(self.obb.get_obb(), triangle)

        transform = create_obb_transform(obb)

        for i in range(3):
            vertex = np.array([triangle.vertices[i][0], triangle.vertices[i][1], triangle.vertices[i][2], 1.0])
            product = np.matmul(transform, vertex)
            self.display_triangle.vertices[i] = np.array([product[0], product[1], product[2]])

        self.display_mesh = create_triangle_mesh(self.display_triangle)
        self.display_entity.model_setter(self.display_mesh)

        self.static_obb.half_extents = self.obb.get_obb().half_extents
        self.static_mesh = create_obb_mesh(self.static_obb)
        self.static_entity.model_setter(self.static_mesh)

        if collision:
            self.obb.color_setter(TOUCH_COLOUR)
            self.triangle.color_setter(TOUCH_COLOUR)
        else:
            self.obb.color_setter(NORMAL_COLOUR)
            self.triangle.color_setter(NORMAL_COLOUR)

print_key = True

def print_vertex(vertex, end="\n"):
    print(str(vertex[0]) + "f, " + str(vertex[1]) + "f, " + str(vertex[2]) + "f", end=end)

def print_triangle(name, triangle):
    print("vec3 " + name + "[] = {")
    for i in range(len(triangle.vertices)):
        if i != 0:
            print(",")
        print("    ", end="")
        print_vertex(triangle.vertices[i], end="")
    print("\n};")

class TriangleCollisionTest(Entity):
    def __init__(self, *vertices):
        if len(vertices) != 6:
            raise ValueError("Need to have 6 vertices in order to make 2 triangles.")

        self.triangle_a = InteractiveTriangle(vertices[0], vertices[1], vertices[2])
        self.triangle_a.color_setter(NORMAL_COLOUR)
        self.triangle_b = InteractiveTriangle(vertices[3], vertices[4], vertices[5])
        self.triangle_b.color_setter(NORMAL_COLOUR)

        super().__init__()

    def update(self):
        global print_key
        collision = check_triangle_triangle_collision(self.triangle_a.get_triangle(), self.triangle_b.get_triangle())
        if collision:
            self.triangle_a.color_setter(TOUCH_COLOUR)
            self.triangle_b.color_setter(TOUCH_COLOUR)
        else:
            self.triangle_a.color_setter(NORMAL_COLOUR)
            self.triangle_b.color_setter(NORMAL_COLOUR)

        if print_key:
            print_triangle("triangle_a", self.triangle_a.get_triangle())
            print_triangle("triangle_b", self.triangle_b.get_triangle())
            print_key = False


    def set_visibility(self, visibility: bool):
        self.triangle_a.visible_setter(visibility)
        self.triangle_b.visible_setter(visibility)

class TriangleNormalTest(TriangleCollisionTest):
    def __init__(self, *vertices):
        super().__init__(*vertices)
        self.triangle_a.color_setter(color.orange)
        self.triangle_b.color_setter(color.blue)
        self.__normal = VectorDisplayEntity(get_triangle_centroid(self.triangle_a.get_triangle()), np.array([0.0, 1.0, 0.0]), tube_radius=0.05)

    def update(self):
        normal = get_triangle_triangle_collision_normal(self.triangle_a.get_triangle(), self.triangle_b.get_triangle())
        if normal is None:
            self.__normal.visible_setter(False)
        else:
            self.__normal.visible_setter(True)
            self.__normal.set_vector(normal)
            self.__normal.set_position(get_triangle_centroid(self.triangle_a.get_triangle()))

class TriangleNormalTestSAT(TriangleCollisionTest):
    def __init__(self, *vertices):
        super().__init__(*vertices)
        self.triangle_a.color_setter(color.orange)
        self.triangle_b.color_setter(color.blue)
        self.__normal = VectorDisplayEntity(get_triangle_centroid(self.triangle_a.get_triangle()), np.array([0.0, 1.0, 0.0]), tube_radius=0.05)

    def update(self):
        normal = get_triangle_triangle_collision_sat(self.triangle_a.get_triangle(), self.triangle_b.get_triangle())
        if normal is None:
            self.__normal.visible_setter(False)
        else:
            self.__normal.visible_setter(True)
            self.__normal.set_vector(normal)
            self.__normal.set_position(get_triangle_centroid(self.triangle_a.get_triangle()))

triangle_index = 0
num_triangles = 0
collision_tests: list[TriangleCollisionTest] = list()

def input(key):
    global print_key
    if key == "p":
        print_key = True

def main():
    global triangle_index, num_triangles, collision_tests
    app = Ursina()
    sun = DirectionalLight()
    sun.look_at(Vec3(0, -1, 0.7))
    Sky()

    # testing - these were printed out during a triangle collision in the real code, supposedly no collision.

#    test_log = read_log_file("/home/legendmixer/CLionProjects/TekPhysics/scripts/collision_log.txt")
#    for triangle_a, triangle_b, line_num in test_log:
#        collision_tests.append(TriangleCollisionTest(
#            *triangle_a.vertices,
#            *triangle_b.vertices
#        ))
#        collision_tests[-1].set_visibility(False)
#
#    num_triangles = len(collision_tests)
#    collision_tests[0].set_visibility(True)

    test_log = read_log_file_obb_triangle("/home/legendmixer/CLionProjects/TekPhysics/scripts/collision_log.txt")
    for obb, triangle, line_num in test_log:
        if check_obb_triangle_collision(obb, triangle):
            print(f"Collision @ {line_num}")
        else:
            print(f"Nothing @ {line_num}")
#    for obb_a, obb_b, line_num in test_log:
#        if check_obb_obb_collision(obb_a, obb_b):
#            print(f"Collision @ {line_num}")
#        else:
#            print(f"Nothing @ {line_num}")

    # obb_test = InteractiveOBB(centre=np.array([0, 0 ,0]))
    # triangle_test = InteractiveTriangle(np.array([0, 0, 0]), np.array([1, 0, 0]), np.array([0, 0, 1]))

    # collision_test = OBBTriangleCollisionTest(np.array([0, -1, 0]), np.array([0.0, 1.0, 0.0]), np.array([1.0, 0.0, 0.0]), np.array([0.0, 0.0, 1.0]))

    # collision_test_2 = TriangleNormalTest(np.array([0.0, 1.0, 0.0]), np.array([1.0, 0.0, 0.0]), np.array([0.0, 0.0, 1.0]), np.array([0.0, 2.0, 0.0]), np.array([1.0, 1.0, 0.0]), np.array([0.0, 1.0, 1.0]))

    collision_test_3 = TriangleCollisionTest(np.array([-1.0, 0.0, 0.0]),
                                             np.array([1.0, 0.0, 0.0]),
                                             np.array([1.0, 1.0, 0.0]),
                                             np.array([0.0, 0.0, -1.0]),
                                             np.array([0.0, 0.0, 1.0]),
                                             np.array([0.0, 1.0, 1.0]))

    vector = VectorDisplayEntity(np.array([0.0, 0.0, 0.0]), np.array([-1.0, 0.0, 1.0]))

    EditorCamera()  # add camera controls for orbiting and moving the camera

    app.run()

if __name__ == "__main__":
    main()
