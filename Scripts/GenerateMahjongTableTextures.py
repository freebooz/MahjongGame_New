"""Generate a self-contained PBR texture set for the standard Mahjong table.

The output uses the Unreal-friendly metallic/roughness workflow:

* BaseColor: sRGB RGB texture
* Normal: tangent-space DirectX normal texture
* ORM: R=ambient occlusion, G=roughness, B=metallic

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
    MaterialSpec("M_Table_Felt_Green_PBR", "Felt", (8, 98, 43), 0.82, 0.0, "felt", 1.25),
    MaterialSpec("M_Table_BlackPowderMetal_PBR", "BlackPowderMetal", (12, 15, 14), 0.34, 0.78, "powder", 0.52),
    MaterialSpec("M_Table_PianoBlackMetal_PBR", "PianoBlackMetal", (5, 7, 7), 0.18, 0.50, "piano", 0.18),
    MaterialSpec("M_Table_SmokedOak_PBR", "SmokedOak", (66, 39, 23), 0.39, 0.0, "wood", 1.05),
    MaterialSpec("M_Table_PerforatedGrille_PBR", "PerforatedGrille", (44, 48, 46), 0.43, 0.38, "grille", 1.20),
    MaterialSpec("M_Table_Rubber_PBR", "Rubber", (8, 10, 9), 0.80, 0.0, "rubber", 0.65),
    MaterialSpec("M_Table_Chrome_PBR", "Chrome", (166, 174, 176), 0.13, 1.0, "brushed", 0.25),
    MaterialSpec("M_Table_DeckComposite_PBR", "DeckComposite", (14, 24, 19), 0.64, 0.0, "composite", 0.45),
    MaterialSpec("M_Table_IndicatorRed_PBR", "IndicatorRed", (124, 3, 4), 0.17, 0.0, "lens", 0.12),
    MaterialSpec("M_Table_IndicatorGreen_PBR", "IndicatorGreen", (2, 112, 15), 0.17, 0.0, "lens", 0.12),
    MaterialSpec("M_Table_ControllerFace_PBR", "ControllerFace", (90, 96, 94), 0.30, 0.32, "brushed", 0.36),
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
        weave = math.sin(x * math.pi * 0.53) * math.sin(y * math.pi * 0.49)
        return 0.46 * weave + 0.30 * (n - 0.5)
    if spec.pattern == "wood":
        grain = math.sin((u * 34.0 + math.sin(v * 7.0) * 0.65) * math.tau)
        pores = math.sin((u * 119.0 + v * 2.5) * math.tau) * 0.20
        return grain * 0.48 + pores + (n - 0.5) * 0.10
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
        return h * 0.085
    if spec.pattern == "wood":
        return h * 0.26
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


def orm_pixel(spec: MaterialSpec) -> PixelFunction:
    def pixel(x: int, y: int, size: int) -> tuple[int, int, int]:
        h = height_value(spec, x, y, size)
        ao = 0.52 if spec.pattern == "grille" and h < -0.5 else 0.94 + min(0.04, max(-0.08, h * 0.025))
        rough = spec.roughness + h * (0.055 if spec.pattern in {"felt", "wood", "powder"} else 0.025)
        return (
            clamp_byte(ao * 255.0),
            clamp_byte(max(0.03, min(1.0, rough)) * 255.0),
            clamp_byte(spec.metallic * 255.0),
        )

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
    args = parser.parse_args()
    if args.size < 64 or args.size > 4096 or args.size & (args.size - 1):
        raise SystemExit("--size must be a power of two between 64 and 4096")

    project_root = Path(__file__).resolve().parents[1]
    output_dir = args.output_dir or project_root / "SourceArt" / "3D" / "MahjongTable" / "Textures"
    output_dir.mkdir(parents=True, exist_ok=True)
    generated: list[dict[str, object]] = []

    for spec in MATERIALS:
        channels = {
            "BaseColor": base_color_pixel(spec),
            "Normal": normal_pixel(spec),
            "ORM": orm_pixel(spec),
        }
        texture_files: dict[str, str] = {}
        for channel, pixel in channels.items():
            path = output_dir / f"T_Table_{spec.stem}_{channel}.png"
            write_rgb_png(path, args.size, pixel)
            texture_files[channel] = path.name
        generated.append(
            {
                "material_slot": spec.slot,
                "textures": texture_files,
                "roughness": spec.roughness,
                "metallic": spec.metallic,
            }
        )

    files = sorted(output_dir.glob("T_Table_*.png"))
    manifest = {
        "workflow": "Unreal metallic-roughness PBR",
        "resolution": [args.size, args.size],
        "packing": {"ORM.R": "ambient_occlusion", "ORM.G": "roughness", "ORM.B": "metallic"},
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
