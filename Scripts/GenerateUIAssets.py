from __future__ import annotations

import csv
import hashlib
import json
from pathlib import Path

from PIL import Image, ImageDraw, ImageFilter, ImageFont


ROOT = Path(__file__).resolve().parents[1]
SOURCE = ROOT / "SourceArt" / "UI"
BG_DIR = SOURCE / "Backgrounds"
PANEL_DIR = SOURCE / "Panels"
BUTTON_DIR = SOURCE / "Buttons"
CONTROL_DIR = SOURCE / "Controls"
AVATAR_DIR = SOURCE / "Avatars"
ICON_DIR = SOURCE / "Icons"
TILE_DIR = SOURCE / "Tiles"
DATA_DIR = SOURCE / "Data"

COLORS = {
    "PrimaryGreen": "#176B5B",
    "DeepTableGreen": "#073F36",
    "JadeGreen": "#42A58C",
    "WarmGold": "#D9A441",
    "DarkGold": "#8C6422",
    "CreamWhite": "#F4EEDC",
    "InkBlack": "#18201F",
    "WarningRed": "#B8463A",
    "InfoBlue": "#397DA5",
    "DisabledGray": "#6A706D",
}


def rgba(value: str, alpha: int = 255) -> tuple[int, int, int, int]:
    value = value.lstrip("#")
    return tuple(int(value[i:i + 2], 16) for i in (0, 2, 4)) + (alpha,)


def font(size: int, bold: bool = False) -> ImageFont.FreeTypeFont:
    candidates = [
        Path("C:/Windows/Fonts/msyhbd.ttc" if bold else "C:/Windows/Fonts/msyh.ttc"),
        Path("C:/Windows/Fonts/simhei.ttf"),
        Path("C:/Windows/Fonts/arial.ttf"),
    ]
    for path in candidates:
        if path.exists():
            return ImageFont.truetype(str(path), size)
    return ImageFont.load_default()


def ensure_dirs() -> None:
    for path in (BG_DIR, PANEL_DIR, BUTTON_DIR, CONTROL_DIR, AVATAR_DIR, ICON_DIR, TILE_DIR, DATA_DIR):
        path.mkdir(parents=True, exist_ok=True)


def save(image: Image.Image, path: Path) -> None:
    image.save(path, "PNG", optimize=True)


def inset_pattern(draw: ImageDraw.ImageDraw, size: tuple[int, int], color: tuple[int, int, int, int]) -> None:
    w, h = size
    step = 24
    for x in range(20, w - 20, step):
        draw.line([(x, 12), (x + 8, 20), (x + 16, 12)], fill=color, width=2)
        draw.line([(x, h - 12), (x + 8, h - 20), (x + 16, h - 12)], fill=color, width=2)


def panel_asset(name: str, fill: str, edge: str, radius: int, margin: int, size: tuple[int, int] = (256, 256)) -> dict:
    image = Image.new("RGBA", size, (0, 0, 0, 0))
    draw = ImageDraw.Draw(image)
    w, h = size
    shadow = Image.new("RGBA", size, (0, 0, 0, 0))
    sd = ImageDraw.Draw(shadow)
    sd.rounded_rectangle((14, 18, w - 10, h - 8), radius=radius, fill=(0, 0, 0, 100))
    shadow = shadow.filter(ImageFilter.GaussianBlur(7))
    image.alpha_composite(shadow)
    draw.rounded_rectangle((10, 10, w - 14, h - 14), radius=radius, fill=rgba(fill, 244), outline=rgba(edge), width=4)
    draw.rounded_rectangle((17, 17, w - 21, h - 21), radius=max(4, radius - 7), outline=rgba(edge, 110), width=2)
    inset_pattern(draw, size, rgba(edge, 72))
    save(image, PANEL_DIR / f"{name}.png")
    return {"name": name, "size": list(size), "margin_px": margin, "margin_normalized": round(margin / size[0], 4), "draw_as": "Box"}


