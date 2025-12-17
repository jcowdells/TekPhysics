import json
import os
import re
from dataclasses import dataclass
from enum import Enum, auto
import argparse
from more_itertools import unique_everseen
from anytree import Node, RenderTree
from anytree.exporter import DotExporter, UniqueDotExporter


@dataclass
class Parameter:
    param_name: str
    param_type: str

class ReportType(Enum):
    FUNCTION = auto()
    MACRO = auto()
    STRUCT = auto()
    TYPEDEF_STRUCT = auto()
    UNKNOWN = auto()

@dataclass
class Report:
    type: ReportType
    name: str
    params: list[Parameter]
    doxygen_comment: list[str] | None
    num_lines: int
    num_comments: int

@dataclass
class ReportOptions:
    display_type: bool
    display_name: bool
    display_params: bool
    display_doxygen: bool
    display_comment_ratio: bool
    display_length: bool
    warn_doxygen: bool
    warn_comment_ratio: bool
    warn_length: bool
    warn_only: bool

class FunctionNode(Node):
    def __init__(self, name, parent=None, children=None, **kwargs):
        super().__init__(name, parent, children, **kwargs)
        self.fillcolor = "white"

macro_func_pattern = re.compile(
    r'^\s*#define\s+([A-Za-z_]\w*)\s*\(([^)]*)\)',
)

func_pattern = re.compile(
    r'^\s*'
    r'(?:[A-Za-z_]\w*\s+){1,2}'
    r'(?!(?:if|while|for|switch|return|sizeof)\b)'
    r'([A-Za-z_]\w*)'
    r'\s*\(([^)]*)\)\s*'
    r'\{',
)

struct_pattern = re.compile(
    r'^\s*struct\s+([A-Za-z_]\w*)\s*\{([^}]*)}',
    re.DOTALL | re.MULTILINE
)

typedef_struct_pattern = re.compile(
    r'^\s*typedef\s+struct(?:\s+([A-Za-z_]\w*))?\s*\{([^}]*)}',
    re.DOTALL | re.MULTILINE
)

PROJECT_ROOT = "../"
SEARCH = ["core", "tekgl", "tekphys", "tekgui", "main.c", "tekgl.h"]

def get_path(root, *args):
    return os.path.abspath(os.path.join(root, *args))

def read_file(file_path):
    with open(file_path, "r", encoding="utf-8") as f:
        file_data = f.read()
    return file_data

def read_blacklist(file_path):
    blacklist = list()
    with open(file_path, "r", encoding="utf-8") as f:
        for line in f:
            if line.startswith("#"):
                continue
            blacklist.append(line.strip())
    return blacklist

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

def get_previous_comment(file_lines, end_line_number):
    if end_line_number < 0:
        return None

    i = end_line_number
    line = file_lines[i]

    # make sure that there is actually some form of comment there.
    if not (line.endswith("*/") or line.startswith("///")):
        return None

    # now iterate over lines until the start of the comment is reached.
    comment_lines = list()
    while not (line.startswith("/**") or line.startswith("///")):
        comment_lines.insert(0, line)

        i -= 1
        if i < 0:
            return None
        line = file_lines[i]

    comment_lines.insert(0, line)
    return comment_lines

def get_comment_ratio(file_lines, start_line_number, end_func=lambda l: l.startswith("}")):
    i = start_line_number
    line: str = file_lines[i]
    num_comments = 0

    while not end_func(line):
        if "//" in line:
            num_comments += 1

        if i + 1 >= len(file_lines):
            break
        i += 1
        line = file_lines[i]

    if "//" in line:
        num_comments += 1
    i += 1
    return i - start_line_number, num_comments

def get_function_data(line):
    macro_func_match = macro_func_pattern.match(line)
    func_match = func_pattern.match(line)
    struct_match = struct_pattern.match(line)
    typedef_struct_match = typedef_struct_pattern.match(line)
    if macro_func_match:
        report_type = ReportType.MACRO
        match = macro_func_match
    elif func_match:
        report_type = ReportType.FUNCTION
        match = func_match
    elif struct_match:
        report_type = ReportType.STRUCT
        match = struct_match
    elif typedef_struct_match:
        report_type = ReportType.TYPEDEF_STRUCT
        match = typedef_struct_match
    else:
        return None

    return report_type, match.group(1), match.group(2)

def generate_function_report(file_lines, function_line_number):
    line = file_lines[function_line_number]

    function_data = get_function_data(line)
    if function_data is None:
        return None

    report_type, report_name, report_args = function_data
    doxygen_comment = get_previous_comment(file_lines, function_line_number - 1)
    if report_type == ReportType.MACRO:
        num_lines, num_comments = get_comment_ratio(file_lines, function_line_number, end_func=lambda l: not l.endswith("\\"))
    else:
        num_lines, num_comments = get_comment_ratio(file_lines, function_line_number)

    return Report(
        type=report_type,
        name=report_name,
        params=report_args,
        doxygen_comment=doxygen_comment,
        num_lines=num_lines,
        num_comments=num_comments
    )

