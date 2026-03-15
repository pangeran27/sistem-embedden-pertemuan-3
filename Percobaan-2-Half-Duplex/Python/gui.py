"""
Percobaan 2 — Half Duplex STM32 ↔ ESP32 ↔ Python GUI
=====================================================
User Interface (Tkinter) untuk:
  • Mengontrol 4 LED di STM32   (toggle ON/OFF)
  • Menampilkan status 2 Push Button di STM32  (real-time via polling)

Perbedaan dengan Percobaan 1 (Full Duplex):
  - Komunikasi STM32 ↔ ESP32 hanya menggunakan 1 kabel data (half-duplex)
  - Status button di-poll secara periodik oleh ESP32 (bukan event-driven)
  - Dari sisi GUI, protokol tetap sama (transparan)

Kebutuhan:
  pip install pyserial

Cara pakai:
  1. Hubungkan ESP32 ke PC via USB.
  2. Jalankan:  python gui.py
  3. Pilih COM port ESP32 → klik Connect.
  4. Klik tombol LED untuk toggle, status button otomatis update.
"""

import tkinter as tk
from tkinter import ttk, messagebox
import serial
import serial.tools.list_ports
import threading
import time


class HalfDuplexGUI:
    def __init__(self, root: tk.Tk):
        self.root = root
        self.root.title("Percobaan 2 — Half Duplex Control Panel")
        self.root.geometry("580x520")
        self.root.resizable(False, False)

        self.ser: serial.Serial | None = None
        self.running = False

        # State
        self.led_states = [False] * 4
        self.btn_states = [False] * 2

        self._build_ui()

    # ------------------------------------------------------------------ UI
    def _build_ui(self):
        # ---------- Connection ----------
        frm_conn = ttk.LabelFrame(self.root, text="Serial Connection", padding=8)
        frm_conn.pack(fill="x", padx=10, pady=(10, 4))

        ttk.Label(frm_conn, text="Port:").pack(side="left")
        self.port_var = tk.StringVar()
        self.cmb_port = ttk.Combobox(
            frm_conn, textvariable=self.port_var, width=14, state="readonly"
        )
        self.cmb_port.pack(side="left", padx=4)
        self._refresh_ports()

        ttk.Button(frm_conn, text="⟳", width=3, command=self._refresh_ports).pack(
            side="left"
        )
        self.btn_connect = ttk.Button(
            frm_conn, text="Connect", command=self._toggle_connection
        )
        self.btn_connect.pack(side="left", padx=8)

        self.lbl_status = ttk.Label(frm_conn, text="● Disconnected", foreground="red")
        self.lbl_status.pack(side="right")

        # ---------- Mode Indicator ----------
        frm_mode = ttk.Frame(self.root)
        frm_mode.pack(fill="x", padx=10, pady=(0, 2))
        ttk.Label(
            frm_mode,
            text="Mode: HALF DUPLEX — 1 wire (STM32 PA9 ↔ ESP32 GPIO16)",
            font=("Segoe UI", 9, "italic"),
            foreground="#888",
        ).pack(anchor="w")

        # ---------- LED Control ----------
        frm_led = ttk.LabelFrame(self.root, text="LED Control (STM32)", padding=10)
        frm_led.pack(fill="x", padx=10, pady=6)

        self.led_canvases = []
        self.led_buttons = []
        for i in range(4):
            col = tk.Frame(frm_led)
            col.pack(side="left", expand=True)

            cv = tk.Canvas(col, width=52, height=52, highlightthickness=0)
            cv.pack()
            cv.create_oval(6, 6, 46, 46, fill="#555555", outline="#333", width=2, tags="led")
            self.led_canvases.append(cv)

            btn = ttk.Button(
                col, text=f"LED {i+1}\nOFF", width=8,
                command=lambda idx=i: self._toggle_led(idx),
            )
            btn.pack(pady=(4, 0))
            self.led_buttons.append(btn)

        # ---------- Push Button Status ----------
        frm_btn = ttk.LabelFrame(self.root, text="Push Button Status (STM32)", padding=10)
        frm_btn.pack(fill="x", padx=10, pady=6)

        self.btn_canvases = []
        self.btn_labels = []
        for i in range(2):
            col = tk.Frame(frm_btn)
            col.pack(side="left", expand=True)

            cv = tk.Canvas(col, width=60, height=60, highlightthickness=0)
            cv.pack()
            cv.create_rectangle(8, 8, 52, 52, fill="#555555", outline="#333", width=2, tags="btn")
            self.btn_canvases.append(cv)

            lbl = ttk.Label(col, text=f"BTN {i+1}: Released", font=("Segoe UI", 10))
            lbl.pack(pady=(4, 0))
            self.btn_labels.append(lbl)

        # ---------- Log ----------
        frm_log = ttk.LabelFrame(self.root, text="Communication Log", padding=6)
        frm_log.pack(fill="both", expand=True, padx=10, pady=(6, 10))

        self.txt_log = tk.Text(frm_log, height=7, state="disabled", font=("Consolas", 9))
        sb = ttk.Scrollbar(frm_log, command=self.txt_log.yview)
        self.txt_log.configure(yscrollcommand=sb.set)
        self.txt_log.pack(side="left", fill="both", expand=True)
        sb.pack(side="right", fill="y")

    # ------------------------------------------------------------------ helpers
    def _refresh_ports(self):
        ports = [p.device for p in serial.tools.list_ports.comports()]
        self.cmb_port["values"] = ports
        if ports:
            self.port_var.set(ports[0])

    def _log(self, msg: str):
        self.txt_log.configure(state="normal")
        self.txt_log.insert("end", f"{msg}\n")
        self.txt_log.see("end")
        self.txt_log.configure(state="disabled")

    # ------------------------------------------------------------------ serial
    def _toggle_connection(self):
        if self.ser and self.ser.is_open:
            self._disconnect()
        else:
            self._connect()

    def _connect(self):
        port = self.port_var.get()
        if not port:
            messagebox.showwarning("Port", "Pilih COM port terlebih dahulu.")
            return
        try:
            self.ser = serial.Serial(port, 115200, timeout=0.1)
            self.running = True
            self.btn_connect.configure(text="Disconnect")
            self.lbl_status.configure(text="● Connected", foreground="green")
            self._log(f"Connected to {port}")

            # Start reader thread
            threading.Thread(target=self._read_loop, daemon=True).start()

            # Request initial status after MCU boots
            self.root.after(1200, lambda: self._send("STATUS"))
        except Exception as e:
            messagebox.showerror("Connection Error", str(e))

    def _disconnect(self):
        self.running = False
        if self.ser:
            try:
                self.ser.close()
            except Exception:
                pass
        self.btn_connect.configure(text="Connect")
        self.lbl_status.configure(text="● Disconnected", foreground="red")
        self._log("Disconnected")

    def _send(self, cmd: str):
        if self.ser and self.ser.is_open:
            self.ser.write((cmd + "\n").encode())
            self._log(f"→ TX: {cmd}")

    def _read_loop(self):
        while self.running:
            try:
                if self.ser and self.ser.in_waiting:
                    raw = self.ser.readline()
                    line = raw.decode(errors="replace").strip()
                    if line:
                        self.root.after(0, self._process_rx, line)
            except Exception as e:
                self.root.after(0, self._log, f"RX error: {e}")
                break
            time.sleep(0.01)

    # ------------------------------------------------------------------ protocol
    def _process_rx(self, line: str):
        """
        Proses data yang diterima dari ESP32.
        Dalam half-duplex, semua response berformat:
          S:L1:0,L2:1,L3:0,L4:0,B1:0,B2:1
        """

        if line.startswith("S:"):
            # Parse full status
            try:
                state_changed = False
                parts = line[2:].split(",")
                for part in parts:
                    key, val = part.split(":")
                    v = bool(int(val))
                    if key[0] == "L":
                        idx = int(key[1]) - 1
                        if self.led_states[idx] != v:
                            self.led_states[idx] = v
                            self._update_led_ui(idx)
                            state_changed = True
                    elif key[0] == "B":
                        idx = int(key[1]) - 1
                        if self.btn_states[idx] != v:
                            self.btn_states[idx] = v
                            self._update_btn_ui(idx)
                            state_changed = True

                # Hanya log jika ada perubahan state (mengurangi spam)
                if state_changed:
                    self._log(f"← RX: {line}")
            except (ValueError, IndexError):
                self._log(f"← RX (parse error): {line}")

        elif line.startswith("ESP32"):
            # Info message dari ESP32
            self._log(f"← {line}")

        else:
            # Log pesan lain
            self._log(f"← RX: {line}")

    # ------------------------------------------------------------------ LED
    def _toggle_led(self, idx: int):
        new_state = not self.led_states[idx]
        self.led_states[idx] = new_state
        cmd = f"LED{idx+1}:{'ON' if new_state else 'OFF'}"
        self._send(cmd)
        self._update_led_ui(idx)

    def _update_led_ui(self, idx: int):
        on = self.led_states[idx]
        color = "#FFD700" if on else "#555555"      # gold / grey
        self.led_canvases[idx].delete("led")
        self.led_canvases[idx].create_oval(
            6, 6, 46, 46, fill=color, outline="#333", width=2, tags="led"
        )
        self.led_buttons[idx].configure(text=f"LED {idx+1}\n{'ON' if on else 'OFF'}")

    # ------------------------------------------------------------------ Button
    def _update_btn_ui(self, idx: int):
        pressed = self.btn_states[idx]
        color = "#00CC66" if pressed else "#555555"  # green / grey
        text = f"BTN {idx+1}: {'Pressed' if pressed else 'Released'}"
        self.btn_canvases[idx].delete("btn")
        self.btn_canvases[idx].create_rectangle(
            8, 8, 52, 52, fill=color, outline="#333", width=2, tags="btn"
        )
        self.btn_labels[idx].configure(text=text)

    # ------------------------------------------------------------------ cleanup
    def on_closing(self):
        self.running = False
        if self.ser and self.ser.is_open:
            try:
                self.ser.close()
            except Exception:
                pass
        self.root.destroy()


# ====================================================================== main
if __name__ == "__main__":
    root = tk.Tk()
    app = HalfDuplexGUI(root)
    root.protocol("WM_DELETE_WINDOW", app.on_closing)
    root.mainloop()