def generate_panels() -> list[dict]:
    specs = [
        ("T_Panel_Main_GreenGold_9Slice", "#0B5649", "#D9A441", 28, 40),
        ("T_Panel_Dialog_CreamGold_9Slice", "#F4EEDC", "#D9A441", 32, 44),
        ("T_Panel_Dialog_DarkGreen_9Slice", "#073F36", "#D9A441", 32, 44),
        ("T_Panel_PlayerInfo_9Slice", "#145B50", "#42A58C", 24, 36),
        ("T_Panel_RoomRule_9Slice", "#F4EEDC", "#8C6422", 20, 34),
        ("T_Panel_Toast_9Slice", "#18201F", "#D9A441", 18, 30),
        ("T_Panel_ScoreRow_9Slice", "#176B5B", "#D9A441", 16, 28),
        ("T_Panel_InputBox_9Slice", "#F4EEDC", "#42A58C", 16, 28),
        ("T_Panel_Tab_9Slice", "#176B5B", "#D9A441", 16, 28),
        ("T_Panel_Notice_9Slice", "#133E4B", "#397DA5", 18, 30),
        ("T_Panel_NetworkStatus_9Slice", "#18201F", "#42A58C", 14, 26),
    ]
    return [panel_asset(*spec) for spec in specs]


BUTTON_COLORS = {
    "PrimaryGold": ("#D9A441", "#8C6422"),
    "PrimaryGreen": ("#176B5B", "#42A58C"),
    "SecondaryBlue": ("#397DA5", "#79B6D6"),
    "DangerRed": ("#B8463A", "#E47A6E"),
    "NeutralDark": ("#18201F", "#6A706D"),
    "TransparentIcon": ("#176B5B", "#D9A441"),
    "RoundIcon": ("#176B5B", "#D9A441"),
    "MahjongAction": ("#176B5B", "#D9A441"),
}


def button_image(base: str, edge: str, state: str, square: bool = False) -> Image.Image:
    size = (192, 192) if square else (320, 112)
    image = Image.new("RGBA", size, (0, 0, 0, 0))
    draw = ImageDraw.Draw(image)
    w, h = size
    fill = rgba(base)
    outline = rgba(edge)
    offset = 5 if state == "Pressed" else 0
    if state == "Hovered":
        fill = tuple(min(255, c + 18) for c in fill[:3]) + (255,)
    if state == "Disabled":
        fill = rgba(COLORS["DisabledGray"], 185)
        outline = (120, 126, 123, 210)
    radius = 42 if square else 28
    box = (10, 10 + offset, w - 10, h - 14 + offset)
    draw.rounded_rectangle(box, radius=radius, fill=fill, outline=outline, width=5)
    draw.rounded_rectangle((18, 18 + offset, w - 18, h - 22 + offset), radius=max(8, radius - 8), outline=(255, 255, 255, 50), width=2)
    if state == "Hovered":
        draw.line((36, 25, w - 36, 25), fill=(255, 244, 208, 145), width=3)
    return image


def generate_buttons() -> list[dict]:
    inventory = []
    for kind, (base, edge) in BUTTON_COLORS.items():
        for state in ("Normal", "Hovered", "Pressed", "Disabled"):
            name = f"T_Btn_{kind}_{state}"
            image = button_image(base, edge, state, square=kind in {"RoundIcon", "MahjongAction"})
            save(image, BUTTON_DIR / f"{name}.png")
            inventory.append({"name": name, "size": list(image.size), "margin_px": 28, "draw_as": "Box"})
    priorities = {"Peng": ("#176B5B", "#42A58C"), "Gang": ("#397DA5", "#D9A441"), "Hu": ("#B8463A", "#F4EEDC"), "Pass": ("#18201F", "#6A706D"), "PlayTile": ("#D9A441", "#8C6422")}
    for action, (base, edge) in priorities.items():
        for state in ("Normal", "Hovered", "Pressed", "Disabled"):
            name = f"T_Btn_{action}_{state}"
            image = button_image(base, edge, state, square=True)
            save(image, BUTTON_DIR / f"{name}.png")
            inventory.append({"name": name, "size": list(image.size), "margin_px": 30, "draw_as": "Box"})
    return inventory


