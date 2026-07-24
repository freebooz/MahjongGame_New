"""Generate the mobile PBR texture set for the tabletop-only Mahjong asset.

The output uses the Unreal-friendly metallic/roughness workflow:

* BaseColor: sRGB RGB texture
* Normal: tangent-space DirectX normal texture
* Roughness: linear grayscale texture
* AO: linear grayscale texture

Only Python's standard library is required so the texture build is reproducible on
machines that do not have Pillow installed.
"""

from __future__ import annotations

import argparse
import hashlib
import json
import math
import struct
import zlib
from dataclasses import dataclass
from pathlib import Path
from typing import Callable


PixelFunction = Callable[[int, int, int], tuple[int, int, int]]


@dataclass(frozen=True)
class MaterialSpec:
    slot: str
    stem: str
    base_color: tuple[int, int, int]
    roughness: float
    metallic: float
    pattern: str
    normal_strength: float = 1.0


MATERIALS = (
    MaterialSpec("M_Table_Walnut_PBR", "Wood", (94, 36, 12), 0.24, 0.0, "wood", 0.95),
    MaterialSpec("M_Table_Felt_Green_PBR", "Felt", (13, 74, 28), 0.86, 0.0, "felt", 1.02),
)


def clamp_byte(value: float) -> int:
    return max(0, min(255, round(value)))


def noise(x: int, y: int, seed: int = 0) -> float:
    value = (x * 374761393 + y * 668265263 + seed * 1442695041) & 0xFFFFFFFF
    value = ((value ^ (value >> 13)) * 1274126177) & 0xFFFFFFFF
    value ^= value >> 16
    return (value & 0xFFFF) / 65535.0


