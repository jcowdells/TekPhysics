import os
import re

macro_func_pattern = re.compile(
    r'^\s*#define\s+([A-Za-z_]\w*)\s*\(([^)]*)\)',
)

func_pattern = re.compile(
    r'^\s*'
    r'(?:[A-Za-z_]\w*\s+)*'
    r'(?!(?:if|while|for|switch|return|sizeof)\b)'
    r'([A-Za-z_]\w*)'
    r'\s*\(([^)]*)\)\s*'
    r'\{?\s*$'
)

struct_pattern = re.compile(
    r'^\s*struct\s+([A-Za-z_]\w*)\s*\{'
)

typedef_struct_pattern = re.compile(
    r'^\s*typedef\s+struct(?:\s+([A-Za-z_]\w*))?\s*\{'
)

PROJECT_ROOT = "../"
SEARCH = ["core", "tekgl", "tekphys", "tekgui", "main.c", "tekgl.h"]

def get_path(root, *args):
    return os.path.abspath(os.path.join(root, *args))

def read_file(file_path):
    with open(file_path) as f:
        file_data = f.read()
    return file_data

def generate_file_tree():
    root_file_tree = dict()
    file_queue = list()
    file_queue.append((PROJECT_ROOT, root_file_tree))

    first_pass_complete = False

    while len(file_queue) > 0:
        current_dir, file_tree = file_queue.pop(-1)
        for loop_path_raw in os.listdir(current_dir):
            if not first_pass_complete and not loop_path_raw in SEARCH:
                continue
            loop_path = get_path(current_dir, loop_path_raw)
            if os.path.isdir(loop_path):
                next_branch = dict()
                file_tree[loop_path_raw] = next_branch
                file_queue.append((loop_path, next_branch))
            else:
                file_tree[loop_path_raw] = loop_path
        first_pass_complete = True

    return root_file_tree

def process_file(file_path):
    file_data = read_file(file_path)
    for line in file_data.split("\n"):
        macro_func_match = macro_func_pattern.match(line)
        func_match = func_pattern.match(line)
        struct_match = struct_pattern.match(line)
        typedef_struct_match = typedef_struct_pattern.match(line)
        if macro_func_match:
            print(f"Macro: {macro_func_match.group(1)}")
        elif func_match:
            print(f"Func: {func_match.group(1)}")
        elif struct_match:
            print(f"Struct: {struct_match.group(1)}")
        elif typedef_struct_match:
            print(f"TDStruct: {typedef_struct_match.group(1)}")

def generate_project_data(file_tree):
    file_queue = list()
    file_queue.append(generate_file_tree())
    while len(file_queue) > 0:
        file_tree = file_queue.pop(-1)
        for file_name, file_data in file_tree.items():
            if type(file_data) == dict:
               file_queue.append(file_data)
            else:
                print(f"-------------------------------- {file_data}")
                process_file(file_data)

def main():
    generate_project_data(generate_file_tree())

if __name__ == "__main__":
    main()
