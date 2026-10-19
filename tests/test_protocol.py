"""Observable GBC protocol contracts, shared unchanged by every test target.

Boundary cases below are deliberately small and named. Expected replies come
from both pinned firmware images; successful EEPROM/network cases use the
controlled peripherals described in README.md, not real phone measurements.
"""
from protocol import Reply


class GameProtocolTests:
    factory = None

    def setUp(self):
        self.adapter = self.factory()
        self.addCleanup(self.adapter.close)

    def start(self):
        self.reply(0x10, b'NINTENDO', 0x90, b'NINTENDO')

    def reply(self, command, data, response, payload=b'', **options):
        actual = self.adapter.request(command, data, **options)
        self.assertEqual(self.adapter.ack_device, self.adapter.device)
        expected = Reply(command ^ 0x80, response, payload)
        if actual != expected:
            self.fail(f'Expected: {expected}\nReceived: {actual}')

    def error(self, command, data, code):
        self.reply(command, data, 0xEE, bytes([command, code]))

    def test_start_echoes_nintendo(self):
        self.start()

    def test_start_accepts_trailing_byte_and_echoes_only_nintendo(self):
        self.reply(0x10, b'NINTENDOX', 0x90, b'NINTENDO')

    def test_start_rejects_alternative_libmobile_greeting(self):
        """The library's extra greeting is absent from both supplied firmwares."""
        self.error(0x10, b'EVERYONE HAPPY MOBILE CONNECTION', 2)

    def test_start_rejects_wrong_magic(self):
        self.error(0x10, b'XINTENDO', 2)

    def test_start_is_case_sensitive(self):
        self.error(0x10, b'nintendo', 2)

    def test_start_rejects_truncated_magic(self):
        self.error(0x10, b'NINTEND', 2)

    def test_start_rejects_empty_payload(self):
        self.error(0x10, b'', 2)

    def test_start_can_retry_after_invalid_magic(self):
        self.error(0x10, b'XINTENDO', 2)
        self.start()

    def test_start_rejects_second_session(self):
        self.start()
        self.error(0x10, b'NINTENDO', 1)

    def test_sync_ignores_incomplete_magic_and_garbage(self):
        for byte in b'\x99\x00\x66\xff\x4b':
            self.adapter.transfer(byte)
        self.start()

    def test_checksum_failure_is_reported_in_packet_ack(self):
        self.start()
        self.assertEqual(self.adapter.request(0x18, b'\0', checksum_error=True), Reply(0xF1))

    def test_parser_recovers_after_checksum_failure(self):
        self.start()
        self.adapter.request(0x18, b'\0', checksum_error=True)
        self.reply(0x18, b'\0', 0x98)

    def test_unknown_command_is_rejected_in_packet_ack(self):
        self.start()
        self.assertEqual(self.adapter.request(0x27), Reply(0xF0))

    def test_null_command_acknowledges_without_command_reply(self):
        self.start()
        self.assertEqual(self.adapter.request(0x0F), Reply(0x8F))

    def test_length_above_255_is_discarded(self):
        self.start()
        self.assertEqual(self.adapter.request(0x18, b'\0', length_high=1), Reply(0xD2))

    def test_reserved_header_byte_is_ignored(self):
        self.start()
        self.reply(0x18, b'\0', 0x98, reserved=1)

    def test_alternate_official_client_id_is_accepted(self):
        """The 8-bit interface also accepts the official GBA identifier 0x81."""
        self.start()
        self.reply(0x18, b'\0', 0x98, device=0x81)

    def test_invalid_client_id_repeats_adapter_id_without_response(self):
        self.start()
        self.assertEqual(self.adapter.request(0x18, b'\0', device=0x82), Reply(self.adapter.device))

    def test_config_read_rejects_missing_offset_and_size(self):
        self.start()
        self.error(0x19, b'', 2)

    def test_config_read_rejects_missing_size(self):
        self.start()
        self.error(0x19, b'\0', 2)

    def test_config_write_rejects_missing_offset(self):
        self.start()
        self.error(0x1A, b'', 2)

    def test_tcp_send_without_server_data_returns_connection_id(self):
        self.start()
        self.adapter.tcp_connection()
        self.reply(0x15, b'\0position', 0x95, b'\0')

    def test_tcp_send_can_return_already_received_server_data(self):
        self.start()
        self.adapter.tcp_connection()
        self.adapter.receive(b'server')
        self.reply(0x15, b'\0position', 0x95, b'\0server')

    def test_tcp_empty_receive_waits_when_no_server_data_exists(self):
        self.start()
        self.adapter.tcp_connection()
        self.assertEqual(self.adapter.request(0x15, b'\0'), Reply(0x95))

    def test_tcp_empty_receive_returns_available_server_data(self):
        self.start()
        self.adapter.tcp_connection()
        self.adapter.receive(b'server')
        self.reply(0x15, b'\0', 0x95, b'\0server')

    def test_tcp_receive_keeps_bytes_beyond_one_response(self):
        self.start()
        self.adapter.tcp_connection()
        data = bytes(i & 255 for i in range(300))
        self.adapter.receive(data)
        self.reply(0x15, b'\0', 0x95, b'\0' + data[:254])
        self.reply(0x15, b'\0', 0x95, b'\0' + data[254:])

    def test_udp_receive_keeps_datagram_tail_for_next_response(self):
        """A 300-byte datagram fits the adapter buffer but needs two serial replies."""
        self.start()
        self.adapter.udp_connection()
        data = bytes(i & 255 for i in range(300))
        self.adapter.receive(data)
        self.reply(0x15, b'\0', 0x95, b'\0' + data[:254])
        self.reply(0x15, b'\0', 0x95, b'\0' + data[254:])

    def test_udp_open_uses_requested_address_and_port(self):
        """The simulated peer echoes only packets sent to its exact endpoint."""
        self.start()
        self.adapter.internet()
        self.adapter.peer(bytes.fromhex('c0000201'), 0x1234)
        self.reply(0x25, bytes.fromhex('c00002011234'), 0xA5, b'\0')
        self.adapter.receive(echo=True)
        self.reply(0x15, b'\0position', 0x95, b'\0position')

    def test_udp_second_connection_keeps_its_own_endpoint(self):
        self.start()
        self.adapter.internet()
        self.reply(0x25, bytes.fromhex('c00002011234'), 0xA5, b'\0')
        self.reply(0x25, bytes.fromhex('c00002021235'), 0xA5, b'\1')
        self.adapter.peer(bytes.fromhex('c0000202'), 0x1235)
        self.adapter.receive(echo=True)
        self.reply(0x15, b'\1position', 0x95, b'\1position')

    def test_udp_first_connection_retains_endpoint_after_second_open(self):
        self.start()
        self.adapter.internet()
        self.reply(0x25, bytes.fromhex('c00002011234'), 0xA5, b'\0')
        self.reply(0x25, bytes.fromhex('c00002021235'), 0xA5, b'\1')
        self.adapter.peer(bytes.fromhex('c0000201'), 0x1234)
        self.adapter.receive(echo=True)
        self.reply(0x15, b'\0position', 0x95, b'\0position')

    def test_udp_send_to_other_endpoint_has_no_peer_echo(self):
        """Negative control: the peer must not echo a datagram addressed elsewhere."""
        self.start()
        self.adapter.internet()
        self.adapter.peer(bytes.fromhex('c0000202'), 0x1235)
        self.reply(0x25, bytes.fromhex('c00002011234'), 0xA5, b'\0')
        self.adapter.receive(echo=True)
        self.reply(0x15, b'\0position', 0x95, b'\0')

    def test_tcp_multiple_positions_use_one_open_connection(self):
        self.start()
        self.adapter.tcp_connection()
        self.adapter.receive(echo=True)
        for position in range(3):
            data = bytes([position]) * 8
            self.reply(0x15, b'\0' + data, 0x95, b'\0' + data)

    def test_udp_close_then_data_reports_invalid_connection(self):
        self.start()
        self.adapter.udp_connection()
        self.reply(0x26, b'\0', 0xA6, b'\0')
        self.error(0x15, b'\0position', 0)

    def test_mode_zero_returns_empty_success(self):
        self.start()
        self.reply(0x18, b'\0', 0x98)

    def test_mode_one_acknowledges_in_original_8_bit_mode(self):
        """Checks the switch command's reply only, not GBC support for 32-bit transfers."""
        self.start()
        self.reply(0x18, b'\1', 0x98)

    def test_empty_mode_request_defaults_to_zero(self):
        self.start()
        self.reply(0x18, b'', 0x98)

    def test_mode_rejects_value_two(self):
        self.start()
        self.error(0x18, b'\2', 2)

    def test_mode_ignores_trailing_payload(self):
        self.start()
        self.reply(0x18, b'\0\xff', 0x98)

    def test_config_read_requires_started_session(self):
        self.error(0x19, b'\0\1', 1)

    def test_config_write_requires_started_session(self):
        self.error(0x1A, b'\0\xff', 1)

    def test_config_write_reports_offset_and_written_size(self):
        self.start()
        self.reply(0x1A, b'\x40abcdefgh', 0x9A, b'\x40\x08')

    def test_config_write_can_be_read_back(self):
        self.start()
        self.reply(0x1A, b'\x40abcdefgh', 0x9A, b'\x40\x08')
        self.reply(0x19, b'\x40\x08', 0x99, b'\x40abcdefgh')

    def test_config_write_preserves_neighboring_bytes(self):
        self.start()
        self.reply(0x1A, b'\x40ABC', 0x9A, b'\x40\x03')
        self.reply(0x19, b'\x3f\x05', 0x99, b'\x3f\x3fABC\x43')

    def test_packet_magic_inside_config_data_does_not_restart_parser(self):
        self.start()
        data = b'\x99\x66\x4b\x80\xd2\x00\xff'
        self.reply(0x1A, b'\x30' + data, 0x9A, b'\x30\x07')
        self.reply(0x19, b'\x30\x07', 0x99, b'\x30' + data)

    def test_reinit_keeps_protocol_session_open(self):
        self.start()
        self.reply(0x16, b'', 0x96)
        self.reply(0x18, b'\0', 0x98)

    def test_reinit_preserves_config_bytes(self):
        self.start()
        self.reply(0x1A, b'\x40ABC', 0x9A, b'\x40\x03')
        self.reply(0x16, b'', 0x96)
        self.reply(0x19, b'\x40\x03', 0x99, b'\x40ABC')

    def test_session_end_waits_for_adapter_timer(self):
        """With no timer events, repeated polling must not complete session end."""
        self.start()
        self.assertEqual(self.adapter.request(0x11), Reply(0x91))

    def test_data_requires_connected_call(self):
        self.start()
        self.error(0x15, b'\0position', 1)

    def test_udp_open_requires_internet_connection(self):
        self.start()
        self.error(0x25, bytes.fromhex('7f0000011234'), 1)

    def test_udp_open_returns_connection_id(self):
        self.start()
        self.adapter.internet()
        self.reply(0x25, bytes.fromhex('7f0000011234'), 0xA5, b'\0')

    def test_udp_open_allocates_two_distinct_connections(self):
        self.start()
        self.adapter.internet()
        self.reply(0x25, bytes.fromhex('7f0000011234'), 0xA5, b'\0')
        self.reply(0x25, bytes.fromhex('7f0000011235'), 0xA5, b'\1')
        self.error(0x25, bytes.fromhex('7f0000011236'), 0)

    def test_udp_close_returns_connection_id(self):
        self.start()
        self.adapter.udp_connection()
        self.reply(0x26, b'\0', 0xA6, b'\0')

    def test_udp_close_rejects_unknown_connection(self):
        self.start()
        self.adapter.udp_connection()
        self.error(0x26, b'\1', 0)

    def test_udp_data_rejects_unknown_connection(self):
        self.start()
        self.adapter.udp_connection()
        self.error(0x15, b'\1position', 0)

    def test_udp_receive_without_data_replies_immediately(self):
        self.start()
        self.adapter.udp_connection()
        self.reply(0x15, b'\0', 0x95, b'\0')

    def test_udp_receive_returns_server_payload(self):
        self.start()
        self.adapter.udp_connection()
        self.adapter.receive(b'position')
        self.reply(0x15, b'\0', 0x95, b'\0position')

    def test_udp_receive_does_not_repeat_previous_datagram(self):
        self.start()
        self.adapter.udp_connection()
        self.adapter.receive(b'position')
        self.reply(0x15, b'\0', 0x95, b'\0position')
        self.reply(0x15, b'\0', 0x95, b'\0')


