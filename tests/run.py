#!/usr/bin/env python3
"""Run shared game-protocol contracts and separate internal AT diagnostics."""
import argparse
from collections import Counter
from datetime import datetime
import hashlib
from pathlib import Path
import subprocess
import sys
import tempfile
import time
import unittest
from zoneinfo import ZoneInfo

from firmware_emulator import Firmware, PROFILES, load_images
from firmware_emulator.at import ATFirmware, REPAIR_OFFSET, REPAIR_SHA256
from libmobile import Libmobile, SOURCES, build
from report import write_reports
from test_protocol import GameProtocolTests
from test_at import ATParserTests


class Results(unittest.TestResult):
    def __init__(self, target):
        super().__init__()
        self.target = target
        self.records = []

    def startTest(self, test):
        super().startTest(test)
        self.started = time.monotonic()

    def record(self, test, status, detail=''):
        adapter = getattr(test, 'adapter', None)
        substitutions = sorted(set(getattr(getattr(adapter, 'cpu', None), 'substitutions', [])))
        record = {'target': self.target, 'name': test._testMethodName.removeprefix('test_'),
                  'description': test.shortDescription() or '', 'status': status,
                  'detail': detail, 'seconds': round(time.monotonic() - self.started, 4),
                  'exchanges': getattr(adapter, 'history', []), 'substitutions': substitutions}
        self.records.append(record)
        print(f'{status:5} {self.target:20} {record["name"]}')

    def addSuccess(self, test):
        super().addSuccess(test)
        self.record(test, 'PASS')

    def addFailure(self, test, error):
        super().addFailure(test, error)
        self.record(test, 'FAIL', str(error[1]))

    def addError(self, test, error):
        super().addError(test, error)
        self.record(test, 'ERROR', self._exc_info_to_string(error, test))

    def addSkip(self, test, reason):
        super().addSkip(test, reason)
        self.record(test, 'SKIP', reason)


def main():
    here = Path(__file__).resolve().parent
    root = here.parent
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--target', choices=('all', 'firmware', 'libmobile'), default='all')
    parser.add_argument('--suite', choices=('all', 'protocol', 'at'), default='all')
    parser.add_argument('--roms', type=Path, default=here / 'firmware_emulator' / 'roms')
    parser.add_argument('--output', type=Path, default=here / 'reports')
    args = parser.parse_args()
    if args.target == 'libmobile' and args.suite == 'at':
        parser.error('libmobile has no internal AT parser; use --target firmware --suite at')
    try:
        # Validate every input before running any tests. Library-only runs need no ROMs.
        images = load_images(args.roms) if args.target != 'libmobile' else [(p, None) for p in PROFILES]
        source_files = sorted(root.glob('*.c')) + sorted(root.glob('*.h'))
        source_hash = hashlib.sha256(b''.join(p.name.encode() + b'\0' + p.read_bytes() for p in source_files)).hexdigest()
        revision = subprocess.run(['git', '-C', str(root), 'log', '-1', '--format=%H', '--',
                                   *(name + '.c' for name in SOURCES)],
                                  check=True, capture_output=True, text=True).stdout.strip()
        records = []
        with tempfile.TemporaryDirectory(prefix='libmobile-tests-') as temporary:
            library = build(root, Path(temporary)) if args.target != 'firmware' and args.suite != 'at' else None
            for profile, rom in images:
                targets = []
                if args.suite != 'at':
                    if args.target != 'libmobile':
                        targets.append((profile.name + ' firmware', GameProtocolTests,
                                        lambda p=profile, r=rom: Firmware(p, r)))
                    if args.target != 'firmware':
                        targets.append((profile.name + ' libmobile', GameProtocolTests,
                                        lambda p=profile: Libmobile(p, library)))
                if args.target != 'libmobile' and args.suite != 'protocol':
                    targets.append((profile.name + ' AT original', ATParserTests,
                                    lambda p=profile, r=rom: ATFirmware(p, r)))
                    if profile.name == 'DDI':
                        targets.append(('DDI AT candidate repair', ATParserTests,
                                        lambda p=profile, r=rom: ATFirmware(p, r, repair=True)))
                for target, contracts, factory in targets:
                    case = type('AdapterTests', (contracts, unittest.TestCase), {'factory': staticmethod(factory)})
                    result = Results(target)
                    unittest.defaultTestLoader.loadTestsFromTestCase(case).run(result)
                    records.extend(result.records)
        metadata = {'generated': datetime.now(ZoneInfo('Europe/Berlin')).isoformat(timespec='seconds'),
                    'library_revision': revision, 'library_source_sha256': source_hash,
                    'profiles': [{'name': p.name, 'filename': p.filename, 'sha256': p.sha256} for p in PROFILES],
                    'at_candidate': {'offset': hex(REPAIR_OFFSET), 'original': '5eff', 'candidate': '7eff',
                                     'sha256': REPAIR_SHA256,
                                     'scope': 'AT diagnostics only; original ROM files are never modified.'}}
        write_reports(args.output, records, metadata)
        totals = Counter(r['status'] for r in records)
        print('\n' + ', '.join(f'{totals[status]} {status}' for status in ('PASS', 'FAIL', 'ERROR', 'SKIP')))
        print(f'Reports: {args.output.resolve()}')
        return int(bool(totals['FAIL'] or totals['ERROR']))
    except (OSError, ValueError, subprocess.CalledProcessError) as error:
        print(f'Cannot run suite: {error}', file=sys.stderr)
        if isinstance(error, subprocess.CalledProcessError):
            print(error.stderr, file=sys.stderr)
        return 2


if __name__ == '__main__':
    sys.exit(main())
