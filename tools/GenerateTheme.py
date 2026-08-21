#!/bin/env python3

from dataclasses import dataclass, astuple
from typing import overload
from typing_extensions import Self
import argparse
import copy
import math
import sys


@dataclass
class RGBA:
    r: float
    g: float
    b: float
    a: float = 1.0

    def __str__(self):
        return f"[{self.r:0=.2f}, {self.g:.2f}, {self.b:.2f}, {self.a:.2f}]"

    def __iter__(self):
        return iter(astuple(self))

    def __add__(self, other: Self) -> Self:
        return self.__class__(self.r + other.r,
                              self.g + other.g,
                              self.b + other.b,
                              self.a + other.a)

    def __sub__(self, other: Self) -> Self:
        return self.__class__(self.r - other.r,
                              self.g - other.g,
                              self.b - other.b,
                              self.a - other.a)

    @overload
    def __mul__(self, other: Self) -> Self: ...

    @overload
    def __mul__(self, other: float | int) -> Self: ...

    def __mul__(self, other: Self | float | int) -> Self:
        if isinstance(other, (float, int)):
            scalar = float(other)
            return self.__class__(self.r * scalar,
                                  self.g * scalar,
                                  self.b * scalar,
                                  self.a * scalar)
        if isinstance(other, RGBA):
            return self.__class__(self.r * other.r,
                                  self.g * other.g,
                                  self.b * other.b,
                                  self.a * other.a)
        return NotImplemented

    def __rmul__(self, other: float | Self) -> Self:
        return self.__mul__(other)

    def __truediv__(self, other: float) -> Self:
        return self.__class__(self.r / other,
                              self.g / other,
                              self.b / other,
                              self.a / other)


@dataclass
class HSVA:
    h: float
    s: float
    v: float
    a: float = 1.0

    def __str__(self):
        return f"[{self.h:0=.0f}, {self.s:.2f}, {self.v:.2f}, {self.a:.2f}]"

    def __iter__(self):
        return iter(astuple(self))


def normalize(x):
    return x / 255.0


def parse_hex(hex_str: str):
    original_hex_str = hex_str
    hex_str = hex_str.lstrip('#')

    if len(hex_str) == 3:
        # short string, just double every char
        hex_str = "".join(c+c for c in hex_str)

    if len(hex_str) != 6:
        raise ValueError("Invalid hex color: '{}'", original_hex_str)

    r = int(hex_str[0:2], 16)
    g = int(hex_str[2:4], 16)
    b = int(hex_str[4:6], 16)
    return RGBA(normalize(r), normalize(g), normalize(b), 1.0)


def lerp(a, b, t: float):
    return a + t * (b - a)


def clamp(x, lo, hi):
    if x < lo:
        return lo
    if x > hi:
        return hi
    return x


def to_hsva(rgba: RGBA) -> HSVA:
    r, g, b, a = rgba

    r = clamp(r, 0.0, 1.0)
    g = clamp(g, 0.0, 1.0)
    b = clamp(b, 0.0, 1.0)
    a = clamp(a, 0.0, 1.0)

    c_min = min(r, g, b)
    c_max = max(r, g, b)
    delta = c_max - c_min
    v = c_max
    s = 0.0 if v == 0.0 else delta / v

    if delta == 0.0:
        h = 0.0
    elif v == r:
        h = 60.0 * (((g - b) / delta) % 6.0)
    elif v == g:
        h = 60.0 * (((b - r) / delta) + 2.0)
    else:
        h = 60.0 * (((r - g) / delta) + 4.0)
    h %= 360.0
    return HSVA(h, s, v, a)


def to_rgba(hsva: HSVA) -> RGBA:
    h, s, v, a = hsva
    h = h % 360.0
    s = clamp(s, 0.0, 1.0)
    v = clamp(v, 0.0, 1.0)
    c = v * s
    x = c * (1.0 - abs((h / 60.0) % 2.0 - 1.0))
    m = v - c

    if h < 60.0:
        r, g, b = c, x, 0.0
    elif h < 120.0:
        r, g, b = x, c, 0.0
    elif h < 180.0:
        r, g, b = 0.0, c, x
    elif h < 240.0:
        r, g, b = 0.0, x, c
    elif h < 300.0:
        r, g, b = x, 0.0, c
    else:
        r, g, b = c, 0.0, x

    return RGBA(r + m, g + m, b + m, a)


