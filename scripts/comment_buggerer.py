# import project_crawler
import tkinter as tk
import tkinter.ttk as ttk
from dataclasses import dataclass
import re
from enum import Enum

class HighlighterMode(Enum):
    KEYWORD = 0
    TYPE    = 1
    MACRO   = 2
    STRING  = 3

class Highlighter:
    @staticmethod
    def split_text(text):
        text += "\0"
        start_index = -1
        word_builder = ""
        word_list = list()
        delimiters = (" ", "\n", ";", ",", ".", "(", ")", "{", "}", "\0")
        for i, char in enumerate(text):
            if char in delimiters:
                if word_builder == "":
                    continue
                word_list.append((word_builder, start_index, i))
                word_builder = ""
            else:
                if word_builder == "":
                    start_index = i
                word_builder += char

        return word_list

    def __init__(self):
        self.rules = dict()

    def add_rule(self, rule, colour):
        pass

    def __generate_keywords(self, word_list):
        word_list = self.split_text(text)
        

    def generate_tags(self, text):

        for rule, colour in self.rules.items():
            if 
        word_list = self.split_text(text)


class EditorPanel(ttk.Frame):
    HEIGHT = 20

    def __init__(self, root, title="Default Title", editable=False, highlighter=None):
        super().__init__(root)
        self.title = ttk.Label(self, text=title)
        self.text = tk.Text(self)
        self.scroll = ttk.Scrollbar(self, orient=tk.VERTICAL, command=self.text.yview)
        self.editable = editable
        if not editable:
            self.text.config(state=tk.DISABLED)
        self.title.pack(side=tk.TOP)
        self.scroll.pack(side=tk.RIGHT, fill=tk.Y)
        self.text.pack(expand=True, fill=tk.Y)
        self.highlighter = highlighter

    def __guard(self, func, *args, **kwargs):
        if not self.editable:
            self.text.config(state=tk.NORMAL)
        func(*args, **kwargs)
        if not self.editable:
            self.text.config(state=tk.DISABLED)

    def __write(self, text):
        self.text.delete("1.0", tk.END)
        self.text.insert(tk.INSERT, text)

    def write(self, text):
        self.__guard(self.__write, text)

    def __str__(self):
        return self.text.get("1.0", "end-1c")

    def __highlight(self):
        text = str(self)
        print(self.split_text(text))

    def highlight(self):
        if self.highlighter is None:
            return
        self.__guard(self.__highlight)

class Window(tk.Tk):
    def __init__(self):
        super().__init__()
        self.title("Comment Buggerer v1.0 Alpha")
        self.geometry("1280x720")

        self.editor = ttk.Frame(self)

        highlighter = Highlighter()
        highlighter.add_rule(None, "int")

        self.original = EditorPanel(self.editor, title="Original Code", highlighter=highlighter)
        self.editing = EditorPanel(self.editor, title="Edited Version")
        self.final = EditorPanel(self.editor, title="Final Version", editable=True)

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

        self.original.write("#include <something.h>\n\nint main(int argc, char** argv) {\n    printf(\"Hello World!\");\n    return 0;\n}\n");
        self.original.highlight()

def main():
    window = Window()
    # window.mainloop()

if __name__ == "__main__":
    main()