def config_read(name, offset, size, valid, description):
    def test(self):
        self.start()
        data = bytes([offset, size])
        if valid:
            self.reply(0x19, data, 0x99, bytes([offset]) + bytes(range(offset, offset + size)))
        else:
            self.error(0x19, data, 2)
    test.__name__ = 'test_' + name
    test.__doc__ = description
    return test


# Limits are crossed one at a time so failures identify the broken contract.
for name, offset, size, valid, description in (
    ('config_read_first_byte', 0, 1, True, 'Read the first addressable configuration byte.'),
    ('config_read_last_byte', 255, 1, True, 'Offset plus size may equal 256.'),
    ('config_read_maximum_chunk', 0, 128, True, '128 bytes is the maximum configuration chunk.'),
    ('config_read_last_maximum_chunk', 128, 128, True, 'The final half of configuration remains readable.'),
    ('config_read_rejects_zero_size', 0, 0, False, 'A zero-size read is an error, not an empty success.'),
    ('config_read_rejects_chunk_above_128', 0, 129, False, 'A valid address cannot override the chunk-size limit.'),
    ('config_read_rejects_address_overflow', 255, 2, False, 'A small read still cannot cross address 255.'),
    ('config_read_rejects_overflowing_maximum_chunk', 129, 128, False, 'Both bounds must hold together.'),
):
    setattr(GameProtocolTests, 'test_' + name, config_read(name, offset, size, valid, description))


