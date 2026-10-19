"""Game-side 8-bit framing, shared by the two firmware and libmobile drivers."""
from dataclasses import dataclass


@dataclass(frozen=True)
class Reply:
    ack: int
    command: int | None = None
    data: bytes = b''

    def __repr__(self):
        return str(self)

    def __str__(self):
        if self.command is None:
            return f'ACK {self.ack:02X}, no reply'
        return f'ACK {self.ack:02X}; {self.command:02X}: {self.data.hex(" ").upper() or "(empty)"}'


class Link:
    """Each driver returns the byte staged before the current exchange."""
    def request(self, command, data=b'', **options):
        reply = self._request(command, data, **options)
        if not hasattr(self, 'history'):
            self.history = []
        self.history.append({'command': f'{command:02X}', 'payload': data.hex(' '),
                             'options': options, 'ack_device': self.ack_device, 'reply': str(reply)})
        return reply

    def _request(self, command, data=b'', *, checksum_error=False,
                length_high=0, reserved=0, device=0x80):
        if len(data) > 255:
            raise ValueError('request payload exceeds the protocol limit')
        header = bytes([command, reserved, length_high, len(data)])
        checksum = (sum(header + data) ^ int(checksum_error)).to_bytes(2, 'big')
        for byte in b'\x99\x66' + header + data + checksum:
            self.transfer(byte)
        self.ack_device = self.transfer(device)
        ack = self.transfer(0)
        previous = None
        for _ in range(24):
            byte = self.transfer(0x4B)
            if previous == 0x99 and byte == 0x66:
                break
            previous = byte
        else:
            return Reply(ack)
        header = bytes(self.transfer(0x4B) for _ in range(4))
        size = int.from_bytes(header[2:4], 'big')
        if size > 255:
            raise RuntimeError(f'invalid response length {size}')
        payload = bytes(self.transfer(0x4B) for _ in range(size))
        checksum = bytes(self.transfer(0x4B) for _ in range(2))
        if int.from_bytes(checksum, 'big') != sum(header + payload):
            raise RuntimeError('invalid response checksum')
        if self.transfer(0x4B) != self.device:
            raise RuntimeError('invalid response device ID')
        self.transfer(0x80)
        self.transfer(header[0] ^ 0x80)
        self.transfer(0x4B)
        return Reply(ack, header[0], payload)
