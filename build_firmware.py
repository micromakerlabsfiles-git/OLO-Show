import os
import sys
import re
import shutil
import subprocess
import threading
import tkinter as tk
from tkinter import ttk, filedialog, messagebox

# Try to load PIL for a small logo icon
try:
    from PIL import Image, ImageTk
    HAS_PIL = True
except ImportError:
    HAS_PIL = False

# Resolve project root
if getattr(sys, 'frozen', False):
    PROJECT_ROOT = os.path.dirname(sys.executable)
else:
    PROJECT_ROOT = os.path.dirname(os.path.abspath(__file__))


class BuildToolApp:
    MAX_SLOTS = 10

    def __init__(self, root):
        self.root = root
        self.root.title("OLO Show — Firmware Builder")
        self.root.geometry("730x680")
        self.root.resizable(True, True)
        self.root.configure(bg="#1a1a2e")

        # Color palette
        BG       = "#1a1a2e"
        CARD     = "#16213e"
        ACCENT   = "#ff793f"
        ACCENT2  = "#cd6133"
        TEXT     = "#d1d8e0"
        WHITE    = "#ffffff"
        CYAN     = "#00e5ff"

        self.bg = BG; self.card = CARD; self.accent = ACCENT
        self.text = TEXT

        # ttk styles
        s = ttk.Style()
        s.theme_use('clam')
        s.configure('.',              background=BG,   foreground=TEXT)
        s.configure('TFrame',         background=BG)
        s.configure('Card.TFrame',    background=CARD)
        s.configure('TLabel',         background=BG,   foreground=TEXT,  font=("Segoe UI", 10))
        s.configure('Card.TLabel',    background=CARD, foreground=TEXT,  font=("Segoe UI", 10))
        s.configure('H1.TLabel',      background=BG,   foreground=WHITE, font=("Segoe UI", 17, "bold"))
        s.configure('Sub.TLabel',     background=BG,   foreground=ACCENT,font=("Segoe UI", 10, "italic"))
        s.configure('TCheckbutton',   background=CARD, foreground=TEXT,  font=("Segoe UI", 9))
        s.configure('TCombobox',      fieldbackground="#0d0d1a", foreground=WHITE, background=CARD)
        s.map('TCombobox',            fieldbackground=[('readonly','#0d0d1a')],
                                      foreground=[('readonly', WHITE)])

        # HEADER
        hdr = ttk.Frame(root)
        hdr.pack(fill="x", padx=20, pady=(15, 4))
        ttk.Label(hdr, text="OLO SHOW",        style="H1.TLabel").pack(anchor="w")
        ttk.Label(hdr, text="Firmware Builder — compile animation .h files into flashable .bin",
                  style="Sub.TLabel").pack(anchor="w")

        sep = tk.Frame(root, height=1, bg=ACCENT)
        sep.pack(fill="x", padx=20, pady=(2, 6))

        # FORM CARD
        form = ttk.Frame(root, style="Card.TFrame", padding=(15, 12))
        form.pack(fill="both", expand=True, padx=20, pady=(0, 10))
        form.columnconfigure(0, weight=1)
        form.rowconfigure(0, weight=1) # The slots group will expand

        # Target Slots Group
        slots_lf = ttk.LabelFrame(form, text="Target Slots Configuration", style="Card.TFrame")
        slots_lf.grid(row=0, column=0, columnspan=2, sticky="nsew", pady=(0, 8))
        slots_lf.columnconfigure(0, weight=1)
        slots_lf.rowconfigure(1, weight=1) # Row 1 has the scrollable list

        # Top Buttons for slot actions
        btns_f = ttk.Frame(slots_lf, style="Card.TFrame")
        btns_f.grid(row=0, column=0, sticky="ew", padx=10, pady=(5, 5))
        
        tk.Button(btns_f, text="➕ Add Video Source (.h)", command=self.add_slot,
                  bg=ACCENT, fg=WHITE, activebackground=ACCENT2, activeforeground=WHITE,
                  bd=0, padx=8, pady=4, font=("Segoe UI", 9, "bold"), cursor="hand2").pack(side="left", padx=(0, 5))
                  
        tk.Button(btns_f, text="📦 Load ZIP of Headers", command=self.load_zip_headers,
                  bg="#27ae60", fg=WHITE, activebackground="#2ecc71", activeforeground=WHITE,
                  bd=0, padx=8, pady=4, font=("Segoe UI", 9, "bold"), cursor="hand2").pack(side="left", padx=(0, 5))

        tk.Button(btns_f, text="🧹 Clear All Paths", command=self.clear_all_paths,
                  bg="#f39c12", fg=WHITE, activebackground="#f1c40f", activeforeground=WHITE,
                  bd=0, padx=8, pady=4, font=("Segoe UI", 9, "bold"), cursor="hand2").pack(side="left", padx=(0, 5))

        tk.Button(btns_f, text="🗑️ Remove All Slots", command=self.remove_all_slots,
                  bg="#c0392b", fg=WHITE, activebackground="#e74c3c", activeforeground=WHITE,
                  bd=0, padx=8, pady=4, font=("Segoe UI", 9, "bold"), cursor="hand2").pack(side="left")

        # Scrollable Canvas
        canvas_slots = tk.Canvas(slots_lf, borderwidth=0, highlightthickness=0, bg=CARD, height=180)
        scrollbar = ttk.Scrollbar(slots_lf, orient="vertical", command=canvas_slots.yview)
        
        # Configure scrollable frame
        self.scroll_frame = ttk.Frame(canvas_slots, style="Card.TFrame")
        self.scroll_frame.columnconfigure(1, weight=1)
        
        self.scroll_frame.bind(
            "<Configure>",
            lambda e: canvas_slots.configure(
                scrollregion=canvas_slots.bbox("all")
            )
        )
        
        canvas_window = canvas_slots.create_window((0, 0), window=self.scroll_frame, anchor="nw")
        canvas_slots.configure(yscrollcommand=scrollbar.set)
        
        # Configure dynamic width matching
        canvas_slots.bind('<Configure>', lambda e: canvas_slots.itemconfig(canvas_window, width=e.width))
        
        # Bind MouseWheel to canvas_slots
        def _on_mousewheel(event):
            canvas_slots.yview_scroll(int(-1 * (event.delta / 120)), "units")
        canvas_slots.bind_all("<MouseWheel>", _on_mousewheel)
        
        canvas_slots.grid(row=1, column=0, sticky="nsew")
        scrollbar.grid(row=1, column=1, sticky="ns")

        self.slot_vars = []
        for i in range(10):
            self.slot_vars.append(tk.StringVar())
            
        self.rebuild_slots_ui()

        # Pre-fill with existing workspace files
        self._prefill_default()

        # Options Row: board + display
        opts = ttk.Frame(form, style="Card.TFrame")
        opts.grid(row=1, column=0, columnspan=2, sticky="we", pady=(8, 0))
        opts.columnconfigure(0, weight=1)
        opts.columnconfigure(1, weight=1)

        # Display driver
        ttk.Label(opts, text="Display Driver:", style="Card.TLabel").grid(
            row=0, column=0, sticky="w", pady=(0, 4))
        self.display_var = tk.StringVar(value='SSD1306 0.96" OLED')
        self.display_combo = ttk.Combobox(
            opts, textvariable=self.display_var,
            values=['SSD1306 0.96" OLED', 'SH1106 1.3" OLED'],
            state="readonly", width=22)
        self.display_combo.grid(row=1, column=0, sticky="we", padx=(0, 8))

        # Target board
        ttk.Label(opts, text="Target Board:", style="Card.TLabel").grid(
            row=0, column=1, sticky="w", pady=(0, 4))
        self.board_var = tk.StringVar(value="ESP32-C3 Supermini (esp32c3)")
        self.board_combo = ttk.Combobox(
            opts, textvariable=self.board_var,
            values=["ESP32 Dev Module (esp32dev)",
                    "ESP32-C3 Supermini (esp32c3)",
                    "Raspberry Pi Pico W (pico_w)"],
            state="readonly", width=24)
        self.board_combo.grid(row=1, column=1, sticky="we")

        # Row 2 — checkboxes
        chk_row = ttk.Frame(form, style="Card.TFrame")
        chk_row.grid(row=2, column=0, columnspan=2, sticky="w", pady=(8, 0))

        self.sound_var = tk.BooleanVar(value=True)
        tk.Checkbutton(chk_row, text="Enable Buzzer Feedback",
                       variable=self.sound_var,
                       bg=CARD, fg=TEXT, selectcolor="#0d0d1a",
                       activebackground=CARD, activeforeground=TEXT,
                       font=("Segoe UI", 9)).pack(side="left", padx=(0, 20))

        # Output path hint label
        self.out_label = ttk.Label(form, text="Output → (Multiple files will be built)",
                                   style="Card.TLabel",
                                   foreground="#7f8c8d")
        self.out_label.grid(row=3, column=0, columnspan=2, sticky="w", pady=(6, 0))

        # Row 4 — Build button
        self.build_btn = tk.Button(
            form, text="🚀  BUILD CUSTOM FIRMWARE BINARY",
            command=self.start_build_thread,
            bg=ACCENT, fg=WHITE, activebackground=ACCENT2,
            activeforeground=WHITE, bd=0, pady=10,
            font=("Segoe UI", 12, "bold"), cursor="hand2")
        self.build_btn.grid(row=4, column=0, columnspan=2, sticky="we", pady=(8, 0))

        # Row 5 — log label
        ttk.Label(form, text="Build Log:", style="Card.TLabel").grid(
            row=5, column=0, columnspan=2, sticky="w", pady=(8, 2))

        # Row 6 — log terminal
        log_frame = tk.Frame(form, bg=CARD)
        log_frame.grid(row=6, column=0, columnspan=2, sticky="nsew", pady=(0, 4))
        log_frame.columnconfigure(0, weight=1)
        log_frame.rowconfigure(0, weight=1)
        form.rowconfigure(6, weight=1)

        self.log_text = tk.Text(
            log_frame, bg="#0a0a14", fg=CYAN, font=("Consolas", 10),
            wrap="word", insertbackground=CYAN, relief="flat", bd=0)
        self.log_text.grid(row=0, column=0, sticky="nsew")

        sb = tk.Scrollbar(log_frame, command=self.log_text.yview,
                          bg=CARD, troughcolor=CARD)
        sb.grid(row=0, column=1, sticky="ns")
        self.log_text.config(yscrollcommand=sb.set)

    def _prefill_default(self):
        """Pre-fill slots with existing include headers."""
        inc_dir = os.path.join(PROJECT_ROOT, "include")
        if not os.path.exists(inc_dir):
            return
            
        h_files = [f for f in os.listdir(inc_dir) if re.match(r'video\d+\.h$', f.lower())]
        if not h_files:
            return
            
        def get_index(name):
            match = re.search(r'video(\d+)\.h$', name.lower())
            return int(match.group(1)) if match else 999
        h_files.sort(key=get_index)
        
        max_idx = get_index(h_files[-1])
        if max_idx > len(self.slot_vars):
            self.slot_vars = [tk.StringVar() for _ in range(max_idx)]
            self.rebuild_slots_ui()
            
        for f in h_files:
            idx = get_index(f) - 1
            if idx >= 0 and idx < len(self.slot_vars):
                path = os.path.join(inc_dir, f)
                try:
                    with open(path, "r", encoding="utf-8", errors="ignore") as file_obj:
                        hdr_text = file_obj.read()
                    if "TOTAL_FRAMES = 0" not in hdr_text:
                        self.slot_vars[idx].set(os.path.abspath(path))
                except Exception:
                    pass

    def add_slot(self):
        self.slot_vars.append(tk.StringVar())
        self.rebuild_slots_ui()

    def clear_all_paths(self):
        for var in self.slot_vars:
            var.set("")

    def remove_all_slots(self):
        if messagebox.askyesno("Confirm Reset", "Are you sure you want to remove all slots?"):
            self.slot_vars = [tk.StringVar()]
            self.rebuild_slots_ui()

    def remove_slot(self, idx):
        if len(self.slot_vars) <= 1:
            self.slot_vars[0].set("")
            return
        self.slot_vars.pop(idx)
        self.rebuild_slots_ui()

    def load_zip_headers(self):
        import zipfile
        fpath = filedialog.askopenfilename(
            title="Select ZIP file of C++ headers",
            filetypes=[("ZIP files", "*.zip")]
        )
        if not fpath:
            return
        
        try:
            with zipfile.ZipFile(fpath, 'r') as zip_ref:
                # Find all video*.h files in zip
                h_files = [f for f in zip_ref.namelist() if re.match(r'.*video\d+\.h$', f.lower())]
                if not h_files:
                    messagebox.showerror("Error", "No video*.h files found in the ZIP.")
                    return
                
                # Extract them to include directory
                dest_dir = os.path.join(PROJECT_ROOT, "include")
                os.makedirs(dest_dir, exist_ok=True)
                
                # Sort by index
                def get_index(name):
                    match = re.search(r'video(\d+)\.h$', name.lower())
                    return int(match.group(1)) if match else 999
                h_files.sort(key=get_index)
                
                self.slot_vars.clear()
                
                for f in h_files:
                    filename = os.path.basename(f)
                    dest_path = os.path.join(dest_dir, filename)
                    with zip_ref.open(f) as src_f, open(dest_path, "wb") as dest_f:
                        dest_f.write(src_f.read())
                    self.slot_vars.append(tk.StringVar(value=os.path.abspath(dest_path)))
                    
            self.rebuild_slots_ui()
            self.append_log(f"[Build] Extracted and loaded {len(h_files)} slots from ZIP.\n")
            messagebox.showinfo("Success", f"Loaded {len(h_files)} slot headers from ZIP successfully!")
        except Exception as e:
            messagebox.showerror("ZIP Error", f"Failed to extract ZIP: {e}")

    def rebuild_slots_ui(self):
        for child in self.scroll_frame.winfo_children():
            child.destroy()
            
        for i, var in enumerate(self.slot_vars):
            row_f = ttk.Frame(self.scroll_frame, style="Card.TFrame")
            row_f.pack(fill="x", expand=True, pady=2)
            row_f.columnconfigure(1, weight=1)
            
            lbl = ttk.Label(row_f, text=f"Slot {i+1} (video{i+1}.h):", width=16, style="Card.TLabel")
            lbl.grid(row=0, column=0, sticky="w", padx=(5, 10))
            
            ent = tk.Entry(row_f, textvariable=var, bg="#0d0d1a", fg="#ffffff", 
                           insertbackground="#ffffff", bd=0, font=("Consolas", 9))
            ent.grid(row=0, column=1, sticky="we", padx=(0, 5), ipady=3)
            
            browse_btn = tk.Button(row_f, text="Browse…", command=lambda idx=i: self.browse_slot(idx),
                                   bg="#ff793f", fg="#ffffff", activebackground="#cd6133", activeforeground="#ffffff",
                                   bd=0, padx=8, pady=2, font=("Segoe UI", 8, "bold"), cursor="hand2")
            browse_btn.grid(row=0, column=2, padx=2)
            
            clear_btn = tk.Button(row_f, text="Clear", command=lambda v=var: v.set(""),
                                  bg="#c0392b", fg="#ffffff", activebackground="#e74c3c", activeforeground="#ffffff",
                                  bd=0, padx=8, pady=2, font=("Segoe UI", 8, "bold"), cursor="hand2")
            clear_btn.grid(row=0, column=3, padx=2)
            
            remove_btn = tk.Button(row_f, text="Remove", command=lambda idx=i: self.remove_slot(idx),
                                   bg="#7f8c8d", fg="#ffffff", activebackground="#95a5a6", activeforeground="#ffffff",
                                   bd=0, padx=8, pady=2, font=("Segoe UI", 8, "bold"), cursor="hand2")
            remove_btn.grid(row=0, column=4, padx=(2, 5))

    def browse_slot(self, idx):
        fpath = filedialog.askopenfilename(
            title=f"Select Header for Slot {idx+1}",
            filetypes=[("Header files", "*.h"), ("All files", "*.*")]
        )
        if fpath:
            self.slot_vars[idx].set(os.path.abspath(fpath))

    def append_log(self, text):
        self.log_text.insert("end", text)
        self.log_text.see("end")
        self.root.update_idletasks()

    def start_build_thread(self):
        self.build_btn.config(state="disabled", text="🔧  COMPILING…")
        self.log_text.delete("1.0", "end")
        t = threading.Thread(target=self.run_compilation, daemon=True)
        t.start()

    def run_compilation(self):
        # 1. Copy active headers or create placeholders
        active_count = 0
        num_slots = len(self.slot_vars)
        
        # Clean any old video*.h files in include folder first
        inc_dir = os.path.join(PROJECT_ROOT, "include")
        if os.path.exists(inc_dir):
            for file_name in os.listdir(inc_dir):
                if re.match(r'^video\d+\.h$', file_name.lower()):
                    try:
                        os.remove(os.path.join(inc_dir, file_name))
                    except Exception:
                        pass
        
        for i in range(num_slots):
            h_file = self.slot_vars[i].get().strip()
            dest_h = os.path.join(PROJECT_ROOT, "include", f"video{i+1}.h")
            
            if h_file:
                if not os.path.exists(h_file):
                    self.append_log(f"[Error] Selected file for Slot {i+1} does not exist: {h_file}\n")
                    self.reset_build_btn(); return
                
                self.append_log(f"[Build] Copying Slot {i+1}: {os.path.basename(h_file)} → include/video{i+1}.h...\n")
                try:
                    os.makedirs(os.path.dirname(dest_h), exist_ok=True)
                    shutil.copy(h_file, dest_h)
                    active_count += 1
                except Exception as e:
                    self.append_log(f"[Error] Copy failed for Slot {i+1}: {e}\n")
                    self.reset_build_btn(); return
            else:
                # Create empty placeholder file so compiler doesn't fail
                self.append_log(f"[Build] Creating placeholder empty header for Slot {i+1}...\n")
                try:
                    with open(dest_h, "w", encoding="utf-8") as f:
                        f.write(f"""// OLO Show Slot {i+1} Empty Placeholder
#ifndef VIDEO{i+1}_H
#define VIDEO{i+1}_H
#include <Arduino.h>
const int VIDEO{i+1}_TOTAL_FRAMES = 0;
const int VIDEO{i+1}_FRAME_DELAY  = 100;
const unsigned char video{i+1}_frames[1][1024] PROGMEM = {{ {{0}} }};
#endif
""")
                except Exception as e:
                    self.append_log(f"[Error] Creating placeholder failed for Slot {i+1}: {e}\n")
                    self.reset_build_btn(); return

        # Generate GeneratedVideos.h registry header
        self.append_log("[Build] Writing include/GeneratedVideos.h…\n")
        try:
            gen_path = os.path.join(PROJECT_ROOT, "include", "GeneratedVideos.h")
            with open(gen_path, "w", encoding="utf-8") as f:
                f.write("// OLO Show Dynamic Videos Registry\n")
                f.write("#ifndef GENERATED_VIDEOS_H\n")
                f.write("#define GENERATED_VIDEOS_H\n\n")
                f.write("#include <Arduino.h>\n\n")
                
                for i in range(num_slots):
                    f.write(f'#include "video{i+1}.h"\n')
                    
                f.write(f"\n#define MAX_ANIM_SLOTS {num_slots}\n\n")
                f.write("struct SlotInfo {\n")
                f.write("  const unsigned char (*frames)[1024];\n")
                f.write("  int total_frames;\n")
                f.write("  int frame_delay;\n")
                f.write("};\n\n")
                
                f.write("const SlotInfo SLOT_TABLE[MAX_ANIM_SLOTS] = {\n")
                for i in range(num_slots):
                    f.write(f"  {{ video{i+1}_frames, VIDEO{i+1}_TOTAL_FRAMES, VIDEO{i+1}_FRAME_DELAY }},\n")
                f.write("};\n\n")
                f.write("#endif // GENERATED_VIDEOS_H\n")
            self.append_log("[Build] GeneratedVideos.h registry written.\n")
        except Exception as e:
            self.append_log(f"[Error] Writing GeneratedVideos.h failed: {e}\n")
            self.reset_build_btn(); return

        if active_count == 0:
            self.append_log("[Warning] No animation files loaded! Building with empty slots.\n")

        # 2. Patch Config.h
        config_path = os.path.join(PROJECT_ROOT, "include", "Config.h")
        self.append_log("[Build] Patching include/Config.h…\n")
        try:
            with open(config_path, 'r', encoding='utf-8', errors='ignore') as f:
                cfg = f.read()
            if 'SSD1306' in self.display_var.get():
                cfg = re.sub(r'//#define\s+USE_SSD1306', '#define USE_SSD1306', cfg)
                cfg = re.sub(r'#define\s+USE_SH1106',    '//#define USE_SH1106', cfg)
            else:
                cfg = re.sub(r'#define\s+USE_SSD1306',    '//#define USE_SSD1306', cfg)
                cfg = re.sub(r'//#define\s+USE_SH1106',  '#define USE_SH1106', cfg)
            sound_str = "true" if self.sound_var.get() else "false"
            cfg = re.sub(r'#define\s+SOUND_ENABLED\s+\w+',
                         f'#define SOUND_ENABLED      {sound_str}', cfg)
            with open(config_path, 'w', encoding='utf-8') as f:
                f.write(cfg)
            self.append_log("[Build] Config.h patched.\n")
        except Exception as e:
            self.append_log(f"[Error] Config patch failed: {e}\n")
            self.reset_build_btn(); return

        # 3. Locate pio
        pio = self._find_pio()
        self.append_log(f"[Build] PlatformIO path: {pio}\n")

        # 4. Env
        env_map = {
            "ESP32 Dev Module (esp32dev)":    "esp32dev",
            "ESP32-C3 Supermini (esp32c3)":   "esp32c3",
            "Raspberry Pi Pico W (pico_w)":   "pico_w",
        }
        env = env_map.get(self.board_var.get(), "esp32c3")
        self.append_log(f"[Build] Compiling env={env}…\n\n")

        # 5. Run PlatformIO
        try:
            proc = subprocess.Popen(
                [pio, "run", "-e", env],
                stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
                text=True, cwd=PROJECT_ROOT,
                creationflags=subprocess.CREATE_NO_WINDOW if sys.platform == 'win32' else 0
            )
            for line in proc.stdout:
                self.append_log(line)
            proc.wait()

            if proc.returncode == 0:
                self.append_log("\n[Success] Compilation complete!\n")

                src_bin = os.path.join(PROJECT_ROOT, ".pio", "build", env, "firmware.bin")
                if os.path.exists(src_bin):
                    # Check firmware size is below 3MB
                    size_bytes = os.path.getsize(src_bin)
                    size_mb = size_bytes / (1024 * 1024)
                    self.append_log(f"[Build] Compiled firmware size: {size_mb:.2f} MB ({size_bytes} bytes)\n")
                    
                    if size_bytes > 3 * 1024 * 1024:
                        self.append_log(f"[Error] Firmware size ({size_mb:.2f} MB) exceeds the 3.0 MB limit!\n")
                        messagebox.showerror(
                            "Firmware Size Error ⚠️",
                            f"Build completed, but firmware size is {size_mb:.2f} MB, which exceeds the 3.0 MB limit!\n\n"
                            "Please reduce the number of animation files or frames to keep it under 3.0 MB."
                        )
                        self.reset_build_btn()
                        return

                    # Output 1 — project root directory
                    out_bin = os.path.join(PROJECT_ROOT, "firmware.bin")
                    shutil.copy(src_bin, out_bin)
                    self.append_log(f"[Success] firmware.bin → {out_bin}\n")

                    # Output 2 — dist/ for browser flasher
                    dist_bin = os.path.join(PROJECT_ROOT, "dist", "firmware.bin")
                    os.makedirs(os.path.dirname(dist_bin), exist_ok=True)
                    shutil.copy(src_bin, dist_bin)
                    self.append_log(f"[Success] dist/firmware.bin → {dist_bin}\n")

                    src_boot = os.path.join(PROJECT_ROOT, ".pio", "build", env, "bootloader.bin")
                    src_part = os.path.join(PROJECT_ROOT, ".pio", "build", env, "partitions.bin")
                    
                    if os.path.exists(src_boot):
                        dist_boot = os.path.join(PROJECT_ROOT, "dist", "bootloader.bin")
                        shutil.copy(src_boot, dist_boot)
                        self.append_log(f"[Success] dist/bootloader.bin → {dist_boot}\n")
                        
                    if os.path.exists(src_part):
                        dist_part = os.path.join(PROJECT_ROOT, "dist", "partitions.bin")
                        shutil.copy(src_part, dist_part)
                        self.append_log(f"[Success] dist/partitions.bin → {dist_part}\n")

                    messagebox.showinfo(
                        "Build Successful ✅",
                        f"Firmware compiled successfully!\n\n"
                        f"firmware.bin saved to project dist folder.\n\n"
                        "Use the OLO Show web page to flash it."
                    )
                else:
                    self.append_log("[Error] Binary not found after build.\n")
                    messagebox.showerror("Build Error", "Output binary missing after successful build.")
            else:
                self.append_log(f"\n[Error] Compiler exited with code {proc.returncode}\n")
                messagebox.showerror("Build Failed", "PlatformIO compilation failed — see the log.")
        except Exception as e:
            self.append_log(f"[Error] Could not spawn process: {e}\n")
            messagebox.showerror("System Error", str(e))

        self.reset_build_btn()

    def _find_pio(self):
        """Resolve pio/pio.exe path."""
        up = os.environ.get("USERPROFILE") or os.environ.get("HOME", "")
        local = os.path.join(up, ".platformio", "penv", "Scripts", "pio.exe")
        if os.path.exists(local):
            return local
        return "pio"

    def reset_build_btn(self):
        self.build_btn.config(state="normal", text="🚀  BUILD CUSTOM FIRMWARE BINARY")


if __name__ == "__main__":
    os.chdir(PROJECT_ROOT)

    if not os.path.exists(os.path.join(PROJECT_ROOT, "platformio.ini")):
        root_tmp = tk.Tk(); root_tmp.withdraw()
        messagebox.showerror(
            "Path Error",
            "Cannot find 'platformio.ini'.\n\n"
            "Please place build_firmware.py inside the OLO_Show project folder and try again."
        )
        sys.exit(1)

    root = tk.Tk()
    app  = BuildToolApp(root)
    root.mainloop()
