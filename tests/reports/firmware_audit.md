# DDI firmware bit audit

## Conclusion

The AT parser instruction at DDI file offset `0x0C3D0` remains the only
confidently reconstructable defect found. No additional candidate is justified
as a repair by the comparisons below. This does **not** certify the remaining
image: these are different adapter firmware versions, not two reads of one ROM.

The original binaries are never modified or committed. The candidate repair is
used only in a separately labelled AT diagnostic target, in memory.

## Inputs and provenance

Both images are 524,288-byte, big-endian SH firmware images mapped at `0x08000000`.
Offsets below are **file offsets**, unless explicitly identified as addresses.

| Image | Version string | Original SHA-256 |
| --- | --- | --- |
| DDI | `00.36GDD504853` | `3151a0b81177d94a4a186d0b5e3c6659279739f12ee77c5fc4b443a744199386` |
| cdmaOne | `00.10GCD504853` | `3f8eeed8afeb6509f27fb755b4e4d6f90c3579498a9529e3c9efe6ac752a8549` |

The ROM memory in the supplied `mobile_2026_10_19.gar` Ghidra project matches
these binaries byte for byte. It provides annotations, not an independent or
corrected copy. Names are advisory: cdmaOne's function labelled `at_parse_ATV`
at address `0x08028642` is reached by **X**, not V. V actually dispatches to
`0x080285DE`, and both firmwares reject V2. The tests follow the dispatch code.

## Search scope

1. The raw comparison visits every word-aligned 64-byte window in each complete
   image, including regions outside annotated functions. It finds windows with
   identical first/last eight bytes and exactly one differing bit between them.
   Windows with fewer than eight distinct byte values and anchors with more than
   eight candidate locations are excluded to reduce padding/repetition matches.
   Overlapping matches are deduplicated by the two differing-byte offsets.
   This yields **26 candidates**, listed below. Reproduce without Ghidra:

   ```sh
   python3 tests/firmware_emulator/audit.py
   ```

2. All 884 DDI and 671 cdmaOne functions in the supplied Ghidra project were
   exported as decoded instructions. Concrete 7/8-digit address operands were
   masked, preserving registers, immediates, instruction kinds, and ordering.
   For DDI functions with at least 12 instructions, shared four-instruction
   sequences shortlisted 20 counterparts. Candidates had to differ in length by
   at most two instructions or 10%, and score at least 90% with Python
   `difflib.SequenceMatcher(autojunk=False)`. This found **445** matches:
   **401** exact normalized matches and **44** near matches. These are heuristic
   matches, not proof of equivalence; repeated helpers can match ambiguously.
   Manual review of the near-match edits found eight aligned one-bit instruction
   replacements. Their classifications are recorded below.

3. The project's entire decoded ROM listing was checked for flow destinations
   lacking a defined instruction: 68,410 instructions / 136,820 bytes in DDI,
   and 61,938 / 123,876 in cdmaOne. Its recorded flow edges gave ten DDI and
   seven cdmaOne exceptions, all indirect JSRs. Seven shared exceptions point
   into RAM at `0x09001534` / `0x09000600`; DDI's three additional exceptions
   at `0x0801828E`, `0x080182A2`, `0x080182AC` point into bootloader regions
   (`0x080014E8`, `0x08000E74`, `0x08000600`) not decoded there. None was an
   unexplained direct branch into an invalid instruction. This checks only
   recorded edges, not all possible indirect destinations or runtime flows.

Instruction coverage within annotated function bodies is smaller: 123,530 DDI
bytes and 109,772 cdmaOne bytes. The unclassified remainder contains data,
padding, and potentially additional code. Neither a valid instruction nor an
exact normalized match proves that its literals, tables, or parameters are intact.
The raw scan can miss clustered errors, unmatched regions, repetitive data,
differences in anchors, and errors within eight bytes of an image boundary.

## Raw isolated-bit candidates

The byte pair is DDI to cdmaOne. A one-bit difference is a search result, not a
suggestion to copy that byte. In particular, helpers with different signedness
or return values can match the wrong counterpart.