# Return color with V shifted towards 0.5
def tone(color: RGBA, amount: float) -> RGBA:
    h, s, v, a = to_hsva(color)
    sign = 1.0 if v < 0.5 else -1.0
    v += sign * amount
    v = clamp(v, 0.0, 1.0)
    output = to_rgba(HSVA(h, s, v, a))
    return output


# Return color with alpha scaled.
def fade(color: RGBA, scale: float) -> RGBA:
    r, g, b, a = color
    a *= scale
    a = clamp(a, 0.0, 1.0)
    output = RGBA(r, g, b, a)
    return output


def shift_hue(color: RGBA, angle: float) -> RGBA:
    h, s, v, a = to_hsva(color)
    h += angle
    h %= 360.0
    output = to_rgba(HSVA(h, s, v, a))
    return output


def saturate(color: RGBA, amount: float) -> RGBA:
    h, s, v, a = to_hsva(color)
    s += amount
    s = clamp(s, 0.0, 1.0)
    output = to_rgba(HSVA(h, s, v, a))
    return output


def brighten(color: RGBA, amount: float) -> RGBA:
    h, s, v, a = to_hsva(color)
    v += amount
    v = clamp(v, 0.0, 1.0)
    output = to_rgba(HSVA(h, s, v, a))
    return output


def generate_json(name, colors):
    print( '{')
    print(f'    "name": "{name}",')
    print( '    "colors": {')

    if len(colors) > 0:
        max_key_length = max([len(key) for key in colors.keys()])
        quoted_key_width = max_key_length + 2

        for idx, (key, val) in enumerate(colors.items()):
            comma = ',' if idx < len(colors) - 1 else ''
            quoted_key = f'"{key}"'
            print(f'        {quoted_key:<{quoted_key_width}} : {val}{comma}')

    print( '    }')
    print( '}')