def simple_control(name: str, size: tuple[int, int], painter) -> None:
    image = Image.new("RGBA", size, (0, 0, 0, 0))
    painter(ImageDraw.Draw(image), size)
    save(image, CONTROL_DIR / f"{name}.png")


def generate_controls() -> list[str]:
    for state, edge in (("Normal", "#8C6422"), ("Focused", "#42A58C"), ("Disabled", "#6A706D")):
        panel_asset(f"T_Input_{state}_9Slice", "#F4EEDC", edge, 16, 28, (256, 128))
        (PANEL_DIR / f"T_Input_{state}_9Slice.png").replace(CONTROL_DIR / f"T_Input_{state}_9Slice.png")
    simple_control("T_Checkbox_Unchecked", (64, 64), lambda d, s: d.rounded_rectangle((8, 8, 56, 56), 10, fill=rgba("#F4EEDC"), outline=rgba("#8C6422"), width=4))
    def checked(d, s):
        d.rounded_rectangle((8, 8, 56, 56), 10, fill=rgba("#176B5B"), outline=rgba("#D9A441"), width=4)
        d.line((18, 33, 29, 44, 48, 19), fill=rgba("#F4EEDC"), width=7, joint="curve")
    simple_control("T_Checkbox_Checked", (64, 64), checked)
    simple_control("T_Checkbox_Disabled", (64, 64), lambda d, s: d.rounded_rectangle((8, 8, 56, 56), 10, fill=rgba("#6A706D", 170), outline=rgba("#9A9E9C"), width=4))
    for state, on in (("Off", False), ("On", True)):
        def toggle(d, s, on=on):
            d.rounded_rectangle((4, 8, 124, 56), 24, fill=rgba("#176B5B" if on else "#6A706D"), outline=rgba("#D9A441"), width=3)
            x = 96 if on else 32
            d.ellipse((x - 20, 12, x + 20, 52), fill=rgba("#F4EEDC"))
        simple_control(f"T_Toggle_{state}", (128, 64), toggle)
    simple_control("T_Slider_Background", (256, 32), lambda d, s: d.rounded_rectangle((4, 10, 252, 22), 6, fill=rgba("#18201F"), outline=rgba("#8C6422"), width=2))
    simple_control("T_Slider_Fill", (256, 32), lambda d, s: d.rounded_rectangle((4, 10, 252, 22), 6, fill=rgba("#42A58C")))
    simple_control("T_Slider_Thumb", (64, 64), lambda d, s: d.ellipse((8, 8, 56, 56), fill=rgba("#D9A441"), outline=rgba("#F4EEDC"), width=4))
    return [p.stem for p in sorted(CONTROL_DIR.glob("*.png"))]


def generate_avatars() -> list[str]:
    frame_specs = {
        "T_AvatarFrame_Normal": ("#42A58C", 5), "T_AvatarFrame_Self": ("#F4EEDC", 7),
        "T_AvatarFrame_Host": ("#D9A441", 8), "T_AvatarFrame_CurrentTurn": ("#D9A441", 11),
        "T_AvatarFrame_Offline": ("#6A706D", 6),
    }
    for name, (color, width) in frame_specs.items():
        image = Image.new("RGBA", (192, 192), (0, 0, 0, 0)); d = ImageDraw.Draw(image)
        d.ellipse((16, 16, 176, 176), outline=rgba(color), width=width)
        d.ellipse((28, 28, 164, 164), outline=rgba("#18201F", 180), width=3)
        if name.endswith("Host"):
            d.polygon([(62, 35), (80, 58), (96, 28), (112, 58), (132, 35), (125, 72), (67, 72)], fill=rgba("#D9A441"))
        save(image, AVATAR_DIR / f"{name}.png")
    seat_specs = {"Empty": "#6A706D", "Ready": "#42A58C", "NotReady": "#D9A441", "Disconnected": "#B8463A"}
    for state, color in seat_specs.items():
        image = Image.new("RGBA", (192, 192), (0, 0, 0, 0)); d = ImageDraw.Draw(image)
        d.rounded_rectangle((12, 12, 180, 180), 32, fill=rgba("#073F36", 220), outline=rgba(color), width=7)
        d.ellipse((64, 42, 128, 106), fill=rgba(color, 170)); d.rounded_rectangle((45, 108, 147, 158), 24, fill=rgba(color, 170))
        save(image, AVATAR_DIR / f"T_Seat_{state}.png")
    return [p.stem for p in sorted(AVATAR_DIR.glob("*.png"))]


