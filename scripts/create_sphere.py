import sys
from dataclasses import dataclass
import math
import os

@dataclass
class Vec3:
    x: float
    y: float
    z: float

@dataclass
class Vec2:
    x: float
    y: float

@dataclass
class Vertex:
    position: Vec3
    normal: Vec3
    tex_coord: Vec2

class MeshFile:
    def __init__(self, filepath: str):
        self.__filepath = filepath
        self.__vertices = list()
        self.__indices = list()

    def add_vertex(self, position: Vec3, normal: Vec3, tex_coord: Vec2):
        self.__vertices.append(Vertex(position, normal, tex_coord))

    def add_triangle(self, point_a: int, point_b: int, point_c: int):
        self.__indices.extend((point_a, point_b, point_c))

    def write(self):
        format_string = "VERTICES\n"
        for vertex in self.__vertices:
            format_string += f"{vertex.position.x:.5f} {vertex.position.y:.5f} {vertex.position.z:.5f} {vertex.normal.x:.5f} {vertex.normal.y:.5f} {vertex.normal.z:.5f} {vertex.tex_coord.x} {vertex.tex_coord.y}\n"
        format_string += "\nINDICES\n"
        for i in range(len(self.__indices)):
            format_string += str(self.__indices[i]) + " "
            if (i + 1) % 3 == 0:
                format_string += "\n"
        format_string += "\nLAYOUT\n3 3 2\n\n$POSITION_LAYOUT_INDEX 0\n"
        with open(self.__filepath, "w") as f:
            f.write(format_string)

def main(radius: float, horizontal_divs: int, vertical_divs: int, filepath: str):
    mesh_file = MeshFile(filepath)
    # algorithm based on: https://www.songho.ca/opengl/gl_sphere.html

    inv_radius = 1.0 / radius
    horizontal_step = 2 * math.pi / horizontal_divs
    vertical_step = math.pi / vertical_divs

    for i in range(vertical_divs + 1):
        vertical_angle = math.pi / 2 - i * vertical_step
        xz = radius * math.cos(vertical_angle)
        y = radius * math.sin(vertical_angle)

        for j in range(horizontal_divs + 1):
            horizontal_angle = j * horizontal_step

            x = xz * math.cos(horizontal_angle)
            z = xz * math.sin(horizontal_angle)

            u = (vertical_angle / math.pi) + 0.5
            v = horizontal_angle / (2.0 * math.pi)

            mesh_file.add_vertex(
                        Vec3(x, y, z),
                        Vec3(x * inv_radius, y * inv_radius, z * inv_radius),
                        Vec2(u, v)
                    )

    for i in range(vertical_divs):
        k1 = i * (horizontal_divs + 1)
        k2 = k1 + horizontal_divs + 1

        for j in range(horizontal_divs):
            if i != 0:
                mesh_file.add_triangle(k2, k1, k1 + 1)
            if i != vertical_divs - 1:
                mesh_file.add_triangle(k2, k1 + 1, k2 + 1)

            k1 += 1
            k2 += 1

    mesh_file.write()
    print(f"Wrote new sphere mesh to '{filepath}'")

if __name__ == "__main__":
    args = sys.argv
    if len(args) != 5:
        print("Usage: radius=float horizontal_divs=int vertical_divs=int")
        raise ValueError("Incorrect number of arguments!")

    radius = h_divs = v_divs = filepath = None
    # first argument will be name of file, so we can ignore
    for arg in args[1:]:
        kv_pair = arg.split("=")
        if len(kv_pair) != 2:
            raise ValueError("Arguments should take the form of 'key=value'!")
        key, value = kv_pair
        if key.lower() == "radius":
            try:
                radius = float(value)
            except ValueError:
                raise ValueError("Invalid radius!")
        elif key.lower() == "horizontal_divs":
            try:
                h_divs = int(value)
            except ValueError:
                raise ValueError("Invalid number of horizontal divisions!")
        elif key.lower() == "vertical_divs":
            try:
                v_divs = int(value)
            except ValueError:
                raise ValueError("Invalid number of vertical divisions!")
        elif key.lower() == "filepath":
            if not os.path.isdir(os.path.dirname(os.path.abspath(value))):
                raise ValueError("File is in a directory that does not exist!")
            filepath = value
        else:
            raise ValueError(f"Invalid argument: '{key}'!")
    main(radius, h_divs, v_divs, filepath)

