# import project_crawler
import tkinter as tk
import tkinter.ttk as ttk
from enum import Enum, auto
from dataclasses import dataclass

class HighlighterMode(Enum):
    KEYWORD = "keyword"
    TYPE    = "type"
    MACRO   = "macro"
    STRING  = "string"
    NUMBER  = "number"
    COMMENT = "comment"

class CommentMode(Enum):
    NONE        = auto()
    AWAIT_NEXT  = auto()
    SINGLE_LINE = auto()
    MULTI_LINE  = auto()
    AWAIT_FINAL = auto()

@dataclass
class Config:
    id: HighlighterMode
    colour: str

@dataclass
class Word:
    word: str
    start: int
    end: int

KEYWORDS = (
    "alignas",
    "alignof",
    "auto",
    "bool",
    "break",
    "case",
    "char",
    "const",
    "constexpr",
    "continue",
    "default",
    "do",
    "double",
    "else",
    "enum",
    "extern",
    "false",
    "float",
    "for",
    "goto",
    "if",
    "inline",
    "int",
    "long",
    "nullptr",
    "register",
    "restrict",
    "return",
    "short",
    "signed",
    "sizeof",
    "static",
    "static_assert",
    "struct",
    "switch",
    "thread_local",
    "true",
    "typedef",
    "typeof",
    "typeof_unqual",
    "union",
    "unsigned",
    "void",
    "volatile",
    "while"
)

TYPES = (
    "bool",
    "char",
    "flag",
    "short",
    "int",
    "float",
    "long",
    "double",
    "unsigned",
    "vec3",
    "vec4",
    "mat3",
    "mat4"
)

class Colour:
    BACKGROUND = "#FAF9DE"
    FOREGROUND = "#000000"
    KEYWORD = "#92005E"
    TYPE = "#348596"
    STRING = "#0106C7"
    NUMBER = "#851DC5"
    COMMENT = "#567F62"
    MACRO = "#990000"

@dataclass
class Tag:
    id: HighlighterMode
    start: str
    end: str

class Highlighter:
    @staticmethod
    def split_str(text, delimiters):
        text += "\0"
        start_index = -1
        word_builder = ""
        word_list = list()
        for i, char in enumerate(text):
            if char in delimiters:
                if word_builder == "":
                    continue
                word_list.append(Word(word_builder, start_index, i))
                word_builder = ""
            else:
                if word_builder == "":
                    start_index = i
                word_builder += char

        return word_list

    @staticmethod
    def split_text(text):
        return Highlighter.split_str(text, delimiters=(" ", "\n", "\r", ";", ",", "(", ")", "{", "}", "\0"))

    @staticmethod
    def split_line(text):
        return Highlighter.split_str(text, delimiters=("\n", "\r",))

    @staticmethod
    def create_tag(tag_id, start, end) -> Tag:
        return Tag(
            tag_id, f"1.0+{start}c", f"1.0+{end}c"
        )

    def __init__(self):
        self.rules = dict()

    def add_rule(self, rule: HighlighterMode, colour: str):
        self.rules[rule] = colour

    def __generate_keyword(self, word_list: list[Word]) -> list[Tag]:
        tags = list()
        for word in word_list:
            if word.word in KEYWORDS:
                tags.append(self.create_tag(
                    HighlighterMode.KEYWORD, word.start, word.end
                ))
        return tags

    def __generate_type(self, word_list: list[Word]) -> list[Tag]:
        tags = list()
        for word in word_list:
            if word.word.strip("*") in TYPES:
                tags.append(self.create_tag(
                    HighlighterMode.TYPE, word.start, word.end
                ))
        return tags

    def __generate_macro(self, line_list: list[Word]) -> list[Tag]:
        tags = list()
        for line in line_list:
            if line.word.startswith("#"):
                tags.append(self.create_tag(
                    HighlighterMode.MACRO, line.start, line.end
                ))
        return tags

    def __generate_comment(self, text: str) -> list[Tag]:
        tags = list()
        start_index = -1
        comment_mode = CommentMode.NONE
        for i, char in enumerate(text):
            if comment_mode == CommentMode.NONE:
                if char == "/":
                    comment_mode = CommentMode.AWAIT_NEXT
            elif comment_mode == CommentMode.AWAIT_NEXT:
                if char == "/":
                    comment_mode = CommentMode.SINGLE_LINE
                    start_index = i
                elif char == "*":
                    comment_mode = CommentMode.MULTI_LINE
                    start_index = i
                else:
                    comment_mode = CommentMode.NONE
            elif comment_mode == CommentMode.SINGLE_LINE:
                if char in ("\n", "\r"):
                    comment_mode = CommentMode.NONE
                    tags.append(self.create_tag(
                        HighlighterMode.COMMENT, start_index - 1, i
                    ))
            elif comment_mode == CommentMode.MULTI_LINE:
                if char == "*":
                    comment_mode = CommentMode.AWAIT_FINAL
            elif comment_mode == CommentMode.AWAIT_FINAL:
                if char == "/":
                    comment_mode = CommentMode.NONE
                    tags.append(self.create_tag(
                        HighlighterMode.COMMENT, start_index - 1, i + 1
                    ))
                else:
                    comment_mode = CommentMode.MULTI_LINE
        return tags

    def __generate_string(self, text: str) -> list[Tag]:
        tags = list()
        start_index = -1
        in_string = False
        for i, char in enumerate(text):
            if char == "\"" and not in_string:
                in_string = True
                start_index = i
            elif char == "\"" and in_string:
                in_string = False
                tags.append(self.create_tag(
                    HighlighterMode.STRING, start_index, i + 1
                ))
        return tags

    def __generate_number(self, word_list: list[Word]) -> list[Tag]:
        tags = list()
        for word in word_list:
            try:
                if word.word.endswith("f"):
                    check_word = word.word[:-1]
                else:
                    check_word = word.word
                float(check_word)
                tags.append(self.create_tag(
                    HighlighterMode.NUMBER, word.start, word.end
                ))
            except ValueError:
                pass
        return tags

    def generate_tags(self, text) -> list[Tag]:
        tags = list()
        word_list = self.split_text(text)
        line_list = self.split_line(text)
        for rule, colour in self.rules.items():
            if rule == HighlighterMode.KEYWORD:
                tags.extend(self.__generate_keyword(word_list))
            elif rule == HighlighterMode.TYPE:
                tags.extend(self.__generate_type(word_list))
            elif rule == HighlighterMode.NUMBER:
                tags.extend(self.__generate_number(word_list))
            elif rule == HighlighterMode.STRING:
                tags.extend(self.__generate_string(text))
            elif rule == HighlighterMode.MACRO:
                tags.extend(self.__generate_macro(line_list))
            elif rule == HighlighterMode.COMMENT:
                tags.extend(self.__generate_comment(text))
        return tags

    def generate_configs(self) -> list[Config]:
        configs = list()
        for rule, colour in self.rules.items():
            configs.append(Config(
                rule, colour
            ))
        return configs