ICON_NAMES = [
    "LoginWechat", "LoginGuest", "QuickStart", "CreateRoom", "JoinRoom", "Lock", "Unlock", "Setting", "Rules", "Back", "Close", "Refresh", "Wifi", "Warning", "Ready", "Host", "Microphone", "Coin", "RoomCard", "Chicken", "BlackEight", "InternalExternalChicken", "Dou", "Fan"
]


def draw_icon(name: str) -> Image.Image:
    image = Image.new("RGBA", (128, 128), (0, 0, 0, 0)); d = ImageDraw.Draw(image)
    gold, cream, green, red = rgba("#D9A441"), rgba("#F4EEDC"), rgba("#42A58C"), rgba("#B8463A")
    d.ellipse((8, 8, 120, 120), fill=rgba("#073F36", 220), outline=gold, width=4)
    if name in {"Close", "Back", "Refresh", "Ready"}:
        if name == "Close": d.line((38, 38, 90, 90), fill=cream, width=10); d.line((90, 38, 38, 90), fill=cream, width=10)
        elif name == "Back": d.line((82, 34, 48, 64, 82, 94), fill=cream, width=10, joint="curve")
        elif name == "Refresh": d.arc((34, 34, 94, 94), 35, 315, fill=cream, width=9); d.polygon([(88, 28), (103, 48), (77, 48)], fill=cream)
        else: d.line((34, 67, 55, 88, 94, 40), fill=cream, width=10, joint="curve")
    elif name in {"Lock", "Unlock"}:
        d.rounded_rectangle((38, 58, 90, 96), 8, fill=gold)
        d.arc((45 if name == "Lock" else 58, 28, 83 if name == "Lock" else 96, 70), 180, 360, fill=cream, width=8)
    elif name == "Wifi":
        for box in ((28, 30, 100, 102), (42, 48, 86, 92)): d.arc(box, 215, 325, fill=cream, width=8)
        d.ellipse((58, 82, 70, 94), fill=gold)
    elif name == "Warning":
        d.polygon([(64, 24), (102, 98), (26, 98)], fill=red, outline=cream); d.line((64, 48, 64, 75), fill=cream, width=8); d.ellipse((60, 82, 68, 90), fill=cream)
    elif name in {"Setting", "Fan"}:
        d.ellipse((43, 43, 85, 85), outline=cream, width=10)
        for a in range(0, 360, 45):
            import math
            x = 64 + int(38 * math.cos(math.radians(a))); y = 64 + int(38 * math.sin(math.radians(a)))
            d.line((64, 64, x, y), fill=gold, width=7)
    elif name in {"Coin", "RoomCard", "BlackEight", "Dou"}:
        d.ellipse((34, 30, 94, 98), fill=gold, outline=cream, width=5)
        if name == "BlackEight": d.ellipse((51, 42, 77, 68), fill=rgba("#18201F")); d.ellipse((51, 68, 77, 94), fill=rgba("#18201F"))
        elif name == "RoomCard": d.rounded_rectangle((47, 43, 81, 85), 5, outline=rgba("#18201F"), width=5)
        else: d.ellipse((51, 49, 77, 79), outline=rgba("#18201F"), width=5)
    elif name in {"Chicken", "InternalExternalChicken"}:
        d.ellipse((43, 48, 84, 91), fill=cream); d.ellipse((76, 35, 99, 58), fill=gold); d.polygon([(96, 48), (112, 55), (97, 61)], fill=red); d.line((54, 91, 48, 105), fill=gold, width=5); d.line((72, 91, 78, 105), fill=gold, width=5)
    elif name == "Microphone":
        d.rounded_rectangle((49, 27, 79, 77), 15, fill=cream); d.arc((38, 45, 90, 94), 0, 180, fill=gold, width=7); d.line((64, 88, 64, 103), fill=gold, width=7)
    elif name in {"LoginGuest", "LoginWechat", "Host"}:
        d.ellipse((48, 30, 80, 62), fill=cream); d.rounded_rectangle((34, 65, 94, 98), 20, fill=green)
        if name == "Host": d.polygon([(43, 32), (54, 45), (64, 27), (74, 45), (86, 32), (80, 53), (48, 53)], fill=gold)
        elif name == "LoginWechat": d.ellipse((72, 64, 100, 86), fill=gold)
    else:
        # Generic room/action pictogram: a crisp doorway plus an identifying notch pattern.
        d.rounded_rectangle((34, 34, 94, 96), 8, outline=cream, width=8); d.rectangle((54, 57, 76, 96), fill=green); d.ellipse((70, 73, 77, 80), fill=gold)
    return image


