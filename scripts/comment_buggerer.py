# import project_crawler
import tkinter as tk
import tkinter.ttk as ttk
from dataclasses import dataclass

class EditorPanel(ttk.Frame):
    HEIGHT = 20

    def __init__(self, root, title="Default Title", editable=False):
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

    def write(self, text):
        if not self.editable:
            self.text.config(state=tk.NORMAL)
        self.text.clear()
        self.text.insert(0, text)
        if not self.editable:
            self.text.config(state=tk.DISABLED)

class Window(tk.Tk):
    def __init__(self):
        super().__init__()
        self.title("Comment Buggerer v1.0 Alpha")
        self.geometry("1280x720")

        self.editor = ttk.Frame(self)

        self.original = EditorPanel(self.editor, title="Original Code")
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

def main():
    window = Window()
    window.mainloop()

if __name__ == "__main__":
    main()
