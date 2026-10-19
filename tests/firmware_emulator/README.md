# Firmware protocol tests

Requires Python 3.10+ and a C11 compiler (`cc`, or `$CC`). From the repository root:

```sh
python3 tests/run.py
```

Put these private, unmodified dumps in `tests/firmware_emulator/roms/` (ignored by Git):

- `DDI_SST_39VF400_00-07FFFF.bin`
- `cdmaOne_SST_39VF400_00-07FFFF.bin`

Only the SHA-256 values in `firmware.py` are accepted. No Ghidra installation is
needed. `--target firmware` checks the dumps; `--target libmobile` checks the same
contracts without needing the dumps. Reports are written to `tests/reports/` as HTML,
Markdown, and JSON. Exit codes: **0** all pass, **1** contract/runtime failures,
**2** missing inputs or build errors. Current libmobile differences are real FAILs.

The tests exchange complete 8-bit game packets. `cpu.py` executes the original
serial and command routines. EEPROM operations, phone status/disconnect, and
socket calls are substituted; connected TCP/UDP cases explicitly seed an open
connection. Each HTML result lists the substitutions it actually used.

`--suite protocol` selects the shared game contracts. `--suite at --target firmware`
selects internal AT diagnostics, with no firmware function substitutions. These
also run a clearly labelled DDI candidate repair in memory (one bit at `0xC3D0`);
the original files remain unchanged. libmobile has no internal AT parser.
Neither suite validates physical timings, 32-bit transfers, or the original
network stack. See `tests/reports/firmware_audit.md` for the bit audit and its limits.
Reproduce its raw candidate scan with `python3 tests/firmware_emulator/audit.py`.

Harness checks: `python3 -m unittest discover -s tests -p test_harness.py`.