def generate_icons() -> list[str]:
    for name in ICON_NAMES:
        save(draw_icon(name), ICON_DIR / f"Icon_{name}.png")
    return [f"Icon_{name}" for name in ICON_NAMES]


CN_NUM = "一二三四五六七八九"


def tile_base() -> tuple[Image.Image, ImageDraw.ImageDraw]:
    image = Image.new("RGBA", (256, 352), (0, 0, 0, 0)); d = ImageDraw.Draw(image)
    d.rounded_rectangle((15, 16, 241, 338), 30, fill=rgba("#D9D1B8"), outline=rgba("#8C6422"), width=5)
    d.rounded_rectangle((20, 12, 236, 326), 28, fill=rgba("#F4EEDC"), outline=(255, 255, 255, 220), width=3)
    return image, d


def generate_tiles() -> list[str]:
    names = []
    for index in range(1, 10):
        image, d = tile_base(); text = CN_NUM[index - 1] + "万"; f = font(76, True)
        box = d.multiline_textbbox((0, 0), text, font=f, spacing=0); x = (256 - (box[2] - box[0])) // 2
        d.multiline_text((x, 83), text, font=f, fill=rgba("#B8463A"), align="center", spacing=0)
        name = f"T_Tile_Wan_{index:02d}"; save(image, TILE_DIR / f"{name}.png"); names.append(name)
        image, d = tile_base()
        positions = [(128, 168)] if index == 1 else [(82 + (i % 3) * 46, 92 + (i // 3) * 72) for i in range(index)]
        for i, (x, y) in enumerate(positions):
            color = rgba("#B8463A") if i == 0 else rgba("#397DA5") if i % 2 else rgba("#176B5B")
            d.ellipse((x - 19, y - 19, x + 19, y + 19), outline=color, width=8); d.ellipse((x - 6, y - 6, x + 6, y + 6), fill=color)
        name = f"T_Tile_Tong_{index:02d}"; save(image, TILE_DIR / f"{name}.png"); names.append(name)
        image, d = tile_base()
        positions = [(128, 170)] if index == 1 else [(82 + (i % 3) * 46, 86 + (i // 3) * 82) for i in range(index)]
        for i, (x, y) in enumerate(positions):
            color = rgba("#B8463A") if index == 1 else rgba("#176B5B") if i % 2 == 0 else rgba("#397DA5")
            d.rounded_rectangle((x - 8, y - 29, x + 8, y + 29), 7, fill=color); d.line((x - 13, y, x + 13, y), fill=rgba("#D9A441"), width=4)
        name = f"T_Tile_Tiao_{index:02d}"; save(image, TILE_DIR / f"{name}.png"); names.append(name)
    image, d = tile_base(); d.rounded_rectangle((34, 30, 222, 308), 24, fill=rgba("#176B5B"), outline=rgba("#D9A441"), width=5); inset_pattern(d, image.size, rgba("#D9A441", 110)); save(image, TILE_DIR / "T_Tile_Back.png"); names.append("T_Tile_Back")
    image, _ = tile_base(); save(image, TILE_DIR / "T_Tile_FrontBlank.png"); names.append("T_Tile_FrontBlank")
    glow = Image.new("RGBA", (256, 352), (0, 0, 0, 0)); gd = ImageDraw.Draw(glow); gd.rounded_rectangle((12, 12, 244, 340), 34, outline=rgba("#D9A441", 230), width=15); glow = glow.filter(ImageFilter.GaussianBlur(7)); save(glow, TILE_DIR / "T_Tile_SelectedGlow.png"); names.append("T_Tile_SelectedGlow")
    mask = Image.new("RGBA", (256, 352), rgba("#18201F", 150)); save(mask, TILE_DIR / "T_Tile_DisabledMask.png"); names.append("T_Tile_DisabledMask")
    with (DATA_DIR / "DT_TileTextureRegistry.csv").open("w", newline="", encoding="utf-8-sig") as handle:
        writer = csv.writer(handle); writer.writerow(["RuleIndex", "Suit", "Rank", "TextureName"])
        for rule_index in range(27):
            suit = ("Wan", "Tong", "Tiao")[rule_index // 9]; rank = rule_index % 9 + 1
            writer.writerow([rule_index, suit, rank, f"T_Tile_{suit}_{rank:02d}"])
    return names


def normalize_backgrounds() -> list[dict]:
    result = []
    for path in sorted(BG_DIR.glob("*.png")):
        image = Image.open(path).convert("RGB")
        image = image.resize((2560, 1440), Image.Resampling.LANCZOS)
        save(image, path)
        result.append({"name": path.stem, "size": [2560, 1440], "streaming": True})
    return result


def quality_record(path: Path) -> dict:
    with Image.open(path) as image:
        alpha = "A" in image.getbands()
        extrema = image.getextrema()
        transparent_corners = False
        if alpha:
            a = image.getchannel("A")
            transparent_corners = all(a.getpixel(p) == 0 for p in ((0, 0), (image.width - 1, 0), (0, image.height - 1), (image.width - 1, image.height - 1)))
        return {
            "path": path.relative_to(ROOT).as_posix(), "size": [image.width, image.height], "mode": image.mode,
            "alpha": alpha, "transparent_corners": transparent_corners, "extrema": extrema,
            "bytes": path.stat().st_size, "sha256": hashlib.sha256(path.read_bytes()).hexdigest(),
        }


def main() -> None:
    ensure_dirs()
    inventory = {
        "design_tokens": COLORS,
        "backgrounds": normalize_backgrounds(),
        "panels": generate_panels(),
        "buttons": generate_buttons(),
        "controls": generate_controls(),
        "avatars": generate_avatars(),
        "icons": generate_icons(),
        "tiles": generate_tiles(),
    }
    files = sorted(SOURCE.rglob("*.png"))
    inventory["quality"] = [quality_record(path) for path in files]
    inventory["counts"] = {key: len(value) for key, value in inventory.items() if isinstance(value, list)}
    (DATA_DIR / "ui_asset_inventory.json").write_text(json.dumps(inventory, ensure_ascii=False, indent=2), encoding="utf-8")
    print(json.dumps(inventory["counts"], ensure_ascii=False))


if __name__ == "__main__":
    main()