def generate_imgui_theme(*,
                         accent: RGBA,
                         bg: RGBA,
                         button: RGBA,
                         frame: RGBA,
                         link: RGBA | None,
                         text: RGBA,
                         text_accent: RGBA):

    hover_tone = 0.25
    active_tone = 0.5

    transparent = RGBA(0, 0, 0, 0)
    white = RGBA(1, 1, 1, 1)

    if to_hsva(bg).v < 0.5: # dark background
        highlight = RGBA(1, 1, 1, 1)
        lowlight = RGBA(0, 0, 0, 1)
    else: # light background
        highlight = RGBA(0, 0, 0, 1)
        lowlight = RGBA(1, 1, 1, 1)

    colors = {}

    colors["Text"]         = text
    colors["TextDisabled"] = tone(text, 0.4)

    colors["WindowBg"] = bg
    colors["ChildBg"]  = transparent
    colors["PopupBg"]  = tone(bg, 0.05)

    # Border color matches frame color, but less saturated.
    colors["Border"]       = fade(saturate(frame, -0.3), 0.5)
    colors["BorderShadow"] = transparent

    # Frames switch to button color when hovered or active.
    colors["FrameBg"]        = fade(frame, 0.5)
    colors["FrameBgHovered"] = fade(button, 0.4)
    colors["FrameBgActive"]  = fade(button, 0.7)

    colors["TitleBg"]          = tone(bg, 0.2)
    colors["TitleBgActive"]    = frame
    colors["TitleBgCollapsed"] = fade(tone(bg, 0.2), 0.5)

    colors["MenuBarBg"] = tone(bg, 0.05)

    # Scrollbars are always shades of gray, never stand out over other widgets.
    colors["ScrollbarBg"]          = fade(lowlight, 0.5)
    colors["ScrollbarGrab"]        = tone(lowlight, 0.3)
    colors["ScrollbarGrabHovered"] = tone(lowlight, 0.4)
    colors["ScrollbarGrabActive"]  = tone(lowlight, 0.5)

    # Checkmarks use the text accent color.
    colors["CheckMark"]          = text_accent
    colors["CheckboxSelectedBg"] = lerp(colors["FrameBg"], colors["FrameBgHovered"] , 0.65)

    colors["SliderGrab"]       = lerp(button, lowlight, 0.2)
    colors["SliderGrabActive"] = lerp(button, highlight, 0.2)

    colors["Button"]        = fade(button, 0.4)
    colors["ButtonHovered"] = button
    colors["ButtonActive"]  = lerp(saturate(button, -0.25), highlight, 0.1)

    # Headers use button colors.
    colors["Header"]        = colors["Button"]
    colors["HeaderHovered"] = colors["ButtonHovered"]
    colors["HeaderActive"]  = colors["ButtonActive"]

    # Separator uses border color (frame color with lower saturation)
    colors["Separator"] = colors["Border"]
    colors["SeparatorHovered"] = fade(frame, 0.8)
    colors["SeparatorActive"] = frame

    # Resize grip is button-like, to stand out above scroll bars.
    colors["ResizeGrip"]        = fade(button, 0.2)
    colors["ResizeGripHovered"] = fade(button, 0.6)
    colors["ResizeGripActive"]  = fade(button, 0.9)

    colors["InputTextCursor"] = text

    # Tabs use button colors, but use the accent when active.
    colors["TabHovered"]                = button
    colors["Tab"]                       = fade(button, 0.4)
    colors["TabSelected"]               = lerp(button, accent, 0.95)
    colors["TabSelectedOverline"]       = text_accent
    colors["TabDimmed"]                 = lerp(colors["Tab"], colors["TitleBg"], 0.8)
    colors["TabDimmedSelected"]         = lerp(colors["TabSelected"], colors["TitleBg"], 0.4)
    colors["TabDimmedSelectedOverline"] = lerp(text_accent, colors["TitleBg"], 0.4)

    # Plot colors are as far fom the accent as possible.
    plot_lines_base = saturate(shift_hue(accent, 120), 0.25)
    plot_histo_base = saturate(shift_hue(accent, -120), 0.25)
    colors["PlotLines"]            = lerp(plot_lines_base, highlight, 0.0)
    colors["PlotLinesHovered"]     = lerp(plot_lines_base, highlight, 0.5)
    colors["PlotHistogram"]        = lerp(plot_histo_base, highlight, 0.0)
    colors["PlotHistogramHovered"] = lerp(plot_histo_base, highlight, 0.5)

    colors["TableHeaderBg"]     = lerp(saturate(frame, -0.25), lowlight, 0.5)
    colors["TableBorderStrong"] = tone(saturate(frame, -0.25), -0.1)
    colors["TableBorderLight"]  = tone(saturate(frame, -0.5), -0.2)

    colors["TableRowBg"]    = transparent
    colors["TableRowBgAlt"] = fade(highlight, 0.05)

    if link is None:
        colors["TextLink"] = colors["HeaderActive"]
    else:
        colors["TextLink"] = link

    colors["TextSelectedBg"] = fade(accent, 0.4)

    colors["TreeLines"] = colors["Border"]

    colors["DragDropTarget"] = fade(lerp(text_accent, highlight, 0.5), 0.9)
    colors["DragDropTargetBg"] = transparent

    colors["UnsavedMarker"] = text

    colors["NavCursor"] = accent

    colors["NavWindowingHighlight"] = fade(highlight, 0.75)
    colors["NavWindowingDimBg"] = fade(lowlight, 0.75)

    colors["ModalWindowDimBg"] = fade(lowlight, 0.75)

    return colors


def main():
    parser = argparse.ArgumentParser(
        description='Generate an ImGui theme from a set of base colors.'
    )
    parser.add_argument('--name',        type=str,       required=True)

    parser.add_argument('--accent',      type=parse_hex, default=None)
    parser.add_argument('--bg',          type=parse_hex, required=True)
    parser.add_argument('--button',      type=parse_hex, default=None)
    parser.add_argument('--frame',       type=parse_hex, required=True)
    parser.add_argument('--link',        type=parse_hex, default=None)
    parser.add_argument('--text',        type=parse_hex, required=True)
    parser.add_argument('--text-accent', type=parse_hex, default=None)

    p = parser.parse_args()

    name = p.name

    accent = p.accent
    bg = p.bg
    button = p.button
    frame = p.frame
    link = p.link
    text = p.text
    text_accent = p.text_accent

    if accent is None:
        accent = frame

    if button is None:
        button = frame

    if text_accent is None:
        text_accent = text



    colors = generate_imgui_theme(
        accent      = accent,
        bg          = bg,
        button      = button,
        frame       = frame,
        link        = link,
        text        = text,
        text_accent = text_accent
    )

    generate_json(name, colors)


if __name__ == '__main__':
    main()
    # c1 = RGBA(0.25, 0.25, 0, 1)
    # print(c1)
    # # c2 = tone(c1, 0)
    # h, s, v, a = to_hsva(c1)
    # c2 = HSVA(h, s, v, a)
    # print(c2)
    # c3 = to_rgba(c2)
    # print(c3)
