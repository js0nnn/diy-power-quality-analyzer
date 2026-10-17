"""Builds docs/hardware-annotated.jpg: numbered callouts over docs/hardware.jpg.
Usage (from repo root): python3 tools/annotate_hardware.py /tmp/annot.html  then
  cp docs/hardware.jpg /tmp/ && google-chrome --headless=new --window-size=1800,1371 --screenshot=/tmp/annot.png file:///tmp/annot.html
  and save /tmp/annot.png as docs/hardware-annotated.jpg (JPEG keeps it around 400 KB).
Edit M below to move or relabel markers (coordinates are in the 1200x1600 photo)."""
import sys
# (n, [(x,y) in 1200x1600 photo space], title, subtitle)
M = [
 (1, [(800, 731)],  "NUCLEO-H755ZI-Q", "dual-core STM32H755 (CM7 + CM4)"),
 (2, [(712, 1109)], "ADE9000 on breakout", "hand-soldered SMD, on zero PCB"),
 (3, [(680, 954), (817, 954)], "Isolators", "SPI + power across the live/safe barrier"),
 (4, [(557, 1046)], "Debug LEDs + reset", "ADE9000 status and IRQ lines"),
 (5, [(640, 1274)], "Current inputs", "Rogowski channels + conditioning"),
 (6, [(971, 1269)], "Voltage inputs", "phase A/B/C + neutral conditioning"),
 (7, [(103, 1520)], "Rogowski coil", "clamp-on current sensor"),
 (8, [(280, 1126)], "20x4 LCD", "live V, I, PF, P readout"),
 (9, [(211, 731)],  "Rotary encoder", "menu navigation"),
 (10, [(1103, 777)], "USB to PC", "UART stream @ 921600 baud"),
]
TOP, PH = 229, 1371          # crop offset and height of the photo region
PW, LW = 1200, 600
W, H = PW + LW, PH
o = [f'<!doctype html><html><head><meta charset="utf-8"><style>body{{margin:0;background:#0d1117}}text{{font-family:"Noto Sans","Liberation Sans",sans-serif}}.m{{font-family:"DejaVu Sans Mono",monospace}}</style></head><body>'
     f'<svg xmlns="http://www.w3.org/2000/svg" width="{W}" height="{H}" viewBox="0 0 {W} {H}">'
     f'<rect width="{W}" height="{H}" fill="#0d1117"/>'
     f'<image href="hardware.jpg" x="0" y="{-TOP}" width="1200" height="1600" preserveAspectRatio="none"/>'
     f'<rect width="{PW}" height="{H}" fill="#000" opacity=".28"/>']
for n, pts, t, s in M:
    for x, y in pts:
        y -= TOP
        o.append(f'<circle cx="{x}" cy="{y}" r="30" fill="none" stroke="#3fb950" stroke-width="3" opacity=".55"/>'
                 f'<circle cx="{x}" cy="{y}" r="19" fill="#238636" stroke="#ffffff" stroke-width="2.5"/>'
                 f'<text x="{x}" y="{y + 7}" text-anchor="middle" fill="#fff" font-size="19" font-weight="700">{n}</text>')
lx = PW + 36
o.append(f'<text class="m" x="{lx}" y="70" fill="#3fb950" font-size="20">$ lsboard</text>'
         f'<text x="{lx}" y="112" fill="#e6edf3" font-size="34" font-weight="700">What\'s on the board</text>')
y = 170
for n, pts, t, s in M:
    o.append(f'<circle cx="{lx + 18}" cy="{y + 2}" r="20" fill="#238636"/>'
             f'<text x="{lx + 18}" y="{y + 9}" text-anchor="middle" fill="#fff" font-size="19" font-weight="700">{n}</text>'
             f'<text x="{lx + 50}" y="{y + 2}" fill="#e6edf3" font-size="27" font-weight="700">{t}</text>'
             f'<text x="{lx + 50}" y="{y + 34}" fill="#9da7b3" font-size="21">{s}</text>')
    y += 114
o.append(f'<line x1="{PW}" y1="0" x2="{PW}" y2="{H}" stroke="#30363d" stroke-width="2"/></svg></body></html>')
open(sys.argv[1], "w").write("".join(o))
print(W, H)
