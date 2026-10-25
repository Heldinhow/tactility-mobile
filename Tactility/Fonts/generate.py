#!/usr/bin/env python3
"""
Generates the font resources:
- Data/system/fonts/AdwaitaSans.ttf: text font TTF subset with the supported characters (non-v2 builds)
- Data/system/fonts/AdwaitaMono.ttf: monospace font TTF subset for the terminal (ASCII)
- Tactility/Fonts/Inter-*.ttf: Inter subsets for the v2 mobile type scale, used only by the build-time
  font generator (they are not embedded in the firmware)
- Tactility/Private/Tactility/lvgl/FontVersions.h: versions of the subsets, for font cache invalidation

The fonts are rasterized from the TTF subsets at build time and on device. Requires the fonttools
package: pip install fonttools brotli
"""
import os
import zlib

from fontTools import subset
from fontTools.ttLib import TTFont
from fontTools.varLib import instancer

# Variable font weight of the text font: 100 (Thin) to 900 (Black), 400 is Regular
TEXT_WEIGHT = 550

# Inclusive ranges
TEXT_CODEPOINT_RANGES = [
    (0x0020, 0x007E), # ASCII
    (0x00A0, 0x00FF), # Latin-1 Supplement
    (0x2010, 0x2027), # Dashes, quotes, bullet, ellipsis
    (0x2030, 0x203A), # Per mille, primes, angle quotes
    (0x20AC, 0x20AC), # Euro sign
]

ASCII_CODEPOINT_RANGES = [
    (0x0020, 0x007E),
    (0x00B7, 0x00B7), # middle dot, used in the Wi-Fi caption
    (0x2026, 0x2026), # ellipsis, used in the scanning/joining states
]

CLOCK_CODEPOINT_RANGES = [
    (0x0020, 0x0020), # space
    (0x002D, 0x002D), # hyphen
    (0x0030, 0x003A), # digits and colon
]

MONO_CODEPOINT_RANGES = [
    (0x0020, 0x007E),
]

base_dir = os.path.dirname(os.path.abspath(__file__))
output_dir = os.path.join(base_dir, "..", "..", "Data", "system", "fonts")
version_header_path = os.path.join(base_dir, "..", "Private", "Tactility", "lvgl", "FontVersions.h")


def subset_font(source, output_path: str, codepoint_ranges: list, weight=None) -> bytes:
    """Writes a TTF subset and returns its bytes. source is a path or a TTFont."""
    font = TTFont(source, recalcTimestamp=False) if isinstance(source, str) else source
    if "fvar" in font:
        # Pin the weight axis and all other axes to their defaults
        axis_values = {axis.axisTag: None for axis in font["fvar"].axes}
        if weight is not None:
            axis_values["wght"] = weight
        font = instancer.instantiateVariableFont(font, axis_values)
    options = subset.Options()
    options.layout_features = []
    options.drop_tables += ["GSUB", "GPOS", "GDEF"]
    options.hinting = False
    options.notdef_outline = True
    subsetter = subset.Subsetter(options)
    subsetter.populate(unicodes=[cp for first, last in codepoint_ranges for cp in range(first, last + 1)])
    subsetter.subset(font)
    os.makedirs(os.path.dirname(output_path), exist_ok=True)
    font.save(output_path)
    with open(output_path, "rb") as file:
        return file.read()


def generate_subset(source_file: str, output_file: str, codepoint_ranges: list) -> bytes:
    output_path = os.path.join(output_dir, output_file)
    print(f"Generating {output_path}")
    return subset_font(os.path.join(base_dir, source_file), output_path, codepoint_ranges)


def generate_inter_subset(source_file: str, output_file: str, codepoint_ranges: list) -> bytes:
    """The Inter subsets are build inputs for the bitmap fonts, not runtime assets, so they stay in
    Tactility/Fonts and are not embedded in Data/system."""
    output_path = os.path.join(base_dir, output_file)
    print(f"Generating {output_path}")
    return subset_font(os.path.join(base_dir, source_file), output_path, codepoint_ranges)


def enable_tabular_figures(font):
    """Maps each digit to Inter's tabular-figure glyph (the tnum feature), because binfont reads
    the cmap directly and does not shape text."""
    gsub = font["GSUB"].table
    mapping = {}
    for record in gsub.FeatureList.FeatureRecord:
        if record.FeatureTag != "tnum":
            continue
        for index in record.Feature.LookupListIndex:
            lookup = gsub.LookupList.Lookup[index]
            if lookup.LookupType != 1:
                continue
            for subtable in lookup.SubTable:
                mapping.update(subtable.mapping)
    for table in font["cmap"].tables:
        for codepoint, name in list(table.cmap.items()):
            if name in mapping:
                table.cmap[codepoint] = mapping[name]


def generate_clock_subset(source_file: str, output_file: str, codepoint_ranges: list) -> bytes:
    output_path = os.path.join(base_dir, output_file)
    print(f"Generating {output_path}")
    font = TTFont(os.path.join(base_dir, source_file), recalcTimestamp=False)
    enable_tabular_figures(font)
    return subset_font(font, output_path, codepoint_ranges)


text_data = generate_subset("AdwaitaSans-Regular.ttf", "AdwaitaSans.ttf", TEXT_CODEPOINT_RANGES)
mono_data = generate_subset("AdwaitaMono-Regular.ttf", "AdwaitaMono.ttf", MONO_CODEPOINT_RANGES)

inter_regular_ascii = generate_inter_subset("Inter-Regular.ttf", "Inter-Regular-Ascii.ttf", ASCII_CODEPOINT_RANGES)
inter_medium_ascii = generate_inter_subset("Inter-Medium.ttf", "Inter-Medium-Ascii.ttf", ASCII_CODEPOINT_RANGES)
inter_medium_latin1 = generate_inter_subset("Inter-Medium.ttf", "Inter-Medium-Latin1.ttf", TEXT_CODEPOINT_RANGES)
inter_medium_clock = generate_clock_subset("Inter-Medium.ttf", "Inter-Medium-Clock.ttf", CLOCK_CODEPOINT_RANGES)

# One version covers every text font: any subset change invalidates all cached text fonts
text_crc = zlib.crc32(text_data)
for data in (inter_regular_ascii, inter_medium_ascii, inter_medium_latin1, inter_medium_clock):
    text_crc = zlib.crc32(data, text_crc)
text_version = text_crc & 0xFFFFFFFF
mono_version = zlib.crc32(mono_data) & 0xFFFFFFFF

print(f"Generating {version_header_path}")
with open(version_header_path, "w") as file:
    file.write("#pragma once\n\n")
    file.write("// Generated by Tactility/Fonts/generate.py\n")
    file.write("// The versions change whenever the TTF subsets change\n")
    file.write(f"#define TT_TEXT_FONT_VERSION 0x{text_version:08X}u\n")
    file.write(f"#define TT_MONO_FONT_VERSION 0x{mono_version:08X}u\n")
