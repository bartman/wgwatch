#!/usr/bin/env python3
"""Print every Braille cell and the block glyph with the same 2x4 fill.

Braille bits are reshuffled into the octant grid. The BLOCK OCTANT code
point is then:

    U+1CD00 + mask - (how many skipped masks are below it)

Unicode skipped 26 masks because an older block glyph already draws them.
Those are recognized from the bit pattern, not from a 256-entry table.
The 16 quadrant mosaics still need the Block Elements code points; those
assignments are not arithmetic.
"""

# Braille bit -> octant bit.
_BRAILLE_TO_OCTANT_BIT = (0, 2, 4, 1, 3, 5, 6, 7)


def braille_to_octant_mask(braille: int) -> int:
    bits = braille - 0x2800
    mask = 0
    for src, dst in enumerate(_BRAILLE_TO_OCTANT_BIT):
        if bits & (1 << src):
            mask |= 1 << dst
    return mask


def _quadrant_uniform(mask: int) -> bool:
    """Pairs (1,3), (2,4), (5,7), (6,8) are each both on or both off."""
    return (
        ((mask >> 0) & 1) == ((mask >> 2) & 1)
        and ((mask >> 1) & 1) == ((mask >> 3) & 1)
        and ((mask >> 4) & 1) == ((mask >> 6) & 1)
        and ((mask >> 5) & 1) == ((mask >> 7) & 1)
    )


def is_skipped(mask: int) -> bool:
    if _quadrant_uniform(mask):
        return True
    if mask in (0b00000011, 0b11000000):          # top row, bottom row
        return True
    if mask in (0b00111111, 0b11111100):          # upper 3 rows, lower 3 rows
        return True
    if mask in (0b00000001, 0b00000010, 0b01000000, 0b10000000):
        return True                               # corner cells 1, 2, 7, 8
    if mask in (0b00010100, 0b00101000):          # octants 3+5, 4+6
        return True
    return False


def _quadrant_code(mask: int) -> int:
    ul = (mask >> 0) & 1
    ur = (mask >> 1) & 1
    ll = (mask >> 4) & 1
    lr = (mask >> 5) & 1
    key = ul | (ur << 1) | (ll << 2) | (lr << 3)
    return (
        0x00A0, 0x2598, 0x259D, 0x2580,
        0x2596, 0x258C, 0x259E, 0x259B,
        0x2597, 0x259A, 0x2590, 0x259C,
        0x2584, 0x2599, 0x259F, 0x2588,
    )[key]


def skipped_code(mask: int) -> int:
    if _quadrant_uniform(mask):
        return _quadrant_code(mask)
    if mask == 0b00000001: return 0x1CEA8  # octant 1
    if mask == 0b00000010: return 0x1CEAB  # octant 2
    if mask == 0b01000000: return 0x1CEA3  # octant 7
    if mask == 0b10000000: return 0x1CEA0  # octant 8
    if mask == 0b00000011: return 0x1FB82  # top row
    if mask == 0b11000000: return 0x2582   # bottom row
    if mask == 0b00111111: return 0x1FB85  # upper three rows
    if mask == 0b11111100: return 0x2586   # lower three rows
    if mask == 0b00010100: return 0x1FBE6  # octants 3+5
    if mask == 0b00101000: return 0x1FBE7  # octants 4+6
    raise AssertionError(mask)


def octant_code(mask: int) -> int:
    if is_skipped(mask):
        return skipped_code(mask)
    skipped_below = sum(is_skipped(k) for k in range(mask))
    return 0x1CD00 + mask - skipped_below


def main() -> None:
    for braille in range(0x2800, 0x2900):
        oct_ = octant_code(braille_to_octant_mask(braille))
        print(f"U+{braille:05X} {chr(braille)}  U+{oct_:05X} {chr(oct_)}")


if __name__ == "__main__":
    main()
