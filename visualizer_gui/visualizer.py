import sys
import serial
import struct
import numpy as np
import pyqtgraph as pg
from PyQt6 import QtWidgets, QtCore, QtGui

PORT = sys.argv[1] if len(sys.argv) > 1 else "/dev/ttyACM0"  # e.g. COM5 on Windows
BAUD = 921600

WAVE_SAMPLES = 256
HEADER = b'\x55\xAA'
PACKET_SIZE = 2058

ADE9000_PCF_FS_CODES = 74532013

# Voltage scaling
ADE9000_FS_VOLTAGE = 1.0
VOLTAGE_TR = 1001   # (1000000 + 1000)/1000

V_FS = ADE9000_FS_VOLTAGE * VOLTAGE_TR

# Current scaling (Rogowski)
ADC_FS_RMS_VOLTAGE = 0.1765
ROGOWSKI_SENSITIVITY = 194e-6
CURRENT_GAIN = 1.42
I_FS = ADC_FS_RMS_VOLTAGE / ROGOWSKI_SENSITIVITY


def crc16_ccitt(data):
    crc = 0xFFFF
    for b in data:
        crc ^= b << 8
        for _ in range(8):
            if crc & 0x8000:
                crc = (crc << 1) ^ 0x1021
            else:
                crc <<= 1
            crc &= 0xFFFF
    return crc


class SerialWorker(QtCore.QThread):

    waveform_ready = QtCore.pyqtSignal(np.ndarray, np.ndarray)

    def run(self):

        try:
            ser = serial.Serial(PORT, BAUD, timeout=0.1)
        except Exception as e:
            print("Serial open error:", e)
            return

        buffer = bytearray()

        while True:

            data = ser.read(4096)

            if data:
                buffer += data

            while True:

                start = buffer.find(HEADER)

                if start == -1:
                    buffer = buffer[-1:]
                    break

                if len(buffer) < start + PACKET_SIZE:
                    break

                packet = buffer[start:start + PACKET_SIZE]
                buffer = buffer[start + PACKET_SIZE:]

                header, frame_id, count = struct.unpack("<HIH", packet[:8])

                offset = 8

                ia = struct.unpack("<{}i".format(WAVE_SAMPLES),
                                   packet[offset:offset + 4 * WAVE_SAMPLES])
                offset += 4 * WAVE_SAMPLES

                va = struct.unpack("<{}i".format(WAVE_SAMPLES),
                                   packet[offset:offset + 4 * WAVE_SAMPLES])
                offset += 4 * WAVE_SAMPLES

                crc_rx = struct.unpack("<H", packet[offset:offset + 2])[0]
                crc_calc = crc16_ccitt(packet[:-2])

                if crc_calc == crc_rx:

                    ia_np = np.array(ia)
                    va_np = np.array(va)

                    self.waveform_ready.emit(ia_np, va_np)

                else:
                    print("CRC ERROR")


