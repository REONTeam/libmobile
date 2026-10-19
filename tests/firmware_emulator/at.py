"""Internal AT parser diagnostics, separate from the game-protocol contracts.

The transport has already stripped AT and replaced CR with its internal 0x8d
marker at this boundary. No phone driver or firmware function is substituted.
These tests inspect parser flags and settings, not bytes returned to the game.
"""
from dataclasses import asdict, dataclass
from hashlib import sha256

from .cpu import InstructionLimit
from .firmware import Firmware


REPAIR_OFFSET = 0xC3D0
REPAIR_SHA256 = '75191991d1c5886ce6470eff657f5ffb2204fa1cf5dddf87c52e41c2e3df5ef9'


def candidate_repair(rom):
    """Change only MOV.L @(60,SP),R14 to ADD #-1,R14 in a private copy."""
    if rom[REPAIR_OFFSET:REPAIR_OFFSET + 2] != bytes.fromhex('5eff'):
        raise ValueError('DDI repair requires the original 5eff instruction')
    image = bytearray(rom)
    image[REPAIR_OFFSET] ^= 0x20
    if sha256(image).hexdigest() != REPAIR_SHA256:
        raise ValueError('DDI repair requires the pinned original image')
    return bytes(image)


@dataclass(frozen=True)
class ATResult:
    completed: bool
    success: int
    error: int
    cursor: int
    echo: int
    quiet: int
    verbose: int
    extended: int


class ATFirmware:
    def __init__(self, profile, rom, repair=False):
        self.profile = profile
        self.firmware = Firmware(profile, candidate_repair(rom) if repair else rom)
        self.cpu = self.firmware.cpu
        self.cpu.hooks.clear()
        self.history = []
        ddi = profile.name == 'DDI'
        # ROM literal addresses, not guessed RAM layout offsets.
        literals = ((0x0800C440, 0x0800C438, 0x0800C43C, 0x0800AB68,
                     0x0800B224, 0x0800B324, 0x0800B46C, 0x0800B648) if ddi else
                    (0x080272F8, 0x080272F0, 0x080272F4, 0x08027C80,
                     0x080283C4, 0x080284C4, 0x08028614, 0x08028810))
        (self.buffer, self.cursor, self.error, self.success,
         self.echo, self.quiet, self.verbose, self.extended) = (self.cpu.read(a, 4) for a in literals)
        self.entry = 0x0800C3A4 if ddi else 0x0802725C

    def command(self, body, caller_stack=0, echo=1, quiet=0, verbose=1):
        """Execute one command body in initialized RAM with explicit settings.

        The unrelated word at entry SP+24 exposes the broken DDI cursor load.
        Each test creates a fresh instance, including after a nonreturning call.
        """
        data = body.encode('ascii')
        if len(data) > 80 or any(b < 0x20 or b > 0x7E for b in data):
            raise ValueError('AT body must contain at most 80 printable ASCII bytes')
        c = self.cpu
        self.firmware.load(self.buffer, data + b'\x8d')
        for address, value in ((self.echo, echo), (self.quiet, quiet), (self.verbose, verbose),
                               (self.extended, 0), (self.error, 0), (self.success, 0)):
            c.write(address, value, 1)
        c.write(c.r[15] + 24, caller_stack, 4)
        completed = True
        try:
            c.call(self.entry, limit=10000)
        except InstructionLimit:
            completed = False
        result = ATResult(completed, c.read(self.success, 1), c.read(self.error, 1),
                          c.read(self.cursor, 4), c.read(self.echo, 1),
                          c.read(self.quiet, 1), c.read(self.verbose, 1), c.read(self.extended, 1))
        self.history.append({'command': 'AT' + body, 'payload': data.hex(),
                             'conditions': f'Entry SP+24={caller_stack}; initial echo={echo}, quiet={quiet}, verbose={verbose}, extended=0',
                             'reply': str(asdict(result))})
        return result

    def close(self):
        self.firmware.close()