MIN_RATIO = 0.1
MAX_RATIO = 0.4
MAX_LENGTH = 60

def display_function_report(report: Report, report_options: ReportOptions):
    display = False
    comment_ratio = report.num_comments / report.num_lines

    if report_options.warn_only:
        if report_options.warn_doxygen and report.doxygen_comment is None:
            display = True

        if report_options.warn_comment_ratio and (comment_ratio < MIN_RATIO or comment_ratio > MAX_RATIO):
            display = True

        if report_options.warn_length and report.num_lines > MAX_LENGTH:
            display = True

        if not display:
            return False

    if report_options.display_name:
        print(f"Function Name: {report.name}")

    if report_options.display_type:
        print(f"Function Type: {report.type.name}")

    if report_options.display_params:
        print(f"Function Params:\n{report.params}")

    if report_options.display_doxygen:
        if report.doxygen_comment is not None:
            print(f"Doxygen Comment:\n{'\n'.join(report.doxygen_comment)}")
        else:
            print("Doxygen Comment: None")

    if report_options.display_length:
        print(f"Function Length: {report.num_lines}")

    if report_options.display_comment_ratio:
        print(f"Comment Ratio: {report.num_comments / report.num_lines}")

    if report_options.warn_doxygen:
        if report.doxygen_comment is None:
            print("WARNING: Function does not have a doxygen comment")

    if report_options.warn_comment_ratio:
        comment_ratio = report.num_comments / report.num_lines
        if comment_ratio < 0.1:
            print("WARNING: Function may be underdocumented")
        elif comment_ratio > 0.4:
            print("WARNING: Function may be overdocumented")

    if report_options.warn_length:
        if report.num_lines > 60:
            print("WARNING: Function may be too long")

    return True

def process_file(file_path, report_options):
    file_data = read_file(file_path)
    file_lines = file_data.split("\n")
    for i in range(len(file_lines)):
        report = generate_function_report(file_lines, i)
        if report is None:
            continue
        if display_function_report(report, report_options):
            print("")

def generate_project_data(file_tree, report_options):
    file_queue = list()
    file_queue.append(file_tree)
    while len(file_queue) > 0:
        file_tree = file_queue.pop(-1)
        for file_name, file_data in file_tree.items():
            if type(file_data) == dict:
               file_queue.append(file_data)
            else:
                print(f"-------------------------------- {file_data}")
                process_file(file_data, report_options)

def line_has_function(line, function_name):
    if function_name + "(" in line:
        return True
    if function_name + ")" in line:
        return True
    if function_name + ";" in line:
        return True
    if function_name + "," in line:
        return True
    return False

def find_function_usage(file, function_name, blacklist) -> list[str]:
    file_lines = file.split("\n")
    current_function = None
    usage = list()

    for i, line in enumerate(file_lines):
        function_data = get_function_data(line)
        new_data = False
        if function_data is not None and function_data[0] == ReportType.FUNCTION:
            current_function = function_data[1]
            new_data = True

        if current_function is None:
            continue

        if current_function in blacklist:
            continue

        line = line.rstrip()

        if line.startswith("#define"):
            continue

        if new_data:
            continue

        if line_has_function(line, function_name):
            if function_name != current_function:
                usage.append((i, current_function))

    usage.sort(key=lambda u: u[0])
    usage = [u[1] for u in usage]
    return list(unique_everseen(usage))

def display_function_list(function_call_stack, function_dict, sub_functions, split_functions, depth=0, parent=None):
    function_name = function_call_stack[-1]
    # print(f"{"  " * depth}{function_name}")
    for function in function_dict[function_name]:
        if function in function_call_stack:
            Node("Recursive call.red", parent=parent)
            continue
        if function in sub_functions:
            Node(f"{function}.lightblue", parent=parent)
        elif function in split_functions:
            Node(f"{function}.lightgreen", parent=parent)
        else:
            child = Node(f"{function}.white", parent=parent)
            display_function_list((*function_call_stack, function), function_dict, sub_functions, split_functions, depth=depth+1, parent=child)

