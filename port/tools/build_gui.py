"""A small GUI front end for build_all.py, meant to be frozen into an exe
with PyInstaller (see docs/port.md) and placed in port/tools/, next to
sources_manifest.json and music-restored.zip.

usage: python port/tools/build_gui.py

Pick a platform (Windows, Linux via WSL, or Android), which games to
build, and whether to fetch sources first; click Build; watch the log.
Runs build_all.run_build() on a background thread (the same function the
CLI uses -- this is a front end, not a second implementation) and streams
its log lines into the window.

Frozen into an exe, this needs no externally-installed Python at all for
the Windows and Linux builds of AS1/Christmas (build_all.py imports and
calls extract_ipa.py/brain_dial.py/as2_data.py directly, bundled into the
same exe, rather than shelling out to a system python). Android builds
and as2_data.py's own ARM-emulation step still need a real Python on
PATH, since they run as separate processes on purpose (build_apk.py's
Android toolchain orchestration; re/scripts/emucalls.py's emulator) --
see fetch_sources.python_exe().
"""
import queue
import threading
import tkinter as tk
from tkinter import ttk, scrolledtext, messagebox

import build_all

GAMES = [("as1", "Amateur Surgeon"), ("xmas", "Amateur Surgeon Christmas"),
         ("as2", "Amateur Surgeon 2"), ("as3", "Amateur Surgeon 3")]


PLATFORM_LABELS = [("windows", "Windows"), ("linux", "Linux (via WSL)"), ("android", "Android")]


class App:
    def __init__(self, root):
        self.root = root
        root.title("Build Bleed Engine games")
        root.geometry("760x560")

        top = ttk.Frame(root, padding=10)
        top.pack(fill="x")

        ttk.Label(top, text="Platform:").grid(row=0, column=0, sticky="w")
        self.platform_var = tk.StringVar(value="windows")
        for i, (key, label) in enumerate(PLATFORM_LABELS):
            ttk.Radiobutton(top, text=label, variable=self.platform_var, value=key).grid(
                row=0, column=i + 1, sticky="w", padx=(0, 12))

        ttk.Label(top, text="Games to build:").grid(row=1, column=0, sticky="w", columnspan=4, pady=(8, 0))
        self.game_vars = {}
        for i, (key, label) in enumerate(GAMES):
            var = tk.BooleanVar(value=True)
            self.game_vars[key] = var
            ttk.Checkbutton(top, text=label, variable=var).grid(row=2, column=i, sticky="w", padx=(0, 12))

        self.fetch_var = tk.BooleanVar(value=True)
        ttk.Checkbutton(top, text="Fetch/verify sources first",
                         variable=self.fetch_var).grid(row=3, column=0, columnspan=4, sticky="w", pady=(8, 0))

        self.build_btn = ttk.Button(top, text="Build", command=self.on_build)
        self.build_btn.grid(row=4, column=0, sticky="w", pady=(10, 0))

        self.progress = ttk.Progressbar(top, mode="indeterminate")
        self.progress.grid(row=4, column=1, columnspan=3, sticky="ew", padx=(10, 0), pady=(10, 0))
        top.columnconfigure(3, weight=1)

        self.log_widget = scrolledtext.ScrolledText(root, state="disabled", font=("Consolas", 9))
        self.log_widget.pack(fill="both", expand=True, padx=10, pady=(0, 10))

        self.queue = queue.Queue()
        self.worker = None
        root.after(100, self.drain_queue)

    def log(self, line):
        # Called from the worker thread; just enqueue, never touch the
        # widget off the main thread.
        self.queue.put(line)

    def drain_queue(self):
        try:
            while True:
                line = self.queue.get_nowait()
                self.log_widget.configure(state="normal")
                self.log_widget.insert("end", line + "\n")
                self.log_widget.see("end")
                self.log_widget.configure(state="disabled")
        except queue.Empty:
            pass
        self.root.after(100, self.drain_queue)

    def on_build(self):
        games = [key for key, _ in GAMES if self.game_vars[key].get()]
        if not games:
            messagebox.showwarning("No games selected", "Pick at least one game to build.")
            return
        self.build_btn.configure(state="disabled")
        self.progress.start(12)
        self.log_widget.configure(state="normal")
        self.log_widget.delete("1.0", "end")
        self.log_widget.configure(state="disabled")

        skip_fetch = not self.fetch_var.get()
        platform = self.platform_var.get()
        self.worker = threading.Thread(target=self.do_build, args=(games, platform, skip_fetch), daemon=True)
        self.worker.start()

    def do_build(self, games, platform, skip_fetch):
        try:
            build_all.run_build(games, platform=platform, skip_fetch=skip_fetch, log=self.log)
            ok, message = True, "Build finished -- see dist/ for the packaged game(s)."
        except Exception as e:
            ok, message = False, f"Build failed: {e}"
            self.log(f"\nFAILED: {e}")
        self.root.after(0, self.build_done, ok, message)

    def build_done(self, ok, message):
        self.progress.stop()
        self.build_btn.configure(state="normal")
        if ok:
            messagebox.showinfo("Build finished", message)
        else:
            messagebox.showerror("Build failed", message)


def main():
    root = tk.Tk()
    App(root)
    root.mainloop()


if __name__ == "__main__":
    main()