class EditorPanel(ttk.Frame):
    HEIGHT = 20

    def __init__(self, root, title="Default Title", editable=False, highlighter=None):
        super().__init__(root)
        self.title = ttk.Label(self, text=title)
        self.text = tk.Text(self, foreground=Colour.FOREGROUND, background=Colour.BACKGROUND)
        self.scroll = ttk.Scrollbar(self, orient=tk.VERTICAL, command=self.text.yview)
        self.text.config(yscrollcommand=self.scroll.set)
        self.editable = editable
        if not editable:
            self.text.config(state=tk.DISABLED)
        self.title.pack(side=tk.TOP)
        self.scroll.pack(side=tk.RIGHT, fill=tk.Y)
        self.text.pack(expand=True, fill=tk.Y)
        self.highlighter = highlighter
        if editable and highlighter is not None:
            self.master.after(10000, self.__recolour_loop)

    def __guard(self, func, *args, **kwargs):
        if not self.editable:
            self.text.config(state=tk.NORMAL)
        func(*args, **kwargs)
        if not self.editable:
            self.text.config(state=tk.DISABLED)

    def __write(self, text):
        self.text.delete("1.0", tk.END)
        self.text.insert(tk.INSERT, text)
        self.highlight()

    def write(self, text):
        self.__guard(self.__write, text)

    def __str__(self):
        return self.text.get("1.0", "end-1c")

    def __highlight(self):
        for config in self.highlighter.generate_configs():
            self.text.tag_configure(config.id.value, foreground=config.colour)

        for tag in self.highlighter.generate_tags(str(self)):
            self.text.tag_add(tag.id.value, tag.start, tag.end)

    def highlight(self):
        if self.highlighter is None:
            return
        self.__guard(self.__highlight)

    def __recolour_loop(self):
        self.highlight()
        self.master.after(10000, self.__recolour_loop)

class Window(tk.Tk):
    def __init__(self):
        super().__init__()
        self.title("Comment Buggerer v1.0 Alpha")
        self.geometry("1280x720")

        self.editor = ttk.Frame(self)

        highlighter = Highlighter()
        highlighter.add_rule(HighlighterMode.KEYWORD, Colour.KEYWORD)
        highlighter.add_rule(HighlighterMode.TYPE, Colour.TYPE)
        highlighter.add_rule(HighlighterMode.NUMBER, Colour.NUMBER)
        highlighter.add_rule(HighlighterMode.STRING, Colour.STRING)
        highlighter.add_rule(HighlighterMode.MACRO, Colour.MACRO)
        highlighter.add_rule(HighlighterMode.COMMENT, Colour.COMMENT)

        self.original = EditorPanel(self.editor, title="Original Code", highlighter=highlighter)
        self.editing = EditorPanel(self.editor, title="Edited Version", highlighter=highlighter)
        self.final = EditorPanel(self.editor, title="Final Version", editable=True, highlighter=highlighter)

        self.original.grid(row=0, column=0, sticky="NS")
        self.editing.grid(row=0, column=1, sticky="NS")
        self.final.grid(row=0, column=2, sticky="NS")

        self.editor.columnconfigure(0, weight=1)
        self.editor.columnconfigure(1, weight=1)
        self.editor.columnconfigure(2, weight=1)
        self.editor.rowconfigure(0, weight=1)

        self.controller = ttk.Frame(self)

        self.accept_button = ttk.Button(self.controller, text="Test")
        self.accept_button.pack(expand=True, side=tk.TOP)

        self.controller.pack(expand=True, fill=tk.Y, side=tk.RIGHT)
        self.editor.pack(expand=True, fill=tk.BOTH, side=tk.LEFT)

        self.original.write("#include <something.h>\n\nint main(int argc, char** argv) {\n    printf(\"Hello World!\"); /* commment */\n    return 0;\n}\n");
        self.original.highlight()

def main():
    window = Window()
    window.mainloop()

if __name__ == "__main__":
    main()