def generate_function_list(file_tree, blacklist_file="blacklist.txt"):
    file_queue = list()
    file_list = list()
    file_queue.append(file_tree)

    if blacklist_file is None:
        blacklist = list()
    else:
        blacklist = read_blacklist(blacklist_file)

    while len(file_queue) > 0:
        file_tree = file_queue.pop(-1)
        for file_name, file_data in file_tree.items():
            if type(file_data) == dict:
                file_queue.append(file_data)
            else:
                file_list.append((file_data, read_file(file_data)))

    function_dict = dict()
    function_count = dict()
    for file_name, file in file_list:
        file_lines = file.split("\n")
        for i in range(len(file_lines)):
            line = file_lines[i]
            function_data = get_function_data(line)
            if function_data is None:
                continue
            function_dict[function_data[1]] = list()
            function_count[function_data[1]] = 0

    for function in function_dict.keys():
        if function in blacklist:
            continue
        for file_name, file in file_list:
            usage = find_function_usage(file, function, blacklist)
            for use in usage:
                function_dict[use].append(function)
            function_count[function] += len(usage)

    with open("function_dict.json", "w") as function_dict_file:
        json.dump(function_dict, function_dict_file)

    with open("function_count.json", "w") as function_count_file:
        json.dump(function_count, function_count_file)

    # root = Node("TekPhysics")

    # display_function_list(("main",), function_dict, parent=root)
    # UniqueDotExporter(root).to_dotfile("hierarchy.dot")

# Node styling function
def mynode(node):
    # Example: color and shape based on the node name
    name, colour = node.name.split(".")
    dot = f"shape=box, style=filled, fillcolor={colour}, label=\"{name}\""
    return dot

# Edge styling (optional)
def myedge(parent, child):
    return 'color=gray'   # simple example

def generate_function_list_from_cache():
    function_dict = None
    function_count = None
    with open("function_dict.json", "r") as function_dict_file:
        function_dict = json.load(function_dict_file)

    with open("function_count.json", "r") as function_count_file:
        function_count = json.load(function_count_file)

    sub_functions = list()
    split_functions = list()

    for function, count in function_count.items():
        num_calls = len(function_dict[function])

        if num_calls == 0: continue
        if count < 2 and num_calls <= 2: continue

        if len(function_dict[function]) > 5:
            new_functions = list()
            for i in range(5, num_calls, 5):
                split_function = f"{function} (Part {chr(ord("A") + int(i / 5) - 1)})"
                new_functions.append(split_function)
                function_dict[split_function] = function_dict[function][i-5:i]
                split_functions.append(split_function)
            function_dict[function][:num_calls - (num_calls % 5)] = new_functions

        sub_functions.append(function)

    root = Node("main.white")
    display_function_list(("main",), function_dict, sub_functions, split_functions, parent=root)
    UniqueDotExporter(
        root,
        nodeattrfunc=mynode,
        edgeattrfunc=myedge
    ).to_picture("hierarchy/main.png")

    for sub_function in sub_functions:
        root = Node(f"{sub_function}.lightblue")
        display_function_list((sub_function,), function_dict, sub_functions, split_functions, parent=root)
        UniqueDotExporter(
            root,
            nodeattrfunc=mynode,
            edgeattrfunc=myedge
        ).to_picture(f"hierarchy/{sub_function}.png")

    for sub_function in split_functions:
        root = Node(f"{sub_function}.lightgreen")
        display_function_list((sub_function,), function_dict, sub_functions, split_functions, parent=root)
        UniqueDotExporter(
            root,
            nodeattrfunc=mynode,
            edgeattrfunc=myedge
        ).to_picture(f"hierarchy/{sub_function}.png")

"""
    display_type: bool
    display_name: bool
    display_params: bool
    display_doxygen: bool
    display_comment_ratio: bool
    display_length: bool
    warn_doxygen: bool
    warn_comment_ratio: bool
    warn_length: bool
"""

def main():
    parser = argparse.ArgumentParser(
        prog="TekPhysics Project Crawler",
        description="Trawl through project and find issues and generate function listings",
        epilog="Copyright 2025 www.legendmixer.net"
    )
    parser.add_argument("-t", "--type", action="store_true")
    parser.add_argument("-n", "--name", action="store_true")
    parser.add_argument("-p", "--params", action="store_true")
    parser.add_argument("-d", "--doxygen", action="store_true")
    parser.add_argument("-c", "--comments", action="store_true")
    parser.add_argument("-l", "--length", action="store_true")
    parser.add_argument("-wd", "--warn_doxygen", action="store_true")
    parser.add_argument("-wc", "--warn_comments", action="store_true")
    parser.add_argument("-wl", "--warn_length", action="store_true")
    parser.add_argument("-wo", "--warn_only", action="store_true")

    args = parser.parse_args()

    report_options = ReportOptions(
        display_type=args.type,
        display_name=args.name,
        display_params=args.params,
        display_doxygen=args.doxygen,
        display_comment_ratio=args.comments,
        display_length=args.length,
        warn_doxygen=args.warn_doxygen,
        warn_comment_ratio=args.warn_comments,
        warn_length=args.warn_length,
        warn_only=args.warn_only
    )

    generate_project_data(generate_file_tree(), report_options)

if __name__ == "__main__":
    # main()
    generate_function_list_from_cache()