def config_write(name, offset, size, valid, description):
    def test(self):
        self.start()
        data = bytes([offset]) + bytes(i & 255 for i in range(size))
        if valid:
            self.reply(0x1A, data, 0x9A, bytes([offset, size]))
        else:
            self.error(0x1A, data, 2)
    test.__name__ = 'test_' + name
    test.__doc__ = description
    return test


for name, offset, size, valid, description in (
    ('config_write_first_byte', 0, 1, True, 'The first byte can be written.'),
    ('config_write_last_byte', 255, 1, True, 'The final byte can be written.'),
    ('config_write_maximum_chunk', 0, 128, True, '128-byte writes are accepted.'),
    ('config_write_last_maximum_chunk', 128, 128, True, 'A maximum chunk can end exactly at address 256.'),
    ('config_write_rejects_zero_size', 0, 0, False, 'An offset without bytes to write is invalid.'),
    ('config_write_rejects_chunk_above_128', 0, 129, False, '129-byte configuration writes are rejected.'),
    ('config_write_rejects_address_overflow', 255, 2, False, 'Writing beyond the configuration area is rejected.'),
):
    setattr(GameProtocolTests, 'test_' + name, config_write(name, offset, size, valid, description))


def udp_echo(size):
    def test(self):
        self.start()
        self.adapter.udp_connection()
        self.adapter.receive(echo=True)
        data = bytes(i & 255 for i in range(size))
        self.reply(0x15, b'\0' + data, 0x95, b'\0' + data)
    test.__doc__ = 'A deterministic peer echoes the outgoing payload, including framing-like byte values.'
    return test


for name, size in (('single_byte', 1), ('eight_byte_position', 8), ('maximum_payload', 254)):
    setattr(GameProtocolTests, 'test_udp_echo_' + name, udp_echo(size))