def height_value(spec: MaterialSpec, x: int, y: int, size: int) -> float:
    u = x / size
    v = y / size
    n = noise(x, y, len(spec.stem))
    if spec.pattern == "felt":
        # Interlaced warp/weft threads plus directional nap at three scales.
        warp = math.sin(x * math.tau / 3.7)
        weft = math.sin(y * math.tau / 4.3)
        interlace = warp * weft
        diagonal_fiber = math.sin((x + y * 1.17) * math.tau / 17.0)
        nap = math.sin((x * 0.23 + y) * math.tau / 61.0)
        coarse = noise(x // 7, y // 7, 37) - 0.5
        return (
            0.25 * warp
            + 0.22 * weft
            + 0.19 * interlace
            + 0.09 * diagonal_fiber
            + 0.07 * nap
            + 0.15 * (n - 0.5)
            + 0.10 * coarse
        )
    if spec.pattern == "wood":
        flowing = (
            u * 27.0
            + math.sin(v * math.tau * 4.6) * 0.82
            + math.sin((v * 11.0 + u * 2.7) * math.tau) * 0.22
        )
        grain = math.sin(flowing * math.tau)
        cathedral = math.sin(
            (u * 8.5 + math.sin(v * math.tau * 2.1) * 1.25) * math.tau
        )
        pores = math.sin((u * 123.0 + v * 3.2) * math.tau) * 0.16
        return grain * 0.39 + cathedral * 0.20 + pores + (n - 0.5) * 0.09
    if spec.pattern == "grille":
        cell = max(12, size // 24)
        px = (x % cell) / cell - 0.5
        py = (y % cell) / cell - 0.5
        hole = 1.0 if px * px + py * py < 0.075 else 0.0
        return -0.85 * hole + (n - 0.5) * 0.06
    if spec.pattern == "brushed":
        return math.sin(y * math.tau / 5.0) * 0.12 + (n - 0.5) * 0.08
    if spec.pattern == "powder":
        return (n - 0.5) * 0.55
    if spec.pattern == "rubber":
        bumps = math.sin(x * math.tau / 9.0) * math.sin(y * math.tau / 9.0)
        return bumps * 0.22 + (n - 0.5) * 0.16
    if spec.pattern == "composite":
        return math.sin((u * 8.0 + v * 5.0) * math.tau) * 0.08 + (n - 0.5) * 0.12
    if spec.pattern == "lens":
        return math.sin(u * math.tau) * math.sin(v * math.tau) * 0.025
    return (n - 0.5) * 0.04


def color_variation(spec: MaterialSpec, x: int, y: int, size: int) -> float:
    h = height_value(spec, x, y, size)
    if spec.pattern == "felt":
        directional_nap = math.sin((x * 0.18 + y) * math.tau / 113.0)
        return h * 0.045 + directional_nap * 0.010
    if spec.pattern == "wood":
        return h * 0.50
    if spec.pattern == "grille":
        return -0.58 if h < -0.5 else h * 0.12
    if spec.pattern == "powder":
        return h * 0.055
    if spec.pattern == "rubber":
        return h * 0.04
    if spec.pattern == "brushed":
        return h * 0.035
    return h * 0.018


def base_color_pixel(spec: MaterialSpec) -> PixelFunction:
    def pixel(x: int, y: int, size: int) -> tuple[int, int, int]:
        variation = color_variation(spec, x, y, size)
        multiplier = max(0.12, 1.0 + variation)
        return tuple(clamp_byte(channel * multiplier) for channel in spec.base_color)

    return pixel


def normal_pixel(spec: MaterialSpec) -> PixelFunction:
    def pixel(x: int, y: int, size: int) -> tuple[int, int, int]:
        left = height_value(spec, (x - 1) % size, y, size)
        right = height_value(spec, (x + 1) % size, y, size)
        down = height_value(spec, x, (y - 1) % size, size)
        up = height_value(spec, x, (y + 1) % size, size)
        nx = (left - right) * spec.normal_strength
        # DirectX tangent space uses a downward/negative green channel.
        ny = (up - down) * spec.normal_strength
        nz = 1.0
        length = math.sqrt(nx * nx + ny * ny + nz * nz)
        return (
            clamp_byte((nx / length * 0.5 + 0.5) * 255.0),
            clamp_byte((ny / length * 0.5 + 0.5) * 255.0),
            clamp_byte((nz / length * 0.5 + 0.5) * 255.0),
        )

    return pixel


def roughness_pixel(spec: MaterialSpec) -> PixelFunction:
    def pixel(x: int, y: int, size: int) -> tuple[int, int, int]:
        h = height_value(spec, x, y, size)
        rough = spec.roughness + h * (0.055 if spec.pattern in {"felt", "wood", "powder"} else 0.025)
        value = clamp_byte(max(0.03, min(1.0, rough)) * 255.0)
        return (value, value, value)

    return pixel


def ao_pixel(spec: MaterialSpec) -> PixelFunction:
    def pixel(x: int, y: int, size: int) -> tuple[int, int, int]:
        h = height_value(spec, x, y, size)
        ao = 0.94 + min(0.04, max(-0.08, h * 0.035))
        value = clamp_byte(ao * 255.0)
        return (value, value, value)

    return pixel


def png_chunk(kind: bytes, payload: bytes) -> bytes:
    return struct.pack(">I", len(payload)) + kind + payload + struct.pack(">I", zlib.crc32(kind + payload) & 0xFFFFFFFF)


def write_rgb_png(path: Path, size: int, pixel: PixelFunction) -> None:
    scanlines = bytearray()
    for y in range(size):
        scanlines.append(0)
        for x in range(size):
            scanlines.extend(pixel(x, y, size))
    header = struct.pack(">IIBBBBB", size, size, 8, 2, 0, 0, 0)
    data = b"\x89PNG\r\n\x1a\n"
    data += png_chunk(b"IHDR", header)
    data += png_chunk(b"IDAT", zlib.compress(bytes(scanlines), level=9))
    data += png_chunk(b"IEND", b"")
    path.write_bytes(data)


def sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(block)
    return digest.hexdigest()


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--size", type=int, default=512, help="Square texture resolution")
    parser.add_argument("--output-dir", type=Path)
    parser.add_argument(
        "--only",
        choices=("All", "Wood", "Felt"),
        default="All",
        help="Regenerate one material while preserving and re-manifesting the complete set",
    )
    args = parser.parse_args()
    if args.size < 64 or args.size > 4096 or args.size & (args.size - 1):
        raise SystemExit("--size must be a power of two between 64 and 4096")

    project_root = Path(__file__).resolve().parents[1]
    output_dir = args.output_dir or project_root / "SourceArt" / "3D" / "MahjongTable" / "Textures"
    output_dir.mkdir(parents=True, exist_ok=True)
    selected_materials = (
        MATERIALS if args.only == "All" else tuple(spec for spec in MATERIALS if spec.stem == args.only)
    )
    for spec in selected_materials:
        channels = {
            "BaseColor": base_color_pixel(spec),
            "Normal": normal_pixel(spec),
            "Roughness": roughness_pixel(spec),
            "AO": ao_pixel(spec),
        }
        for channel, pixel in channels.items():
            path = output_dir / f"T_{spec.stem}_{channel}_2K.png"
            write_rgb_png(path, args.size, pixel)
    generated = []
    for spec in MATERIALS:
        generated.append(
            {
                "material_slot": spec.slot,
                "textures": {
                    channel: f"T_{spec.stem}_{channel}_2K.png"
                    for channel in ("BaseColor", "Normal", "Roughness", "AO")
                },
                "roughness": spec.roughness,
                "metallic": spec.metallic,
            }
        )

    files = sorted(output_dir.glob("T_*_2K.png"))
    manifest = {
        "workflow": "Unreal metallic-roughness PBR",
        "resolution": [args.size, args.size],
        "resolution_label": "2K",
        "channels": ["BaseColor", "Normal", "Roughness", "AO"],
        "materials": generated,
        "files": [{"name": path.name, "bytes": path.stat().st_size, "sha256": sha256(path)} for path in files],
    }
    manifest_path = output_dir / "MahjongTableTextureManifest.json"
    manifest_path.write_text(json.dumps(manifest, ensure_ascii=False, indent=2), encoding="utf-8")
    print(f"PBR_TEXTURE_SET={output_dir}")
    print(f"MATERIAL_COUNT={len(MATERIALS)}")
    print(f"TEXTURE_COUNT={len(files)}")
    print(f"TEXTURE_RESOLUTION={args.size}x{args.size}")


if __name__ == "__main__":
    main()
