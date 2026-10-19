"""Original serial/command code with explicit EEPROM and network substitutes.

Addresses are pinned to SHA-256-identified images. Game-protocol tests avoid
DDI's defective AT parser: telephone status and network calls are supplied by
the controlled test environment. Separate AT diagnostics execute that parser.
"""
from dataclasses import dataclass
from hashlib import sha256
from pathlib import Path

from .cpu import CPU
from protocol import Link


@dataclass(frozen=True)
class Profile:
    name: str
    filename: str
    sha256: str
    device: int
    data_copy: tuple  # ROM start, ROM end, initialized RAM destination
    serial: tuple  # RX handler, main serial service, command service
    state: int
    device_address: int
    staged_byte: int
    idle_counter: int
    eeprom: tuple  # read, write, poll
    network: tuple  # phone status; literal addresses for socket API and errno
    connections: int


PROFILES = (
    Profile('DDI', 'DDI_SST_39VF400_00-07FFFF.bin',
        '3151a0b81177d94a4a186d0b5e3c6659279739f12ee77c5fc4b443a744199386',
        0x8B, (0x875C, 0x9698, 0x09014E58),
        (0x08009F6C, 0x0800970C, 0x0801936E), 0x090076CC,
        0x0900026A, 0x09000284, 0x09000266,
        (0x08031698, 0x0803165A, 0x08031638),
        (0x080108A0, 0x08030738, 0x08030874, 0x08030AD0, 0x08030BE8, 0x08030BE0, 0x08030BDC),
        0x090078F4),
    Profile('cdmaOne', 'cdmaOne_SST_39VF400_00-07FFFF.bin',
        '3f8eeed8afeb6509f27fb755b4e4d6f90c3579498a9529e3c9efe6ac752a8549',
        0x89, (0x87DC, 0x8F8C, 0x0902A47C),
        (0x08020E64, 0x080206A8, 0x08022342), 0x09020D18,
        0x090209D6, 0x090209F0, 0x090209D2,
        (0x0801FDFC, 0x0801FDBE, 0x0801FD9C),
        (0x0802D4A4, 0x08022EF4, 0x08023030, 0x08023290, 0x080233B0, 0x080233A8, 0x080233A4),
        0x09020F40),
)


def reverse(byte):
    return int(f'{byte:08b}'[::-1], 2)