class Scope(QtWidgets.QMainWindow):
    def __init__(self):
        super().__init__()

        self.zoom = 1.0

        self.no_load_samples = []
        self.calibrated = False
        self.current_threshold = 0.3
        self.noise_floor = 0.5   # for harmonics (%)

        self.v_rms_history = []
        self.i_rms_history = []
        self.max_history = 2000   # sliding window

        self.filtered_values = {
            "VRMS": 0,
            "IRMS": 0,
            "PF": 0,
            "Apparent Power": 0,
            "Real Power": 0,
            "Reactive Power": 0,
            "VTHD": 0,
            "ITHD": 0,
            "VPEAK": 0,
            "IPEAK": 0,
        }

        self.alpha = 0.2   # smoothing factor (0.1–0.3 ideal)

        self.paused = False

        self.setWindowTitle("ADE9000 Waveform Viewer (Running)")

        # ---------------- Graph ----------------
        pg.setConfigOption('background', '#0b0b0b')
        pg.setConfigOption('foreground', 'w')

        self.tabs = QtWidgets.QTabWidget()
        self.setCentralWidget(self.tabs)

        # -------- TAB 1: Waveform --------
        self.waveform_tab = QtWidgets.QWidget()
        waveform_layout = QtWidgets.QVBoxLayout()

        self.graph = pg.PlotWidget()
        waveform_layout.addWidget(self.graph)

        self.waveform_tab.setLayout(waveform_layout)
        self.tabs.addTab(self.waveform_tab, "Waveform")

        # -------- TAB 2: Histogram --------
        self.hist_tab = QtWidgets.QWidget()
        hist_layout = QtWidgets.QVBoxLayout()

        # -------- VRMS Histogram --------
        self.v_hist_plot = pg.PlotWidget()
        self.v_hist_plot.setTitle("Voltage RMS Histogram")
        self.v_hist_plot.showGrid(x=True, y=True)
        self.v_hist_plot.setLabel('bottom', 'Voltage (V)')
        self.v_hist_plot.setLabel('left', 'Count')

        # -------- IRMS Histogram --------
        self.i_hist_plot = pg.PlotWidget()
        self.i_hist_plot.setTitle("Current RMS Histogram")
        self.i_hist_plot.showGrid(x=True, y=True)
        self.i_hist_plot.setLabel('bottom', 'Current (A)')
        self.i_hist_plot.setLabel('left', 'Count')

        # -------- TAB 3: Harmonics --------
        self.harm_tab = QtWidgets.QWidget()
        harm_layout = QtWidgets.QVBoxLayout()

        self.harm_plot = pg.PlotWidget()
        self.harm_plot.setTitle("Current Harmonic Spectrum")
        self.harm_plot.showGrid(x=True, y=True)
        self.harm_plot.setLabel('bottom', 'Harmonic Order')
        self.harm_plot.setLabel('left', 'Magnitude (%)')
        self.harm_bar = pg.BarGraphItem(x=[], height=[], width=0.6, brush='m')
        self.harm_plot.addItem(self.harm_bar)

        harm_layout.addWidget(self.harm_plot)
        self.harm_tab.setLayout(harm_layout)

        self.tabs.addTab(self.harm_tab, "Harmonics")

        # Add to layout
        hist_layout.addWidget(self.v_hist_plot)
        hist_layout.addWidget(self.i_hist_plot)

        self.hist_tab.setLayout(hist_layout)
        self.tabs.addTab(self.hist_tab, "I & V Distribution")

        self.graph.showGrid(x=True, y=True)
        self.graph.setMouseEnabled(x=False, y=False)

        # Disable padding 
        self.graph.getViewBox().setDefaultPadding(0)

        self.graph.enableAutoRange(axis=pg.ViewBox.XAxis, enable=False)
        self.graph.enableAutoRange(axis=pg.ViewBox.YAxis, enable=False)

        self.graph.setYRange(-25000000, 25000000)
        self.graph.setXRange(0, 1024, padding=0)

        self.graph.setLabel('left', 'Amplitude', units='')
        self.graph.setLabel('bottom', 'Samples')

        self.graph.getAxis('left').setStyle(tickFont=QtGui.QFont("Arial", 10))
        self.graph.getAxis('bottom').setStyle(tickFont=QtGui.QFont("Arial", 10))

        

        # ---------------- Buffers ----------------
        self.buffer_size = 1024
        self.ia_buffer = np.array([], dtype=np.int32)
        self.va_buffer = np.array([], dtype=np.int32)

        self.legend = self.graph.addLegend()
        self.curve_va = self.graph.plot(pen=pg.mkPen('y', width=2), name="VA")
        self.curve_ia = self.graph.plot(pen=pg.mkPen('c', width=2), name="IA")

        # ---------------- Worker ----------------
        self.worker = SerialWorker()
        self.worker.waveform_ready.connect(self.update_plot)
        self.worker.start()

        # ---------------- Control Variables ----------------
        self.ia_amp = 1.0
        self.va_amp = 1.0
        self.ia_stretch = 1.0
        self.va_stretch = 1.0

        # ---------------- Dock ----------------
        dock = QtWidgets.QDockWidget("Signal Conditioning Panel", self)
        dock.setFloating(False)
        dock.setAllowedAreas(QtCore.Qt.DockWidgetArea.RightDockWidgetArea)

        control_widget = QtWidgets.QWidget()
        main_layout = QtWidgets.QVBoxLayout()

        # ======================
        # Global View (Zoom)
        # ======================
        view_group = QtWidgets.QGroupBox("Global View")
        view_layout = QtWidgets.QVBoxLayout()

        zoom_layout = QtWidgets.QHBoxLayout()

        self.slider_zoom = QtWidgets.QSlider(QtCore.Qt.Orientation.Horizontal)
        self.slider_zoom.setRange(1, 200)
        self.slider_zoom.setValue(100)
        self.slider_zoom.valueChanged.connect(self.update_zoom)

        btn_zoom_reset = QtWidgets.QPushButton("R")
        btn_zoom_reset.setFixedSize(22, 22)
        btn_zoom_reset.clicked.connect(lambda: self.slider_zoom.setValue(100))

        zoom_layout.addWidget(QtWidgets.QLabel("Zoom"))
        zoom_layout.addWidget(self.slider_zoom)
        zoom_layout.addWidget(btn_zoom_reset)

        view_layout.addLayout(zoom_layout)
        view_group.setLayout(view_layout)

        main_layout.addWidget(view_group)

        # ======================
        # Voltage Group (VA)
        # ======================
        va_group = QtWidgets.QGroupBox("Voltage Channel (VA)")
        va_layout = QtWidgets.QVBoxLayout()

        # VA Gain
        va_amp_layout = QtWidgets.QHBoxLayout()

        self.slider_va_amp = QtWidgets.QSlider(QtCore.Qt.Orientation.Horizontal)
        self.slider_va_amp.setRange(1, 100)
        self.slider_va_amp.setValue(50)

        # Set initial value manually
        self.va_amp = 50 / 100

        # Then connect
        self.slider_va_amp.valueChanged.connect(
            lambda v: setattr(self, "va_amp", v / 100)
        )

        btn_va_amp_reset = QtWidgets.QPushButton("R")
        btn_va_amp_reset.setFixedSize(22, 22)
        btn_va_amp_reset.clicked.connect(lambda: self.slider_va_amp.setValue(50))

        va_amp_layout.addWidget(QtWidgets.QLabel("Gain"))
        va_amp_layout.addWidget(self.slider_va_amp)
        va_amp_layout.addWidget(btn_va_amp_reset)

        va_layout.addLayout(va_amp_layout)

        # VA Stretch
        va_stretch_layout = QtWidgets.QHBoxLayout()

        self.slider_va_stretch = QtWidgets.QSlider(QtCore.Qt.Orientation.Horizontal)
        self.slider_va_stretch.setRange(50, 200)
        self.slider_va_stretch.setValue(100)
        self.slider_va_stretch.valueChanged.connect(
            lambda v: setattr(self, "va_stretch", v / 100)
        )

        btn_va_stretch_reset = QtWidgets.QPushButton("R")
        btn_va_stretch_reset.setFixedSize(22, 22)
        btn_va_stretch_reset.clicked.connect(lambda: self.slider_va_stretch.setValue(100))

        va_stretch_layout.addWidget(QtWidgets.QLabel("Time Scale"))
        va_stretch_layout.addWidget(self.slider_va_stretch)
        va_stretch_layout.addWidget(btn_va_stretch_reset)

        va_layout.addLayout(va_stretch_layout)

        va_group.setLayout(va_layout)

        # ======================
        # Separator
        # ======================
        separator = QtWidgets.QFrame()
        separator.setFrameShape(QtWidgets.QFrame.Shape.HLine)
        separator.setFrameShadow(QtWidgets.QFrame.Shadow.Sunken)

        # ======================
        # Current Group (IA)
        # ======================
        ia_group = QtWidgets.QGroupBox("Current Channel (IB)")
        ia_layout = QtWidgets.QVBoxLayout()

        # IA Gain
        ia_amp_layout = QtWidgets.QHBoxLayout()

        self.slider_ia_amp = QtWidgets.QSlider(QtCore.Qt.Orientation.Horizontal)
        self.slider_ia_amp.setRange(1, 5000)
        self.slider_ia_amp.setValue(100)
        self.slider_ia_amp.valueChanged.connect(
            lambda v: setattr(self, "ia_amp", v / 100)
        )

        btn_ia_amp_reset = QtWidgets.QPushButton("R")
        btn_ia_amp_reset.setFixedSize(22, 22)
        btn_ia_amp_reset.clicked.connect(lambda: self.slider_ia_amp.setValue(100))

        ia_amp_layout.addWidget(QtWidgets.QLabel("Gain"))
        ia_amp_layout.addWidget(self.slider_ia_amp)
        ia_amp_layout.addWidget(btn_ia_amp_reset)

        ia_layout.addLayout(ia_amp_layout)

        # IA Stretch
        ia_stretch_layout = QtWidgets.QHBoxLayout()

        self.slider_ia_stretch = QtWidgets.QSlider(QtCore.Qt.Orientation.Horizontal)
        self.slider_ia_stretch.setRange(50, 200)
        self.slider_ia_stretch.setValue(100)
        self.slider_ia_stretch.valueChanged.connect(
            lambda v: setattr(self, "ia_stretch", v / 100)
        )

        btn_ia_stretch_reset = QtWidgets.QPushButton("R")
        btn_ia_stretch_reset.setFixedSize(22, 22)
        btn_ia_stretch_reset.clicked.connect(lambda: self.slider_ia_stretch.setValue(100))

        ia_stretch_layout.addWidget(QtWidgets.QLabel("Time Scale"))
        ia_stretch_layout.addWidget(self.slider_ia_stretch)
        ia_stretch_layout.addWidget(btn_ia_stretch_reset)

        ia_layout.addLayout(ia_stretch_layout)

        ia_group.setLayout(ia_layout)

        # ======================
        # Final Layout
        # ======================
        main_layout.addWidget(va_group)
        main_layout.addWidget(separator)
        main_layout.addWidget(ia_group)
        main_layout.addStretch()

        control_widget.setLayout(main_layout)
        dock.setWidget(control_widget)

        self.addDockWidget(QtCore.Qt.DockWidgetArea.RightDockWidgetArea, dock)

        # ---------------- Bottom Info Bar ----------------
        bottom_dock = QtWidgets.QDockWidget("Electrical Measurements", self)
        bottom_dock.setAllowedAreas(QtCore.Qt.DockWidgetArea.BottomDockWidgetArea)

        bottom_widget = QtWidgets.QWidget()
        bottom_layout = QtWidgets.QGridLayout()
        bottom_layout.setHorizontalSpacing(20)   # space between columns
        bottom_layout.setVerticalSpacing(8)      # space between rows
        bottom_layout.setContentsMargins(10, 5, 10, 5)

        # Labels dictionary
        self.labels = {}

        params = [
            "VRMS", "IRMS", "PF",
            "Apparent Power", "Real Power", "Reactive Power",
            "VTHD", "ITHD", "VPEAK", "IPEAK"
        ]

        for i, name in enumerate(params):
            label_name = QtWidgets.QLabel(f"{name}")
            label_name.setStyleSheet("font-weight: bold;")

            label_val = QtWidgets.QLabel("0")
            label_val.setStyleSheet("""
                background-color: #111;
                color: #00FFAA;
                padding: 4px;
                border: 1px solid #333;
                font-family: Consolas, monospace;
            """)

            label_name.setContentsMargins(0, 0, 5, 0)
            label_val.setContentsMargins(0, 0, 15, 0)
            label_val.setAlignment(QtCore.Qt.AlignmentFlag.AlignLeft | QtCore.Qt.AlignmentFlag.AlignVCenter)

            row = i // 5
            col = (i % 5) * 3   # <-- IMPORTANT (3 columns per group)

            bottom_layout.addWidget(label_name, row, col)
            bottom_layout.addWidget(label_val, row, col + 1)

            self.labels[name] = label_val

        # Tight label column
        for i in range(5):
            base = i * 3
            bottom_layout.setColumnStretch(base, 0)      # label
            bottom_layout.setColumnStretch(base + 1, 0)  # value
            bottom_layout.setColumnStretch(base + 2, 1)  # spacer (EXPANDS)

        bottom_widget.setLayout(bottom_layout)
        bottom_dock.setWidget(bottom_widget)

        self.addDockWidget(QtCore.Qt.DockWidgetArea.BottomDockWidgetArea, bottom_dock)

        dock.setStyleSheet("""
        QDockWidget::title {
            font-size: 16px;
            font-weight: bold;
        }
        """)

        bottom_dock.setStyleSheet("""
        QDockWidget::title {
            font-size: 16px;
            font-weight: bold;
        }
        """)

        app_font = self.font()
        self.setStyleSheet("""

            QWidget {
                font-size: 12pt;
                font-family: "Segoe UI", "Arial";
            }

            QGroupBox {
                font-size: 13pt;
                font-weight: bold;
                border: 1px solid #444;
                margin-top: 10px;
            }

            QGroupBox::title {
                subcontrol-origin: margin;
                subcontrol-position: top left;
                padding: 0 5px;
            }

            QLabel {
                font-size: 12pt;
            }

            QPushButton {
                font-size: 11pt;
                padding: 2px;
            }

            QSlider {
                height: 18px;
            }

            QDockWidget::title {
                font-size: 15pt;
                font-weight: bold;
                text-align: center;
            }

        """)

        
        
        self.setFont(app_font)

        self.last_update_time = 0
        self.update_interval_ms = 500


        # ---------------- Thresholds ----------------
        self.thresholds = {
            "VRMS": {"low": 200, "high": 250},
            "IRMS": {"low": 0, "high": 150},
            "VPEAK": {"low": 280, "high": 350},
            "IPEAK": {"low": 0, "high": 200},
        }

    def update_zoom(self, value):
        self.zoom = 2 ** ((value - 100) / 50)

        base_range = 25000000
        scaled = base_range / self.zoom

        self.graph.setYRange(-scaled, scaled, padding=0)

    def compute_harmonics(self, signal, fs=8000, fundamental=50):

        N = len(signal)

        # Remove DC
        signal = signal - np.mean(signal)

        # Window
        window = np.hanning(N)
        signal = signal * window

        fft = np.fft.rfft(signal)
        mag = np.abs(fft) / (N / 2)

        freqs = np.fft.rfftfreq(N, d=1/fs)

        # ---- FUNDAMENTAL (band energy) ----
        fund_idx = np.argmin(np.abs(freqs - fundamental))

        band = 2
        start = max(fund_idx - band, 1)
        end = min(fund_idx + band + 1, len(mag))

        fund_mag = np.sqrt(np.sum(mag[start:end]**2))

        if fund_mag == 0:
            return [], []

        harmonics = []
        orders = []

        max_order = 15

        for h in range(1, max_order + 1):

            target_freq = h * fundamental
            idx = np.argmin(np.abs(freqs - target_freq))

            start = max(idx - band, 1)
            end = min(idx + band + 1, len(mag))

            value = np.sqrt(np.sum(mag[start:end]**2))

            percent = (value / fund_mag) * 100

            harmonics.append(percent)
            orders.append(h)

        return orders, harmonics

    def update_label_color(self, key, value):

        if key not in self.thresholds:
            return

        low = self.thresholds[key]["low"]
        high = self.thresholds[key]["high"]

        label = self.labels[key]

        if value < low:
            color = "#00BFFF"   # Blue → Underrange
        elif value > high:
            color = "#FF3B3B"   # Red → Overrange
            label.setStyleSheet("background-color: #330000; color: #FF3B3B; ...")
        else:
            color = "#00FFAA"   # Green → Normal

        label.setStyleSheet(f"""
            background-color: #111;
            color: {color};
            padding: 4px;
            border: 1px solid #333;
            font-family: Consolas, monospace;
        """)

    def compute_parameters(self, v, i):

        # Convert to float
        v = v.astype(np.float64)
        i = i.astype(np.float64)

        # -------- SCALE FROM PCF --------
        v = (v / ADE9000_PCF_FS_CODES) * V_FS
        i = (i / ADE9000_PCF_FS_CODES) * I_FS * CURRENT_GAIN

        # Remove DC (still useful)
        v = v - np.mean(v)
        i = i - np.mean(i)

        # RMS
        vrms = np.sqrt(np.mean(v**2))
        irms = np.sqrt(np.mean(i**2))

        # Instantaneous power
        p_inst = v * i

        # Real power
        p_real = np.mean(p_inst)

        # Apparent power
        p_app = vrms * irms

        # Power factor
        pf = p_real / p_app if p_app != 0 else 0

        # Reactive power
        p_reactive = np.sqrt(max(p_app**2 - p_real**2, 0))

        # Peak values
        vpeak = np.max(np.abs(v))
        ipeak = np.max(np.abs(i))

        # -------- THD --------
        def compute_thd(signal):

            N = len(signal)

            # Windowing
            window = np.hanning(N)
            signal = signal * window

            # FFT
            fft = np.fft.rfft(signal)
            mag = np.abs(fft) / (N / 2)

            # Remove DC
            mag[0] = 0

            # Find peak (fundamental region)
            fund_idx = np.argmax(mag)

            if fund_idx == 0:
                return 0

            # ---- KEY FIX: capture energy around fundamental ----
            band = 2   # +/- bins around peak (tune 1–3)

            start = max(fund_idx - band, 1)
            end = min(fund_idx + band + 1, len(mag))

            fundamental = np.sqrt(np.sum(mag[start:end]**2))

            if fundamental == 0:
                return 0

            # Zero out fundamental band before harmonic calc
            mag[start:end] = 0

            # Harmonics = everything else
            harmonics = np.sqrt(np.sum(mag**2))

            return harmonics / fundamental

        vthd = compute_thd(v)
        ithd = compute_thd(i)

        return {
            "VRMS": vrms,
            "IRMS": irms,
            "PF": pf,
            "Apparent Power": p_app,
            "Real Power": p_real,
            "Reactive Power": p_reactive,
            "VTHD": vthd,
            "ITHD": ithd,
            "VPEAK": vpeak,
            "IPEAK": ipeak,
        }

    def keyPressEvent(self, event):
        if event.key() == QtCore.Qt.Key.Key_Space:
            self.paused = not self.paused

            if self.paused:
                self.setWindowTitle("ADE9000 Waveform Viewer (Paused)")
                print("Paused")
            else:
                self.setWindowTitle("ADE9000 Waveform Viewer (Running)")
                print("Resumed")

    def update_plot(self, ia, va):

        if self.paused:
            return

        # 1. Append incoming data
        self.ia_buffer = np.concatenate([self.ia_buffer, ia])
        self.va_buffer = np.concatenate([self.va_buffer, va])

        if len(self.ia_buffer) >= self.buffer_size:

            # ---- COMPUTE PARAMETERS (RAW SIGNAL ONLY) ----
            current_time = QtCore.QDateTime.currentMSecsSinceEpoch()

            if current_time - self.last_update_time >= self.update_interval_ms:
                params = self.compute_parameters(self.va_buffer, self.ia_buffer)

                # ---- UPDATE UI ----
                units = {
                    "VRMS": "V",
                    "IRMS": "A",
                    "PF": "",
                    "Apparent Power": "VA",
                    "Real Power": "W",
                    "Reactive Power": "VAR",
                    "VTHD": "%",
                    "ITHD": "%",
                    "VPEAK": "V",
                    "IPEAK": "A",
                }

                for key, val in params.items():
                    unit = units.get(key, "")

                    if key in ["PF"]:
                        text = f"{val:.4f}"
                    elif key in ["VTHD", "ITHD"]:
                        text = f"{val*100:.2f} %"
                    else:
                        text = f"{val:.2f} {unit}"

                    self.labels[key].setText(text)

                    # Apply threshold coloring
                    if key in ["VRMS", "IRMS", "VPEAK", "IPEAK"]:
                        self.update_label_color(key, val)

                self.last_update_time = current_time


                vrms = params["VRMS"]
                irms = params["IRMS"]

                # -------- CURRENT THRESHOLD FILTER --------
                if irms < self.current_threshold:
                    irms = 0
                    params["IRMS"] = 0
                    params["ITHD"] = 0

                # Store voltage
                self.v_rms_history.append(vrms)
                if len(self.v_rms_history) > self.max_history:
                    self.v_rms_history.pop(0)

                # Store current
                if irms > self.current_threshold:
                    self.i_rms_history.append(irms)
                if len(self.i_rms_history) > self.max_history:
                    self.i_rms_history.pop(0)

                # harmonics
                # if irms > self.current_threshold:
                    # orders, mags = self.compute_harmonics(self.ia_buffer, fs=8000)

                    # # Apply noise floor
                    # mags = [m if m > self.noise_floor else 0 for m in mags]

                    # self.harm_bar.setOpts(
                    #     x=orders,
                    #     height=mags,
                    #     width=0.6
                    # )
                # else:
                #     # No load → flat zero spectrum
                #     self.harm_bar.setOpts(
                #         x=list(range(1, 16)),
                #         height=[0]*15,
                #         width=0.6
                #     )
                
                orders, mags = self.compute_harmonics(self.ia_buffer, fs=8000)
                # Apply noise floor
                mags = [m if m > self.noise_floor else 0 for m in mags]

                self.harm_bar.setOpts(
                    x=orders,
                    height=mags,
                    width=0.6
                )


                # Collect baseline for first few seconds
                if not self.calibrated:
                    self.no_load_samples.append(irms)

                    if len(self.no_load_samples) > 50:
                        noise_mean = np.mean(self.no_load_samples)
                        self.current_threshold = noise_mean + 0.1   # margin
                        self.calibrated = True
                        print("Auto threshold set to:", self.current_threshold)

            # ---- DISPLAY SCALING ONLY ----
            ia_scaled = self.ia_buffer * self.ia_amp
            va_scaled = self.va_buffer * self.va_amp


            # -------- DRIFT REMOVAL (REQUIRED) --------
            # Remove DC offset (main fix)
            # ia_scaled = ia_scaled - np.mean(ia_scaled)

            # -------- OPTIONAL: HIGH-PASS FILTER (better stability) --------
            alpha = 0.995  # 0.98–0.999 (tune if needed)

            def highpass(signal):
                y = np.zeros_like(signal, dtype=float)
                for i in range(1, len(signal)):
                    y[i] = alpha * (y[i-1] + signal[i] - signal[i-1])
                return y

            # ia_scaled = highpass(ia_scaled)

            # Apply stretch (resampling)
            def stretch_signal(signal, factor):
                x_old = np.linspace(0, 1, len(signal))
                x_new = np.linspace(0, 1, int(len(signal) * factor))
                return np.interp(x_new, x_old, signal)

            ia_final = stretch_signal(ia_scaled, self.ia_stretch)
            va_final = stretch_signal(va_scaled, self.va_stretch)

            # X axis
            x_ia = np.arange(len(ia_final))
            x_va = np.arange(len(va_final))

            if self.curve_ia.isVisible():
                self.curve_ia.setData(x_ia, ia_final)

            if self.curve_va.isVisible():
                self.curve_va.setData(x_va, va_final)

            # Clear buffers
            self.ia_buffer = np.array([], dtype=np.int32)
            self.va_buffer = np.array([], dtype=np.int32)

        if len(self.v_rms_history) > 50:
            # -------- VRMS Histogram --------
            v_data = np.array(self.v_rms_history)
            vy, vx = np.histogram(v_data, bins=30)

            self.v_hist_plot.clear()
            v_bg = pg.BarGraphItem(
                x=vx[:-1],
                height=vy,
                width=(vx[1] - vx[0]),
                brush='y'
            )
            self.v_hist_plot.addItem(v_bg)
            self.v_hist_plot.enableAutoRange()


        if len(self.i_rms_history) > 50:
            # -------- IRMS Histogram --------
            i_data = np.array(self.i_rms_history)
            iy, ix = np.histogram(i_data, bins=30)

            self.i_hist_plot.clear()
            i_bg = pg.BarGraphItem(
                x=ix[:-1],
                height=iy,
                width=(ix[1] - ix[0]),
                brush='c'
            )
            self.i_hist_plot.addItem(i_bg)
            self.i_hist_plot.enableAutoRange()


def main():

    app = QtWidgets.QApplication(sys.argv)

    win = Scope()
    win.showMaximized()

    sys.exit(app.exec())


if __name__ == "__main__":
    main()