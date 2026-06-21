
from PIL import Image
import os
import struct

src = r'D:\op\未命名.png'
dst_ico = r'D:\op\OptiScalerAssistant\assets\app.ico'

img = Image.open(src).convert('RGBA')

sizes = [16, 32, 48, 64, 128, 256]
png_datas = []
for s in sizes:
    icon = img.resize((s, s), Image.LANCZOS)
    import io
    buf = io.BytesIO()
    icon.save(buf, format='PNG')
    png_datas.append((s, buf.getvalue()))

header = struct.pack('<HHH', 0, 1, len(sizes))

dir_data = b''
img_data = b''
offsets = []

dir_end = 6 + 16 * len(sizes)
current = dir_end
for s, png in png_datas:
    offsets.append(current)

    w = 0 if s == 256 else s
    h = 0 if s == 256 else s
    dir_data += struct.pack('<BBBBHHII', w, h, 0, 0, 1, 32, len(png), current)
    img_data += png
    current += len(png)

ico_bytes = header + dir_data + img_data
with open(dst_ico, 'wb') as f:
    f.write(ico_bytes)

print(f"ICO: {len(ico_bytes)} bytes, frames: {len(sizes)}")
print(f"Sizes: {sizes}")