class Firmware(Link):
    def __init__(self, profile, rom):
        self.profile = profile
        self.device = profile.device
        self.cpu = CPU(rom)
        self.eeprom = bytearray(range(256))
        self.incoming = b''
        self.echo = False
        self.datagram = True
        self.sent = []
        self.sockets = set()
        self.peer_address = None
        self.pending_eeprom = False
        start, end, address = profile.data_copy
        self.load(address, rom[start:end])
        if profile.name == 'cdmaOne':
            self.load(0x0F000000, rom[0x8F8C:0x9000])
        c = self.cpu
        c.write(profile.state, 4, 4)  # Powered, no protocol session yet.
        c.write(profile.device_address, self.device, 1)
        c.write(profile.staged_byte, reverse(0xD2), 1)
        c.write(profile.idle_counter, 3000, 2)
        c.write(0x05FFFEC3, reverse(0xD2), 1)
        offline = 0x0801AEE4 if profile.name == 'DDI' else 0x0802465C
        c.hooks[offline] = ('disconnect phone', lambda c: 0)
        for address, name, function in zip(profile.eeprom,
                ('EEPROM read', 'EEPROM write', 'EEPROM poll'),
                (self.eeprom_read, self.eeprom_write, self.eeprom_poll)):
            c.hooks[address] = (name, function)
        phone, *literals, error_code = profile.network
        c.hooks[phone] = ('phone status', self.phone_status)
        self.would_block = c.read(error_code, 2)
        for literal, name, function in zip(literals,
                ('socket()', 'close()', 'sendto()', 'recvfrom()', 'socket errno'),
                (self.allocate, self.close_socket, self.send, self.socket_receive, lambda c: self.would_block)):
            c.hooks[c.read(literal, 4)] = (name, function)

        send_literal, recv_literal = ((0x08030880, 0x08030984) if profile.name == 'DDI'
                                      else (0x0802303C, 0x08023140))
        c.hooks[c.read(send_literal, 4)] = ('send()', self.send)
        c.hooks[c.read(recv_literal, 4)] = ('recv()', self.socket_receive)

    def load(self, address, data):
        for i, byte in enumerate(data):
            self.cpu.write(address + i, byte, 1)

    def transfer(self, byte):
        c = self.cpu
        rx, serial, commands = self.profile.serial
        result = reverse(c.read(0x05FFFEC3, 1))
        c.write(0x05FFFEC5, reverse(byte), 1)
        c.call(rx)
        c.call(serial)
        c.call(commands)
        c.call(serial)
        return result

    def internet(self):
        """An established ISP connection, without executing the internal AT path."""
        self.cpu.write(self.profile.state, 2, 4)
        flag = 0x09007938 if self.profile.name == 'DDI' else 0x09020F84
        self.cpu.write(flag, 1, 4)

    def udp_connection(self):
        self.internet()
        slot = self.profile.connections
        self.cpu.write(slot, 0, 4)
        self.cpu.write(slot + 20, 1, 4)
        self.cpu.write(slot + 24, 1, 4)
        self.sockets.add(0)
        self.datagram = True

    def tcp_connection(self):
        self.udp_connection()
        self.cpu.write(self.profile.connections + 24, 0, 4)
        self.datagram = False

    def peer(self, address, port):
        self.peer_address = bytes(address) + port.to_bytes(2, 'big')

    def receive(self, data=b'', echo=False):
        self.incoming = data
        self.echo = echo

    def close(self):
        pass

    def eeprom_read(self, c):
        offset, dest, size = c.r[4] & 255, c.r[5], c.r[6]
        self.load(dest, self.eeprom[offset:offset + size])
        self.pending_eeprom = True
        return 0

    def eeprom_write(self, c):
        offset, src, size = c.r[4] & 255, c.r[5], c.r[6]
        self.eeprom[offset:offset + size] = bytes(c.read(src + i, 1) for i in range(size))
        self.pending_eeprom = True
        return 0

    def eeprom_poll(self, c):
        if self.pending_eeprom:
            self.pending_eeprom = False
            return -1  # One busy poll before the peripheral completes.
        return 0

    def phone_status(self, c):
        c.write(c.r[4], 1, 4)
        return 0

    def allocate(self, c):
        descriptor = 0
        while descriptor in self.sockets:
            descriptor += 1
        self.sockets.add(descriptor)
        return descriptor

    def close_socket(self, c):
        self.sockets.discard(c.r[4])
        return 0

    def send(self, c):
        conn, address, size = c.r[4:7]
        self.sent.append(bytes(c.read(address + i, 1) for i in range(size)))
        reached_peer = True
        if self.peer_address is not None:
            # SH C ABI: sendto's fifth argument (sockaddr*) is on the stack.
            address = c.read(c.r[15], 4)
            endpoint = bytes(c.read(address + i, 1) for i in range(8))
            reached_peer = endpoint[4:8] + endpoint[2:4] == self.peer_address
        if self.echo and reached_peer:
            self.incoming = self.sent[-1]
        return size

    def socket_receive(self, c):
        if not self.incoming:
            return -1
        data = self.incoming[:c.r[6]]
        self.load(c.r[5], data)
        self.incoming = b'' if self.datagram else self.incoming[len(data):]
        return len(data)


def load_images(directory):
    images = []
    for profile in PROFILES:
        path = Path(directory) / profile.filename
        rom = path.read_bytes()
        digest = sha256(rom).hexdigest()
        if digest != profile.sha256:
            raise ValueError(f'{path.name}: unsupported SHA-256 {digest}')
        images.append((profile, rom))
    return images
