from pathlib import Path
import struct
import zlib

for name, height, pixel, palette in (
    ("snapshot-async.png", 240, 17, bytes(i % 256 for i in range(768))),
    ("snapshot-sync.png", 256, 33, bytes([9]) * 768),
):
    data = Path(name).read_bytes()
    assert data[:8] == b"\x89PNG\r\n\x1a\n"
    offset = 8
    chunks = {}
    while offset < len(data):
        size, = struct.unpack_from(">I", data, offset)
        kind = data[offset + 4:offset + 8]
        body = data[offset + 8:offset + 8 + size]
        crc, = struct.unpack_from(">I", data, offset + 8 + size)
        assert zlib.crc32(kind + body) == crc
        chunks[kind] = body
        offset += size + 12
    assert struct.unpack(">IIBBBBB", chunks[b"IHDR"]) == (256, height, 8, 3, 0, 0, 0)
    assert chunks[b"PLTE"] == palette
    assert zlib.decompress(chunks[b"IDAT"]) == (b"\0" + bytes([pixel]) * 256) * height
    assert chunks[b"IEND"] == b""
print("PNG checks passed: dimensions, palette, frame ownership, lossless pixels and chunk CRCs.")
