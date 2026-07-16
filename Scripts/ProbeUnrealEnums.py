import unreal

for enum_name in ("TextureCompressionSettings", "TextureGroup", "TextureMipGenSettings", "SlateBrushDrawType", "MaterialDomain"):
    enum_type = getattr(unreal, enum_name)
    unreal.log(f"[EnumProbe] {enum_name}: {', '.join(name for name in dir(enum_type) if name.isupper())}")