| DDI offset | cdmaOne offset | Byte pair | Assessment |
| --- | --- | --- | --- |
| `0x05005` | `0x05005` | `3A / 3B` | PC-relative literal displacement in startup; different layout. |
| `0x05253` | `0x24CC3` | `E7 / E3` | Different initialization constants, with the reciprocal match below. |
| `0x3212F` | `0x0525B` | `E3 / E7` | Cross-match of another initialization copy; no unique repair. |
| `0x1ACA9` | `0x24421` | `4C / 4E` | Unsigned/signed byte extension; see explanation below. |
| `0x1F783` | `0x0CA5F` | `5C / 5E` | BRA displacement. |
| `0x1ED2D` | `0x0C001` | `F8 / FC` | BSR displacement. |
| `0x0B0CF` | `0x281EB` | `D3 / C3` | Register allocation: R13 versus R12. |
| `0x1EA77` | `0x0BD4B` | `3A / 3E` | BRA displacement. |
| `0x1F8A3` | `0x0CB7F` | `75 / 77` | BRA displacement. |
| `0x309DD` | `0x23199` | `34 / 35` | PC-relative literal displacement. |
| `0x2E7AF` | `0x1EF2B` | `4F / 4D` | Signed/unsigned word helper cross-match, reciprocal below. |
| `0x2E8E7` | `0x1EDF3` | `4D / 4F` | Reciprocal helper cross-match; no unique repair. |
| **`0x0C3D0`** | **`0x27288`** | **`5E / 7E`** | **Defective AT cursor load; supported repair candidate.** |
| `0x1A9CD` | `0x24149` | `67 / 65` | BSR displacement. |
| `0x1EAA9` | `0x0BD7D` | `3A / 3E` | BSR displacement. |
| `0x1F6B5` | `0x0C98D` | `C3 / C7` | BSR displacement. |
| `0x306C1` | `0x22E7D` | `36 / 37` | PC-relative literal displacement. |
| `0x1EEE9` | `0x0C1BD` | `01 / 05` | BRA displacement. |
| `0x102DD` | `0x2D0C5` | `1F / 1E` | PC-relative literal displacement. |
| `0x2FA49` | `0x0FB75` | `20 / 22` | PC-relative literal displacement. |
| `0x34253` | `0x30A33` | `01 / 00` | Constant-return helper cross-match, reciprocal below. |
| `0x355A7` | `0x2F5C7` | `00 / 01` | Reciprocal helper cross-match; no unique repair. |
| `0x1B009` | `0x0D049` | `CC / DC` | ADD register operand R12 versus R13. |
| `0x1B0D5` | `0x0CF7D` | `DC / CC` | Reciprocal ADD register cross-match. |
| `0x2AF75` | `0x1B299` | `60 / 70` | Structure-offset helper cross-match, reciprocal below. |
| `0x2AFB9` | `0x1B255` | `70 / 60` | Reciprocal offset-helper cross-match; no unique repair. |

## Instruction-aligned candidates

| DDI instruction offset | cdmaOne instruction offset | Assessment |
| --- | --- | --- |
| `0x0B412` | `0x285BA` | MOV #95,R3 versus R2, paired with the next comparison's register change. |
| `0x0B414` | `0x285BC` | CMP/GT R3,R4 versus R2,R4; consistent register allocation. |
| `0x0B5D4` | `0x28BB0` | Store through R3 versus R2, consistent with preceding pointer load; ambiguous helper match. |
| `0x0B800` | `0x28930` | Store through R3 versus R2, consistent with preceding pointer load. |
| `0x0C3D0` | `0x27288` | Known cursor defect. |
| `0x1AA44` | `0x241C0` | EXTU.B versus EXTS.B while reading an AT response line. |
| `0x1ACA8` | `0x24420` | Same signedness difference in another AT response-line reader. |
| `0x3081C` | `0x22FDC` | Saving a different register at this position; both functions save R12 and R13, in different order. |

The two byte-extension differences do not provide evidence of corruption. The
extended value is compared with LF (`0x0A`), while the original byte is stored
in the line buffer. Signed and unsigned extension preserve that equality and
the stored low byte even for inputs above `0x7F`. Copying the cdmaOne opcode
there would not constitute a supported repair.

## AT repair evidence and executable diagnostics

At DDI address `0x0800C3D0`, `5E FF` decodes as
`MOV.L @(60,R15),R14`. At the matching cdmaOne location, `7E FF` is
`ADD #-1,R14`. The surrounding code loads the command cursor, increments and
stores it, then needs the old cursor to index the command buffer. The DDI
instruction instead overwrites that index with an unrelated caller-stack word
at entry SP+24. Both parser functions have 158 decoded instructions; after
masking addresses, this is their only instruction difference.

`tests/test_at.py` runs 25 named cases per image/variant: E/Q/V/X settings and
range errors, lowercase, whitespace, chained commands, error short-circuiting,
empty commands, and independence from the suspect caller-stack word. The parser
receives a command body with its internal `0x8D` marker. No AT prefix decoding,
phone peripheral, full transport, physical timing, or game response framing is
claimed by these diagnostics. There are no firmware function substitutions.

| AT target | PASS | FAIL | ERROR |
| --- | ---: | ---: | ---: |
| Original DDI | 7 | 18 | 0 |
| DDI with candidate bit restored in memory | 25 | 0 | 0 |
| Original cdmaOne | 25 | 0 | 0 |

For example, with `E0` and caller-stack word 2, the original DDI parser reads
the terminator immediately, reports success, and leaves echo enabled. The test
also checks cursor progress, so this false success is a FAIL. Other commands
exceed the diagnostic execution budget. The corrected copy and cdmaOne consume
the whole command and update the settings. An instruction-budget failure is
distinct from an unsupported opcode or unmapped access, which remains an ERROR.

Candidate image SHA-256:
`75191991d1c5886ce6470eff657f5ffb2204fa1cf5dddf87c52e41c2e3df5ef9`.
The candidate changes exactly one bit of one byte and is used only by the AT
diagnostics. All original-image game-protocol tests retain the unmodified ROM.
libmobile has no internal AT parser; these cases therefore do not measure
libmobile compatibility. Its game-protocol contract results remain separate in
[the generated report](report.md).

The files establish a defective instruction and strongly support this one-bit
reconstruction. They cannot distinguish a bad read from flash damage, later file
corruption, or a bug present in the shipped firmware. Proving the original byte,
or finding every damaged region, requires an independent dump or another
trustworthy reference. No additional byte has been changed on speculation.
