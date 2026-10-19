"""Small functional SH-1 interpreter for selected original firmware routines.

Registers are unsigned 32-bit values. Branch delay slots execute before the
saved branch destination. This models neither instruction timing nor IRQs.
Unsupported instructions fail rather than silently substituting behavior.
"""

def signed(value, bits):
    value &= (1 << bits) - 1
    return value - (1 << bits) if value & 1 << bits - 1 else value


class InstructionLimit(RuntimeError):
    """Firmware did not return within the diagnostic execution budget."""


class CPU:

    def __init__(self, rom):
        self.rom = rom
        self.r = [0] * 16
        self.r[15] = 0x901ff00
        self.pr = 0xffffffff
        self.t = False
        self.mem = {}
        self.steps = 0
        self.hooks = {}
        self.substitutions = []
        self.macl = 0
        self.mach = 0
        self.sr = 0

    @staticmethod
    def mapped(a, n):
        return any(start <= a and a + n <= end for start, end in (
            (0x09000000, 0x09040000), (0x0f000000, 0x0f000800),
            (0x05fff000, 0x06000000)))

    def read(self, a, n):
        a &= 0xffffffff
        if 0x08000000 <= a and a + n <= 0x08000000 + len(self.rom):
            v = int.from_bytes(self.rom[a - 0x8000000:a - 0x8000000 + n], 'big')
        else:
            if not self.mapped(a, n):
                raise RuntimeError(f'unmapped read at 0x{a:08x}')
            v = int.from_bytes(bytes(self.mem.get(a + i, 0) for i in range(n)), 'big')
        return v

    def write(self, a, v, n):
        a &= 0xffffffff
        if not self.mapped(a, n):
            raise RuntimeError(f'unmapped write at 0x{a:08x}')
        v &= (1 << 8 * n) - 1
        for i, x in enumerate(v.to_bytes(n, 'big')):
            self.mem[a + i] = x

    def op(self, pc):
        if pc in self.hooks:
            name, function = self.hooks[pc]
            self.substitutions.append(name)
            self.r[0] = function(self) & 0xffffffff
            return self.pr, False
        w = self.read(pc, 2)
        self.steps += 1
        n = w >> 8 & 15
        m = w >> 4 & 15
        lo = w & 15
        top = w >> 12
        r = self.r
        target = None
        delay = False
        if top == 14:
            r[n] = signed(w & 255, 8)
        elif top == 13:
            r[n] = self.read((pc + 4 & ~3) + (w & 255) * 4, 4)
        elif top == 9:
            r[n] = signed(self.read(pc + 4 + (w & 255) * 2, 2), 16)
        elif top == 7:
            r[n] += signed(w & 255, 8)
        elif top == 1:
            self.write(r[n] + lo * 4, r[m], 4)
        elif top == 5:
            r[n] = self.read(r[m] + lo * 4, 4)
        elif top == 6:
            if lo == 3:
                r[n] = r[m]
            elif lo in [0, 1, 2, 4, 5, 6]:
                size = 1 << (lo & 3)
                r[n] = signed(self.read(r[m], size), size * 8)
                if lo >= 4 and n != m:
                    r[m] += size
            elif lo == 12:
                r[n] = r[m] & 255
            elif lo == 13:
                r[n] = r[m] & 0xffff
            elif lo == 14:
                r[n] = signed(r[m], 8)
            elif lo == 15:
                r[n] = signed(r[m], 16)
            elif lo == 11:
                r[n] = -r[m]
            else:
                raise Exception('group6')
        elif top == 2:
            if lo in [0, 1, 2, 4, 5, 6]:
                size = 1 << (lo & 3)
                if lo >= 4:
                    r[n] -= size
                self.write(r[n], r[m], size)
            elif lo == 8:
                self.t = r[n] & r[m] == 0
            elif lo == 9:
                r[n] &= r[m]
            elif lo == 10:
                r[n] ^= r[m]
            elif lo == 11:
                r[n] |= r[m]
            elif lo == 15:
                self.macl = (signed(r[n], 16) * signed(r[m], 16)) & 0xffffffff
            else:
                raise Exception('group2')
        elif top == 3:
            if lo == 0:
                self.t = r[n] == r[m]
            elif lo == 2:
                self.t = r[n] >= r[m]
            elif lo == 3:
                self.t = signed(r[n], 32) >= signed(r[m], 32)
            elif lo == 6:
                self.t = r[n] > r[m]
            elif lo == 7:
                self.t = signed(r[n], 32) > signed(r[m], 32)
            elif lo == 8:
                r[n] -= r[m]
            elif lo == 12:
                r[n] += r[m]
            else:
                raise Exception('group3')
        elif top in [10, 11]:
            target = pc + 4 + signed(w & 4095, 12) * 2
            delay = True
            if top == 11:
                self.pr = pc + 4
        elif top == 8:
            sub = w >> 8 & 15
            if sub == 8:
                self.t = r[0] == signed(w & 255, 8) & 0xffffffff
            elif sub in [9, 11, 13, 15]:
                take = self.t if sub in [9, 13] else not self.t
                if take:
                    target = pc + 4 + signed(w & 255, 8) * 2
                if sub in [13, 15]:
                    delay = True
                    target = target if take else pc + 4
            elif sub in [0, 1]:
                self.write(r[m] + lo * (sub + 1), r[0], sub + 1)
            elif sub in [4, 5]:
                r[0] = signed(self.read(r[m] + lo * (sub - 3), sub - 3), (sub - 3) * 8)
            else:
                raise Exception('group8')
        elif top == 12:
            sub = w >> 8 & 15
            imm = w & 255
            if sub == 8:
                self.t = r[0] & imm == 0
            elif sub == 9:
                r[0] &= imm
            elif sub == 10:
                r[0] ^= imm
            elif sub == 11:
                r[0] |= imm
            else:
                raise Exception('groupc')
        elif top == 4:
            sub = w & 255
            if sub == 11:
                self.pr = pc + 4
                target = r[n]
                delay = True
            elif sub == 43:
                target = r[n]
                delay = True
            elif sub == 34:
                r[n] -= 4
                self.write(r[n], self.pr, 4)
            elif sub == 38:
                self.pr = self.read(r[n], 4)
                r[n] += 4
            elif sub == 16:
                r[n] -= 1
                self.t = r[n] & 0xffffffff == 0
            elif sub == 21:
                self.t = signed(r[n], 32) > 0
            elif sub in [0, 32]:
                self.t = bool(r[n] & 0x80000000)
                r[n] <<= 1
            elif sub in [1, 33]:
                self.t = bool(r[n] & 1)
                r[n] = r[n] >> 1 if sub == 1 else signed(r[n], 32) >> 1
            elif sub in [8, 24, 40]:
                r[n] <<= {8: 2, 24: 8, 40: 16}[sub]
            elif sub in [9, 25, 41]:
                r[n] >>= {9: 2, 25: 8, 41: 16}[sub]
            elif sub == 0x11:
                self.t = signed(r[n], 32) >= 0
            elif sub in (0x12, 0x02):
                r[n] -= 4
                self.write(r[n], self.macl if sub == 0x12 else self.mach, 4)
            elif sub in (0x16, 0x06):
                value = self.read(r[n], 4)
                if sub == 0x16:
                    self.macl = value
                else:
                    self.mach = value
                r[n] += 4
            elif sub == 0x0e:
                self.sr = r[n]
                self.t = bool(self.sr & 1)
            else:
                raise Exception('group4')
        elif top == 0:
            if w == 9:
                pass
            elif w == 11:
                target = self.pr
                delay = True
            elif w & 255 == 0x1a:
                r[n] = self.macl
            elif w & 255 == 2:
                r[n] = (self.sr & ~1) | int(self.t)
            elif lo in [4, 5, 6]:
                self.write(r[0] + r[n], r[m], 1 << lo - 4)
            elif lo in [12, 13, 14]:
                r[n] = signed(self.read(r[0] + r[m], 1 << lo - 12), 8 * (1 << lo - 12))
            elif w & 255 == 42:
                r[n] = self.pr
            else:
                raise Exception('group0')
        else:
            raise Exception('unknown')
        self.r = [x & 0xffffffff for x in r]
        return (target, delay)

    def call(self, pc, args=(), limit=100000):
        initial_sp = self.r[15]
        self.pr = 0xffffffff
        for i, v in enumerate(args):
            self.r[4 + i] = v
        count = 0
        while pc != 0xffffffff:
            try:
                target, delay = self.op(pc)
                if delay:
                    dt, dd = self.op(pc + 2)
                    if dt is not None or dd:
                        raise RuntimeError('branch in delay slot')
            except Exception as e:
                raise RuntimeError(f'{e} at {pc:08x}, opcode {self.read(pc, 2):04x}') from e
            pc = target if target is not None else pc + 2
            count += 1
            if count > limit:
                raise InstructionLimit(f'Instruction limit at {pc:08x}')
        if self.r[15] != initial_sp:
            raise RuntimeError('unbalanced firmware stack')
        return self.r[0]
