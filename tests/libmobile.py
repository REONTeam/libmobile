"""Build the current checkout unchanged, with a deterministic host environment."""
import ctypes
import os
from pathlib import Path
import shlex
import subprocess
import sys

from protocol import Link


SOURCES = ('callback commands config debug dns inet_pton mobile relay serial util').split()


def build(root, output):
    library = output / ('libtest.dylib' if sys.platform == 'darwin' else 'libtest.so')
    command = shlex.split(os.environ.get('CC', 'cc')) + [
        '-std=c11', '-O0', '-Wall', '-Wextra', '-shared', '-fPIC', '-I', str(root),
        str(Path(__file__).with_name('libmobile_bridge.c')),
        *(str(root / (name + '.c')) for name in SOURCES), '-o', str(library)]
    subprocess.run(command, check=True, capture_output=True, text=True)
    lib = ctypes.CDLL(str(library))
    lib.test_new.argtypes = [ctypes.c_uint]
    lib.test_new.restype = ctypes.c_void_p
    lib.test_free.argtypes = [ctypes.c_void_p]
    lib.test_byte.argtypes = [ctypes.c_void_p, ctypes.c_uint]
    lib.test_byte.restype = ctypes.c_uint
    lib.test_peer.argtypes = [ctypes.c_void_p, ctypes.c_char_p, ctypes.c_uint]
    lib.test_tcp.argtypes = [ctypes.c_void_p]
    lib.test_internet.argtypes = [ctypes.c_void_p, ctypes.c_uint]
    lib.test_receive.argtypes = [ctypes.c_void_p, ctypes.c_char_p, ctypes.c_uint, ctypes.c_uint]
    return lib


class Libmobile(Link):
    def __init__(self, profile, library):
        self.library = library
        self.device = profile.device
        self.context = library.test_new(profile.device & 0x7F)
        if not self.context:
            raise MemoryError('cannot allocate libmobile test adapter')

    def transfer(self, byte):
        return self.library.test_byte(self.context, byte)

    def internet(self):
        self.library.test_internet(self.context, False)

    def udp_connection(self):
        self.library.test_internet(self.context, True)

    def peer(self, address, port):
        self.library.test_peer(self.context, address, port)

    def tcp_connection(self):
        self.library.test_tcp(self.context)

    def receive(self, data=b'', echo=False):
        self.library.test_receive(self.context, data, len(data), echo)

    def close(self):
        self.library.test_free(self.context)
