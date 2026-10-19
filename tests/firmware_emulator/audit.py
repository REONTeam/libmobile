#!/usr/bin/env python3
"""Find isolated bit differences in otherwise identical, relocated ROM windows.

This produces candidates for manual inspection, never automatic repairs. It
does not find dense corruption, unmatched code, or all legitimate differences.
"""
import argparse
from collections import defaultdict
import json
from pathlib import Path


def bit_candidates(ddi, cdma):
    """Search all word-aligned 64-byte windows, using unchanged end anchors.

    Eight-byte anchors avoid quadratic whole-image matching. Repetitive windows
    and anchors with more than eight locations are excluded as ambiguous. A bit
    must lie between the anchors, so differences at the image edges can be missed.
    """
    width, anchor = 64, 8

    def windows(image):
        for offset in range(0, len(image) - width + 1, 2):
            window = bytes(image[offset:offset + width])
            if len(set(window)) >= 8:
                yield offset, window, (window[:anchor], window[-anchor:])

    index = defaultdict(list)
    for offset, _, key in windows(cdma):
        index[key].append(offset)
    found = set()
    for offset, window, key in windows(ddi):
        matches = index.get(key, [])
        if len(matches) > 8:
            continue
        for other in matches:
            differences = [i for i, (a, b) in enumerate(zip(window, cdma[other:other + width])) if a != b]
            if len(differences) != 1:
                continue
            i = differences[0]
            if (window[i] ^ cdma[other + i]).bit_count() == 1:
                found.add((offset + i, other + i))
    rows = []
    for offset, other in sorted(found):
        left = right = 0
        while left < min(offset, other, 4096) and ddi[offset - left - 1] == cdma[other - left - 1]:
            left += 1
        while (right < 4096 and offset + right + 1 < len(ddi) and other + right + 1 < len(cdma)
               and ddi[offset + right + 1] == cdma[other + right + 1]):
            right += 1
        rows.append({'ddi_offset': offset, 'cdma_offset': other,
                     'ddi_byte': ddi[offset], 'cdma_byte': cdma[other],
                     'matching_left': left, 'matching_right': right})
    return rows


def main():
    # Permit both direct execution and importing the scanner in harness checks.
    import sys
    sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
    from firmware_emulator import load_images
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--roms', type=Path, default=Path(__file__).parent / 'roms')
    args = parser.parse_args()
    (_, ddi), (_, cdma) = load_images(args.roms)
    rows = bit_candidates(ddi, cdma)
    for row in rows:
        for field in ('ddi_offset', 'cdma_offset', 'ddi_byte', 'cdma_byte'):
            row[field] = hex(row[field])
    print(json.dumps(rows, indent=2))


if __name__ == '__main__':
    main()
