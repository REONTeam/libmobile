# Firmware conformance report

Generated: 2026-10-19T17:20:09+02:00

Library source revision: `0704f56902f23b7ebf05c82c222e0e145e3140b6`.
Library source SHA-256: `176802fa539488bc63ca1fb516601f99bcbadc7ea99665c89cdba4ac0a21f56b`.

| Target | PASS | FAIL | ERROR | SKIP |
| --- | ---: | ---: | ---: | ---: |
| DDI firmware | 75 | 0 | 0 | 0 |
| DDI libmobile | 54 | 21 | 0 | 0 |
| DDI AT original | 7 | 18 | 0 | 0 |
| DDI AT candidate repair | 25 | 0 | 0 | 0 |
| cdmaOne firmware | 75 | 0 | 0 | 0 |
| cdmaOne libmobile | 53 | 22 | 0 | 0 |
| cdmaOne AT original | 25 | 0 | 0 | 0 |

## Scope

The protocol suite tests game-visible 8-bit exchanges, not a complete adapter emulator.
Original firmware runs from seeded RAM, without executing its reset/boot path.
It executes its serial parser, command dispatch, validation,
response framing, and UDP connection bookkeeping. EEPROM driver operations,
phone status/disconnect, and low-level socket calls are controlled substitutes.
Physical serial timing, the original TCP/IP stack, and a real network are not
exercised. Timer events are frozen: pending tests check that polling alone
does not complete a request, not its physical duration.
The separate AT diagnostics execute the original command-body parser and handlers
without firmware function substitutions. They inspect flags, settings, and cursor
progress at an internal boundary, not game-visible response bytes or the complete
phone transport. libmobile has no corresponding internal AT parser, so these
cases are not run against the library. The DDI candidate target flips one bit
in a private ROM copy at file offset 0xC3D0 (5EFF to 7EFF). Original-image results
remain separate. The 10,000-instruction ceiling detects nonreturning cases; it
is not a latency measurement. This repair is strongly supported, but the cause
of the defective byte and the integrity of the whole dump remain unproven.
The supplied Ghidra archive contains exactly the same bytes as these two dumps.
Passing both images corroborates these selected paths, not the entire dump.
Library-only runs use the recorded firmware contracts without requiring ROMs.
A FAIL is a contract difference, including intentional extensions and firmware
quirks; an ERROR is a harness/runtime problem. Neither is silently marked PASS.

## Firmware inputs

- `DDI_SST_39VF400_00-07FFFF.bin`: `3151a0b81177d94a4a186d0b5e3c6659279739f12ee77c5fc4b443a744199386`
- `cdmaOne_SST_39VF400_00-07FFFF.bin`: `3f8eeed8afeb6509f27fb755b4e4d6f90c3579498a9529e3c9efe6ac752a8549`
- DDI AT candidate, in memory only: `75191991d1c5886ce6470eff657f5ffb2204fa1cf5dddf87c52e41c2e3df5ef9`

See [the firmware audit](firmware_audit.md) for candidate classifications and limits.

## Contract differences

### DDI libmobile: config_read_rejects_zero_size

A zero-size read is an error, not an empty success.

```text
Expected: ACK 99; EE: 19 02
Received: ACK 99; 99: 00
```

### DDI libmobile: config_read_requires_started_session



```text
Expected: ACK 99; EE: 19 01
Received: ACK 99; 99: 00 00
```

### DDI libmobile: config_write_rejects_zero_size

An offset without bytes to write is invalid.

```text
Expected: ACK 9A; EE: 1A 02
Received: ACK 9A; 9A: 00 00
```

### DDI libmobile: config_write_requires_started_session



```text
Expected: ACK 9A; EE: 1A 01
Received: ACK 9A; 9A: 00 01
```

### DDI libmobile: empty_mode_request_defaults_to_zero



```text
Expected: ACK 98; 98: (empty)
Received: ACK 98; EE: 18 02
```

### DDI libmobile: invalid_client_id_repeats_adapter_id_without_response



```text
ACK D2, no reply != ACK 8B, no reply
```

### DDI libmobile: length_above_255_is_discarded



```text
ACK 98; 98: (empty) != ACK D2, no reply
```

### DDI libmobile: session_end_waits_for_adapter_timer

With no timer events, repeated polling must not complete session end.

```text
ACK 91; 91: (empty) != ACK 91, no reply
```

### DDI libmobile: start_rejects_alternative_libmobile_greeting

The library's extra greeting is absent from both supplied firmwares.

```text
Expected: ACK 90; EE: 10 02
Received: ACK 90; 90: 45 56 45 52 59 4F 4E 45 20 48 41 50 50 59 20 4D 4F 42 49 4C 45 20 43 4F 4E 4E 45 43 54 49 4F 4E
```

### DDI libmobile: udp_close_rejects_unknown_connection



```text
Expected: ACK A6; EE: 26 00
Received: ACK A6; EE: 26 01
```

### DDI libmobile: udp_close_returns_connection_id



```text
Expected: ACK A6; A6: 00
Received: ACK A6; EE: 26 01
```

### DDI libmobile: udp_close_then_data_reports_invalid_connection



```text
Expected: ACK A6; A6: 00
Received: ACK A6; EE: 26 01
```

### DDI libmobile: udp_first_connection_retains_endpoint_after_second_open



```text
Expected: ACK A5; A5: 00
Received: ACK A5; EE: 25 01
```

### DDI libmobile: udp_open_allocates_two_distinct_connections



```text
Expected: ACK A5; A5: 00
Received: ACK A5; EE: 25 01
```

### DDI libmobile: udp_open_returns_connection_id



```text
Expected: ACK A5; A5: 00
Received: ACK A5; EE: 25 01
```

### DDI libmobile: udp_open_uses_requested_address_and_port

The simulated peer echoes only packets sent to its exact endpoint.

```text
Expected: ACK A5; A5: 00
Received: ACK A5; EE: 25 01
```

### DDI libmobile: udp_receive_does_not_repeat_previous_datagram



```text
Expected: ACK 95; 95: 00
Received: ACK 95, no reply
```

### DDI libmobile: udp_receive_keeps_datagram_tail_for_next_response

A 300-byte datagram fits the adapter buffer but needs two serial replies.

```text
Expected: ACK 95; 95: 00 FE FF 00 01 02 03 04 05 06 07 08 09 0A 0B 0C 0D 0E 0F 10 11 12 13 14 15 16 17 18 19 1A 1B 1C 1D 1E 1F 20 21 22 23 24 25 26 27 28 29 2A 2B
Received: ACK 95, no reply
```

### DDI libmobile: udp_receive_without_data_replies_immediately



```text
Expected: ACK 95; 95: 00
Received: ACK 95, no reply
```

### DDI libmobile: udp_second_connection_keeps_its_own_endpoint



```text
Expected: ACK A5; A5: 00
Received: ACK A5; EE: 25 01
```

### DDI libmobile: udp_send_to_other_endpoint_has_no_peer_echo

Negative control: the peer must not echo a datagram addressed elsewhere.

```text
Expected: ACK A5; A5: 00
Received: ACK A5; EE: 25 01
```

### DDI AT original: command_chain_does_not_depend_on_caller_stack_word_sixteen



```text
False is not true : AT parser exceeded 10,000 instructions
```

### DDI AT original: command_chain_does_not_depend_on_caller_stack_word_two



```text
False is not true : AT parser exceeded 10,000 instructions
```

### DDI AT original: echo_does_not_depend_on_caller_stack_word_sixteen



```text
False is not true : AT parser exceeded 10,000 instructions
```

### DDI AT original: echo_does_not_depend_on_caller_stack_word_two

In the broken dump, 2 indexes the terminator and silently skips E0.

```text
1 != 3 : terminator was not consumed
```

### DDI AT original: echo_one_enables_echo



```text
False is not true : AT parser exceeded 10,000 instructions
```

### DDI AT original: echo_without_number_defaults_to_zero



```text
False is not true : AT parser exceeded 10,000 instructions
```

### DDI AT original: echo_zero_disables_echo



```text
False is not true : AT parser exceeded 10,000 instructions
```

### DDI AT original: extended_results_accept_four

X, rather than V, reaches cdmaOne's handler with range 0..4.

```text
False is not true : AT parser exceeded 10,000 instructions
```

### DDI AT original: last_echo_command_wins



```text
False is not true : AT parser exceeded 10,000 instructions
```

### DDI AT original: leading_zeroes_in_number_are_accepted



```text
False is not true : AT parser exceeded 10,000 instructions
```

### DDI AT original: lowercase_command_disables_echo



```text
False is not true : AT parser exceeded 10,000 instructions
```

### DDI AT original: quiet_one_preserves_existing_suppression



```text
False is not true : AT parser exceeded 10,000 instructions
```

### DDI AT original: quiet_one_stages_result_suppression

The parser stores 0x11; the outer transport handles its transition.

```text
False is not true : AT parser exceeded 10,000 instructions
```

### DDI AT original: quiet_then_echo_updates_both_settings



```text
False is not true : AT parser exceeded 10,000 instructions
```

### DDI AT original: quiet_zero_enables_results



```text
False is not true : AT parser exceeded 10,000 instructions
```

### DDI AT original: spaces_between_commands_are_ignored



```text
False is not true : AT parser exceeded 10,000 instructions
```

### DDI AT original: verbose_one_selects_text_results



```text
False is not true : AT parser exceeded 10,000 instructions
```

### DDI AT original: verbose_zero_selects_numeric_results



```text
False is not true : AT parser exceeded 10,000 instructions
```

### cdmaOne libmobile: config_read_rejects_zero_size

A zero-size read is an error, not an empty success.

```text
Expected: ACK 99; EE: 19 02
Received: ACK 99; 99: 00
```

### cdmaOne libmobile: config_read_requires_started_session



```text
Expected: ACK 99; EE: 19 01
Received: ACK 99; 99: 00 00
```

### cdmaOne libmobile: config_write_rejects_zero_size

An offset without bytes to write is invalid.

```text
Expected: ACK 9A; EE: 1A 02
Received: ACK 9A; 9A: 00 00
```

### cdmaOne libmobile: config_write_requires_started_session



```text
Expected: ACK 9A; EE: 1A 01
Received: ACK 9A; 9A: 00 01
```

### cdmaOne libmobile: empty_mode_request_defaults_to_zero



```text
Expected: ACK 98; 98: (empty)
Received: ACK 98; EE: 18 02
```

### cdmaOne libmobile: invalid_client_id_repeats_adapter_id_without_response



```text
ACK D2, no reply != ACK 89, no reply
```

### cdmaOne libmobile: length_above_255_is_discarded



```text
ACK 98; 98: (empty) != ACK D2, no reply
```

### cdmaOne libmobile: session_end_waits_for_adapter_timer

With no timer events, repeated polling must not complete session end.

```text
ACK 91; 91: (empty) != ACK 91, no reply
```

### cdmaOne libmobile: start_accepts_trailing_byte_and_echoes_only_nintendo



```text
Expected: ACK 90; 90: 4E 49 4E 54 45 4E 44 4F
Received: ACK 90; EE: 10 02
```

### cdmaOne libmobile: start_rejects_alternative_libmobile_greeting

The library's extra greeting is absent from both supplied firmwares.

```text
Expected: ACK 90; EE: 10 02
Received: ACK 90; 90: 45 56 45 52 59 4F 4E 45 20 48 41 50 50 59 20 4D 4F 42 49 4C 45 20 43 4F 4E 4E 45 43 54 49 4F 4E
```

### cdmaOne libmobile: udp_close_rejects_unknown_connection



```text
Expected: ACK A6; EE: 26 00
Received: ACK A6; EE: 26 01
```

### cdmaOne libmobile: udp_close_returns_connection_id



```text
Expected: ACK A6; A6: 00
Received: ACK A6; EE: 26 01
```

### cdmaOne libmobile: udp_close_then_data_reports_invalid_connection



```text
Expected: ACK A6; A6: 00
Received: ACK A6; EE: 26 01
```

### cdmaOne libmobile: udp_first_connection_retains_endpoint_after_second_open



```text
Expected: ACK A5; A5: 00
Received: ACK A5; EE: 25 01
```

### cdmaOne libmobile: udp_open_allocates_two_distinct_connections



```text
Expected: ACK A5; A5: 00
Received: ACK A5; EE: 25 01
```

### cdmaOne libmobile: udp_open_returns_connection_id



```text
Expected: ACK A5; A5: 00
Received: ACK A5; EE: 25 01
```

### cdmaOne libmobile: udp_open_uses_requested_address_and_port

The simulated peer echoes only packets sent to its exact endpoint.

```text
Expected: ACK A5; A5: 00
Received: ACK A5; EE: 25 01
```

### cdmaOne libmobile: udp_receive_does_not_repeat_previous_datagram



```text
Expected: ACK 95; 95: 00
Received: ACK 95, no reply
```

### cdmaOne libmobile: udp_receive_keeps_datagram_tail_for_next_response

A 300-byte datagram fits the adapter buffer but needs two serial replies.

```text
Expected: ACK 95; 95: 00 FE FF 00 01 02 03 04 05 06 07 08 09 0A 0B 0C 0D 0E 0F 10 11 12 13 14 15 16 17 18 19 1A 1B 1C 1D 1E 1F 20 21 22 23 24 25 26 27 28 29 2A 2B
Received: ACK 95, no reply
```

### cdmaOne libmobile: udp_receive_without_data_replies_immediately



```text
Expected: ACK 95; 95: 00
Received: ACK 95, no reply
```

### cdmaOne libmobile: udp_second_connection_keeps_its_own_endpoint



```text
Expected: ACK A5; A5: 00
Received: ACK A5; EE: 25 01
```

### cdmaOne libmobile: udp_send_to_other_endpoint_has_no_peer_echo

Negative control: the peer must not echo a datagram addressed elsewhere.

```text
Expected: ACK A5; A5: 00
Received: ACK A5; EE: 25 01
```

## All results

| Target | Test | Result |
| --- | --- | --- |
| DDI firmware | `alternate_official_client_id_is_accepted` | PASS |
| DDI firmware | `checksum_failure_is_reported_in_packet_ack` | PASS |
| DDI firmware | `config_read_first_byte` | PASS |
| DDI firmware | `config_read_last_byte` | PASS |
| DDI firmware | `config_read_last_maximum_chunk` | PASS |
| DDI firmware | `config_read_maximum_chunk` | PASS |
| DDI firmware | `config_read_rejects_address_overflow` | PASS |
| DDI firmware | `config_read_rejects_chunk_above_128` | PASS |
| DDI firmware | `config_read_rejects_missing_offset_and_size` | PASS |
| DDI firmware | `config_read_rejects_missing_size` | PASS |
| DDI firmware | `config_read_rejects_overflowing_maximum_chunk` | PASS |
| DDI firmware | `config_read_rejects_zero_size` | PASS |
| DDI firmware | `config_read_requires_started_session` | PASS |
| DDI firmware | `config_write_can_be_read_back` | PASS |
| DDI firmware | `config_write_first_byte` | PASS |
| DDI firmware | `config_write_last_byte` | PASS |
| DDI firmware | `config_write_last_maximum_chunk` | PASS |
| DDI firmware | `config_write_maximum_chunk` | PASS |
| DDI firmware | `config_write_preserves_neighboring_bytes` | PASS |
| DDI firmware | `config_write_rejects_address_overflow` | PASS |
| DDI firmware | `config_write_rejects_chunk_above_128` | PASS |
| DDI firmware | `config_write_rejects_missing_offset` | PASS |
| DDI firmware | `config_write_rejects_zero_size` | PASS |
| DDI firmware | `config_write_reports_offset_and_written_size` | PASS |
| DDI firmware | `config_write_requires_started_session` | PASS |
| DDI firmware | `data_requires_connected_call` | PASS |
| DDI firmware | `empty_mode_request_defaults_to_zero` | PASS |
| DDI firmware | `invalid_client_id_repeats_adapter_id_without_response` | PASS |
| DDI firmware | `length_above_255_is_discarded` | PASS |
| DDI firmware | `mode_ignores_trailing_payload` | PASS |
| DDI firmware | `mode_one_acknowledges_in_original_8_bit_mode` | PASS |
| DDI firmware | `mode_rejects_value_two` | PASS |
| DDI firmware | `mode_zero_returns_empty_success` | PASS |
| DDI firmware | `null_command_acknowledges_without_command_reply` | PASS |
| DDI firmware | `packet_magic_inside_config_data_does_not_restart_parser` | PASS |
| DDI firmware | `parser_recovers_after_checksum_failure` | PASS |
| DDI firmware | `reinit_keeps_protocol_session_open` | PASS |
| DDI firmware | `reinit_preserves_config_bytes` | PASS |
| DDI firmware | `reserved_header_byte_is_ignored` | PASS |
| DDI firmware | `session_end_waits_for_adapter_timer` | PASS |
| DDI firmware | `start_accepts_trailing_byte_and_echoes_only_nintendo` | PASS |
| DDI firmware | `start_can_retry_after_invalid_magic` | PASS |
| DDI firmware | `start_echoes_nintendo` | PASS |
| DDI firmware | `start_is_case_sensitive` | PASS |
| DDI firmware | `start_rejects_alternative_libmobile_greeting` | PASS |
| DDI firmware | `start_rejects_empty_payload` | PASS |
| DDI firmware | `start_rejects_second_session` | PASS |
| DDI firmware | `start_rejects_truncated_magic` | PASS |
| DDI firmware | `start_rejects_wrong_magic` | PASS |
| DDI firmware | `sync_ignores_incomplete_magic_and_garbage` | PASS |
| DDI firmware | `tcp_empty_receive_returns_available_server_data` | PASS |
| DDI firmware | `tcp_empty_receive_waits_when_no_server_data_exists` | PASS |
| DDI firmware | `tcp_multiple_positions_use_one_open_connection` | PASS |
| DDI firmware | `tcp_receive_keeps_bytes_beyond_one_response` | PASS |
| DDI firmware | `tcp_send_can_return_already_received_server_data` | PASS |
| DDI firmware | `tcp_send_without_server_data_returns_connection_id` | PASS |
| DDI firmware | `udp_close_rejects_unknown_connection` | PASS |
| DDI firmware | `udp_close_returns_connection_id` | PASS |
| DDI firmware | `udp_close_then_data_reports_invalid_connection` | PASS |
| DDI firmware | `udp_data_rejects_unknown_connection` | PASS |
| DDI firmware | `udp_echo_eight_byte_position` | PASS |
| DDI firmware | `udp_echo_maximum_payload` | PASS |
| DDI firmware | `udp_echo_single_byte` | PASS |
| DDI firmware | `udp_first_connection_retains_endpoint_after_second_open` | PASS |
| DDI firmware | `udp_open_allocates_two_distinct_connections` | PASS |
| DDI firmware | `udp_open_requires_internet_connection` | PASS |
| DDI firmware | `udp_open_returns_connection_id` | PASS |
| DDI firmware | `udp_open_uses_requested_address_and_port` | PASS |
| DDI firmware | `udp_receive_does_not_repeat_previous_datagram` | PASS |
| DDI firmware | `udp_receive_keeps_datagram_tail_for_next_response` | PASS |
| DDI firmware | `udp_receive_returns_server_payload` | PASS |
| DDI firmware | `udp_receive_without_data_replies_immediately` | PASS |
| DDI firmware | `udp_second_connection_keeps_its_own_endpoint` | PASS |
| DDI firmware | `udp_send_to_other_endpoint_has_no_peer_echo` | PASS |
| DDI firmware | `unknown_command_is_rejected_in_packet_ack` | PASS |
| DDI libmobile | `alternate_official_client_id_is_accepted` | PASS |
| DDI libmobile | `checksum_failure_is_reported_in_packet_ack` | PASS |
| DDI libmobile | `config_read_first_byte` | PASS |
| DDI libmobile | `config_read_last_byte` | PASS |
| DDI libmobile | `config_read_last_maximum_chunk` | PASS |
| DDI libmobile | `config_read_maximum_chunk` | PASS |
| DDI libmobile | `config_read_rejects_address_overflow` | PASS |
| DDI libmobile | `config_read_rejects_chunk_above_128` | PASS |
| DDI libmobile | `config_read_rejects_missing_offset_and_size` | PASS |
| DDI libmobile | `config_read_rejects_missing_size` | PASS |
| DDI libmobile | `config_read_rejects_overflowing_maximum_chunk` | PASS |
| DDI libmobile | `config_read_rejects_zero_size` | FAIL |
| DDI libmobile | `config_read_requires_started_session` | FAIL |
| DDI libmobile | `config_write_can_be_read_back` | PASS |
| DDI libmobile | `config_write_first_byte` | PASS |
| DDI libmobile | `config_write_last_byte` | PASS |
| DDI libmobile | `config_write_last_maximum_chunk` | PASS |
| DDI libmobile | `config_write_maximum_chunk` | PASS |
| DDI libmobile | `config_write_preserves_neighboring_bytes` | PASS |
| DDI libmobile | `config_write_rejects_address_overflow` | PASS |
| DDI libmobile | `config_write_rejects_chunk_above_128` | PASS |
| DDI libmobile | `config_write_rejects_missing_offset` | PASS |
| DDI libmobile | `config_write_rejects_zero_size` | FAIL |
| DDI libmobile | `config_write_reports_offset_and_written_size` | PASS |
| DDI libmobile | `config_write_requires_started_session` | FAIL |
| DDI libmobile | `data_requires_connected_call` | PASS |
| DDI libmobile | `empty_mode_request_defaults_to_zero` | FAIL |
| DDI libmobile | `invalid_client_id_repeats_adapter_id_without_response` | FAIL |
| DDI libmobile | `length_above_255_is_discarded` | FAIL |
| DDI libmobile | `mode_ignores_trailing_payload` | PASS |
| DDI libmobile | `mode_one_acknowledges_in_original_8_bit_mode` | PASS |
| DDI libmobile | `mode_rejects_value_two` | PASS |
| DDI libmobile | `mode_zero_returns_empty_success` | PASS |
| DDI libmobile | `null_command_acknowledges_without_command_reply` | PASS |
| DDI libmobile | `packet_magic_inside_config_data_does_not_restart_parser` | PASS |
| DDI libmobile | `parser_recovers_after_checksum_failure` | PASS |
| DDI libmobile | `reinit_keeps_protocol_session_open` | PASS |
| DDI libmobile | `reinit_preserves_config_bytes` | PASS |
| DDI libmobile | `reserved_header_byte_is_ignored` | PASS |
| DDI libmobile | `session_end_waits_for_adapter_timer` | FAIL |
| DDI libmobile | `start_accepts_trailing_byte_and_echoes_only_nintendo` | PASS |
| DDI libmobile | `start_can_retry_after_invalid_magic` | PASS |
| DDI libmobile | `start_echoes_nintendo` | PASS |
| DDI libmobile | `start_is_case_sensitive` | PASS |
| DDI libmobile | `start_rejects_alternative_libmobile_greeting` | FAIL |
| DDI libmobile | `start_rejects_empty_payload` | PASS |
| DDI libmobile | `start_rejects_second_session` | PASS |
| DDI libmobile | `start_rejects_truncated_magic` | PASS |
| DDI libmobile | `start_rejects_wrong_magic` | PASS |
| DDI libmobile | `sync_ignores_incomplete_magic_and_garbage` | PASS |
| DDI libmobile | `tcp_empty_receive_returns_available_server_data` | PASS |
| DDI libmobile | `tcp_empty_receive_waits_when_no_server_data_exists` | PASS |
| DDI libmobile | `tcp_multiple_positions_use_one_open_connection` | PASS |
| DDI libmobile | `tcp_receive_keeps_bytes_beyond_one_response` | PASS |
| DDI libmobile | `tcp_send_can_return_already_received_server_data` | PASS |
| DDI libmobile | `tcp_send_without_server_data_returns_connection_id` | PASS |
| DDI libmobile | `udp_close_rejects_unknown_connection` | FAIL |
| DDI libmobile | `udp_close_returns_connection_id` | FAIL |
| DDI libmobile | `udp_close_then_data_reports_invalid_connection` | FAIL |
| DDI libmobile | `udp_data_rejects_unknown_connection` | PASS |
| DDI libmobile | `udp_echo_eight_byte_position` | PASS |
| DDI libmobile | `udp_echo_maximum_payload` | PASS |
| DDI libmobile | `udp_echo_single_byte` | PASS |
| DDI libmobile | `udp_first_connection_retains_endpoint_after_second_open` | FAIL |
| DDI libmobile | `udp_open_allocates_two_distinct_connections` | FAIL |
| DDI libmobile | `udp_open_requires_internet_connection` | PASS |
| DDI libmobile | `udp_open_returns_connection_id` | FAIL |
| DDI libmobile | `udp_open_uses_requested_address_and_port` | FAIL |
| DDI libmobile | `udp_receive_does_not_repeat_previous_datagram` | FAIL |
| DDI libmobile | `udp_receive_keeps_datagram_tail_for_next_response` | FAIL |
| DDI libmobile | `udp_receive_returns_server_payload` | PASS |
| DDI libmobile | `udp_receive_without_data_replies_immediately` | FAIL |
| DDI libmobile | `udp_second_connection_keeps_its_own_endpoint` | FAIL |
| DDI libmobile | `udp_send_to_other_endpoint_has_no_peer_echo` | FAIL |
| DDI libmobile | `unknown_command_is_rejected_in_packet_ack` | PASS |
| DDI AT original | `command_chain_does_not_depend_on_caller_stack_word_sixteen` | FAIL |
| DDI AT original | `command_chain_does_not_depend_on_caller_stack_word_two` | FAIL |
| DDI AT original | `echo_does_not_depend_on_caller_stack_word_sixteen` | FAIL |
| DDI AT original | `echo_does_not_depend_on_caller_stack_word_two` | FAIL |
| DDI AT original | `echo_one_enables_echo` | FAIL |
| DDI AT original | `echo_outside_boolean_range_is_rejected_without_change` | PASS |
| DDI AT original | `echo_without_number_defaults_to_zero` | FAIL |
| DDI AT original | `echo_zero_disables_echo` | FAIL |
| DDI AT original | `empty_body_succeeds` | PASS |
| DDI AT original | `error_stops_later_commands` | PASS |
| DDI AT original | `extended_results_accept_four` | FAIL |
| DDI AT original | `extended_results_reject_five_without_change` | PASS |
| DDI AT original | `last_echo_command_wins` | FAIL |
| DDI AT original | `leading_zeroes_in_number_are_accepted` | FAIL |
| DDI AT original | `lowercase_command_disables_echo` | FAIL |
| DDI AT original | `quiet_one_preserves_existing_suppression` | FAIL |
| DDI AT original | `quiet_one_stages_result_suppression` | FAIL |
| DDI AT original | `quiet_then_echo_updates_both_settings` | FAIL |
| DDI AT original | `quiet_two_is_rejected_without_change` | PASS |
| DDI AT original | `quiet_zero_enables_results` | FAIL |
| DDI AT original | `spaces_between_commands_are_ignored` | FAIL |
| DDI AT original | `unknown_command_letter_is_rejected` | PASS |
| DDI AT original | `verbose_one_selects_text_results` | FAIL |
| DDI AT original | `verbose_two_is_rejected_without_change` | PASS |
| DDI AT original | `verbose_zero_selects_numeric_results` | FAIL |
| DDI AT candidate repair | `command_chain_does_not_depend_on_caller_stack_word_sixteen` | PASS |
| DDI AT candidate repair | `command_chain_does_not_depend_on_caller_stack_word_two` | PASS |
| DDI AT candidate repair | `echo_does_not_depend_on_caller_stack_word_sixteen` | PASS |
| DDI AT candidate repair | `echo_does_not_depend_on_caller_stack_word_two` | PASS |
| DDI AT candidate repair | `echo_one_enables_echo` | PASS |
| DDI AT candidate repair | `echo_outside_boolean_range_is_rejected_without_change` | PASS |
| DDI AT candidate repair | `echo_without_number_defaults_to_zero` | PASS |
| DDI AT candidate repair | `echo_zero_disables_echo` | PASS |
| DDI AT candidate repair | `empty_body_succeeds` | PASS |
| DDI AT candidate repair | `error_stops_later_commands` | PASS |
| DDI AT candidate repair | `extended_results_accept_four` | PASS |
| DDI AT candidate repair | `extended_results_reject_five_without_change` | PASS |
| DDI AT candidate repair | `last_echo_command_wins` | PASS |
| DDI AT candidate repair | `leading_zeroes_in_number_are_accepted` | PASS |
| DDI AT candidate repair | `lowercase_command_disables_echo` | PASS |
| DDI AT candidate repair | `quiet_one_preserves_existing_suppression` | PASS |
| DDI AT candidate repair | `quiet_one_stages_result_suppression` | PASS |
| DDI AT candidate repair | `quiet_then_echo_updates_both_settings` | PASS |
| DDI AT candidate repair | `quiet_two_is_rejected_without_change` | PASS |
| DDI AT candidate repair | `quiet_zero_enables_results` | PASS |
| DDI AT candidate repair | `spaces_between_commands_are_ignored` | PASS |
| DDI AT candidate repair | `unknown_command_letter_is_rejected` | PASS |
| DDI AT candidate repair | `verbose_one_selects_text_results` | PASS |
| DDI AT candidate repair | `verbose_two_is_rejected_without_change` | PASS |
| DDI AT candidate repair | `verbose_zero_selects_numeric_results` | PASS |
| cdmaOne firmware | `alternate_official_client_id_is_accepted` | PASS |
| cdmaOne firmware | `checksum_failure_is_reported_in_packet_ack` | PASS |
| cdmaOne firmware | `config_read_first_byte` | PASS |
| cdmaOne firmware | `config_read_last_byte` | PASS |
| cdmaOne firmware | `config_read_last_maximum_chunk` | PASS |
| cdmaOne firmware | `config_read_maximum_chunk` | PASS |
| cdmaOne firmware | `config_read_rejects_address_overflow` | PASS |
| cdmaOne firmware | `config_read_rejects_chunk_above_128` | PASS |
| cdmaOne firmware | `config_read_rejects_missing_offset_and_size` | PASS |
| cdmaOne firmware | `config_read_rejects_missing_size` | PASS |
| cdmaOne firmware | `config_read_rejects_overflowing_maximum_chunk` | PASS |
| cdmaOne firmware | `config_read_rejects_zero_size` | PASS |
| cdmaOne firmware | `config_read_requires_started_session` | PASS |
| cdmaOne firmware | `config_write_can_be_read_back` | PASS |
| cdmaOne firmware | `config_write_first_byte` | PASS |
| cdmaOne firmware | `config_write_last_byte` | PASS |
| cdmaOne firmware | `config_write_last_maximum_chunk` | PASS |
| cdmaOne firmware | `config_write_maximum_chunk` | PASS |
| cdmaOne firmware | `config_write_preserves_neighboring_bytes` | PASS |
| cdmaOne firmware | `config_write_rejects_address_overflow` | PASS |
| cdmaOne firmware | `config_write_rejects_chunk_above_128` | PASS |
| cdmaOne firmware | `config_write_rejects_missing_offset` | PASS |
| cdmaOne firmware | `config_write_rejects_zero_size` | PASS |
| cdmaOne firmware | `config_write_reports_offset_and_written_size` | PASS |
| cdmaOne firmware | `config_write_requires_started_session` | PASS |
| cdmaOne firmware | `data_requires_connected_call` | PASS |
| cdmaOne firmware | `empty_mode_request_defaults_to_zero` | PASS |
| cdmaOne firmware | `invalid_client_id_repeats_adapter_id_without_response` | PASS |
| cdmaOne firmware | `length_above_255_is_discarded` | PASS |
| cdmaOne firmware | `mode_ignores_trailing_payload` | PASS |
| cdmaOne firmware | `mode_one_acknowledges_in_original_8_bit_mode` | PASS |
| cdmaOne firmware | `mode_rejects_value_two` | PASS |
| cdmaOne firmware | `mode_zero_returns_empty_success` | PASS |
| cdmaOne firmware | `null_command_acknowledges_without_command_reply` | PASS |
| cdmaOne firmware | `packet_magic_inside_config_data_does_not_restart_parser` | PASS |
| cdmaOne firmware | `parser_recovers_after_checksum_failure` | PASS |
| cdmaOne firmware | `reinit_keeps_protocol_session_open` | PASS |
| cdmaOne firmware | `reinit_preserves_config_bytes` | PASS |
| cdmaOne firmware | `reserved_header_byte_is_ignored` | PASS |
| cdmaOne firmware | `session_end_waits_for_adapter_timer` | PASS |
| cdmaOne firmware | `start_accepts_trailing_byte_and_echoes_only_nintendo` | PASS |
| cdmaOne firmware | `start_can_retry_after_invalid_magic` | PASS |
| cdmaOne firmware | `start_echoes_nintendo` | PASS |
| cdmaOne firmware | `start_is_case_sensitive` | PASS |
| cdmaOne firmware | `start_rejects_alternative_libmobile_greeting` | PASS |
| cdmaOne firmware | `start_rejects_empty_payload` | PASS |
| cdmaOne firmware | `start_rejects_second_session` | PASS |
| cdmaOne firmware | `start_rejects_truncated_magic` | PASS |
| cdmaOne firmware | `start_rejects_wrong_magic` | PASS |
| cdmaOne firmware | `sync_ignores_incomplete_magic_and_garbage` | PASS |
| cdmaOne firmware | `tcp_empty_receive_returns_available_server_data` | PASS |
| cdmaOne firmware | `tcp_empty_receive_waits_when_no_server_data_exists` | PASS |
| cdmaOne firmware | `tcp_multiple_positions_use_one_open_connection` | PASS |
| cdmaOne firmware | `tcp_receive_keeps_bytes_beyond_one_response` | PASS |
| cdmaOne firmware | `tcp_send_can_return_already_received_server_data` | PASS |
| cdmaOne firmware | `tcp_send_without_server_data_returns_connection_id` | PASS |
| cdmaOne firmware | `udp_close_rejects_unknown_connection` | PASS |
| cdmaOne firmware | `udp_close_returns_connection_id` | PASS |
| cdmaOne firmware | `udp_close_then_data_reports_invalid_connection` | PASS |
| cdmaOne firmware | `udp_data_rejects_unknown_connection` | PASS |
| cdmaOne firmware | `udp_echo_eight_byte_position` | PASS |
| cdmaOne firmware | `udp_echo_maximum_payload` | PASS |
| cdmaOne firmware | `udp_echo_single_byte` | PASS |
| cdmaOne firmware | `udp_first_connection_retains_endpoint_after_second_open` | PASS |
| cdmaOne firmware | `udp_open_allocates_two_distinct_connections` | PASS |
| cdmaOne firmware | `udp_open_requires_internet_connection` | PASS |
| cdmaOne firmware | `udp_open_returns_connection_id` | PASS |
| cdmaOne firmware | `udp_open_uses_requested_address_and_port` | PASS |
| cdmaOne firmware | `udp_receive_does_not_repeat_previous_datagram` | PASS |
| cdmaOne firmware | `udp_receive_keeps_datagram_tail_for_next_response` | PASS |
| cdmaOne firmware | `udp_receive_returns_server_payload` | PASS |
| cdmaOne firmware | `udp_receive_without_data_replies_immediately` | PASS |
| cdmaOne firmware | `udp_second_connection_keeps_its_own_endpoint` | PASS |
| cdmaOne firmware | `udp_send_to_other_endpoint_has_no_peer_echo` | PASS |
| cdmaOne firmware | `unknown_command_is_rejected_in_packet_ack` | PASS |
| cdmaOne libmobile | `alternate_official_client_id_is_accepted` | PASS |
| cdmaOne libmobile | `checksum_failure_is_reported_in_packet_ack` | PASS |
| cdmaOne libmobile | `config_read_first_byte` | PASS |
| cdmaOne libmobile | `config_read_last_byte` | PASS |
| cdmaOne libmobile | `config_read_last_maximum_chunk` | PASS |
| cdmaOne libmobile | `config_read_maximum_chunk` | PASS |
| cdmaOne libmobile | `config_read_rejects_address_overflow` | PASS |
| cdmaOne libmobile | `config_read_rejects_chunk_above_128` | PASS |
| cdmaOne libmobile | `config_read_rejects_missing_offset_and_size` | PASS |
| cdmaOne libmobile | `config_read_rejects_missing_size` | PASS |
| cdmaOne libmobile | `config_read_rejects_overflowing_maximum_chunk` | PASS |
| cdmaOne libmobile | `config_read_rejects_zero_size` | FAIL |
| cdmaOne libmobile | `config_read_requires_started_session` | FAIL |
| cdmaOne libmobile | `config_write_can_be_read_back` | PASS |
| cdmaOne libmobile | `config_write_first_byte` | PASS |
| cdmaOne libmobile | `config_write_last_byte` | PASS |
| cdmaOne libmobile | `config_write_last_maximum_chunk` | PASS |
| cdmaOne libmobile | `config_write_maximum_chunk` | PASS |
| cdmaOne libmobile | `config_write_preserves_neighboring_bytes` | PASS |
| cdmaOne libmobile | `config_write_rejects_address_overflow` | PASS |
| cdmaOne libmobile | `config_write_rejects_chunk_above_128` | PASS |
| cdmaOne libmobile | `config_write_rejects_missing_offset` | PASS |
| cdmaOne libmobile | `config_write_rejects_zero_size` | FAIL |
| cdmaOne libmobile | `config_write_reports_offset_and_written_size` | PASS |
| cdmaOne libmobile | `config_write_requires_started_session` | FAIL |
| cdmaOne libmobile | `data_requires_connected_call` | PASS |
| cdmaOne libmobile | `empty_mode_request_defaults_to_zero` | FAIL |
| cdmaOne libmobile | `invalid_client_id_repeats_adapter_id_without_response` | FAIL |
| cdmaOne libmobile | `length_above_255_is_discarded` | FAIL |
| cdmaOne libmobile | `mode_ignores_trailing_payload` | PASS |
| cdmaOne libmobile | `mode_one_acknowledges_in_original_8_bit_mode` | PASS |
| cdmaOne libmobile | `mode_rejects_value_two` | PASS |
| cdmaOne libmobile | `mode_zero_returns_empty_success` | PASS |
| cdmaOne libmobile | `null_command_acknowledges_without_command_reply` | PASS |
| cdmaOne libmobile | `packet_magic_inside_config_data_does_not_restart_parser` | PASS |
| cdmaOne libmobile | `parser_recovers_after_checksum_failure` | PASS |
| cdmaOne libmobile | `reinit_keeps_protocol_session_open` | PASS |
| cdmaOne libmobile | `reinit_preserves_config_bytes` | PASS |
| cdmaOne libmobile | `reserved_header_byte_is_ignored` | PASS |
| cdmaOne libmobile | `session_end_waits_for_adapter_timer` | FAIL |
| cdmaOne libmobile | `start_accepts_trailing_byte_and_echoes_only_nintendo` | FAIL |
| cdmaOne libmobile | `start_can_retry_after_invalid_magic` | PASS |
| cdmaOne libmobile | `start_echoes_nintendo` | PASS |
| cdmaOne libmobile | `start_is_case_sensitive` | PASS |
| cdmaOne libmobile | `start_rejects_alternative_libmobile_greeting` | FAIL |
| cdmaOne libmobile | `start_rejects_empty_payload` | PASS |
| cdmaOne libmobile | `start_rejects_second_session` | PASS |
| cdmaOne libmobile | `start_rejects_truncated_magic` | PASS |
| cdmaOne libmobile | `start_rejects_wrong_magic` | PASS |
| cdmaOne libmobile | `sync_ignores_incomplete_magic_and_garbage` | PASS |
| cdmaOne libmobile | `tcp_empty_receive_returns_available_server_data` | PASS |
| cdmaOne libmobile | `tcp_empty_receive_waits_when_no_server_data_exists` | PASS |
| cdmaOne libmobile | `tcp_multiple_positions_use_one_open_connection` | PASS |
| cdmaOne libmobile | `tcp_receive_keeps_bytes_beyond_one_response` | PASS |
| cdmaOne libmobile | `tcp_send_can_return_already_received_server_data` | PASS |
| cdmaOne libmobile | `tcp_send_without_server_data_returns_connection_id` | PASS |
| cdmaOne libmobile | `udp_close_rejects_unknown_connection` | FAIL |
| cdmaOne libmobile | `udp_close_returns_connection_id` | FAIL |
| cdmaOne libmobile | `udp_close_then_data_reports_invalid_connection` | FAIL |
| cdmaOne libmobile | `udp_data_rejects_unknown_connection` | PASS |
| cdmaOne libmobile | `udp_echo_eight_byte_position` | PASS |
| cdmaOne libmobile | `udp_echo_maximum_payload` | PASS |
| cdmaOne libmobile | `udp_echo_single_byte` | PASS |
| cdmaOne libmobile | `udp_first_connection_retains_endpoint_after_second_open` | FAIL |
| cdmaOne libmobile | `udp_open_allocates_two_distinct_connections` | FAIL |
| cdmaOne libmobile | `udp_open_requires_internet_connection` | PASS |
| cdmaOne libmobile | `udp_open_returns_connection_id` | FAIL |
| cdmaOne libmobile | `udp_open_uses_requested_address_and_port` | FAIL |
| cdmaOne libmobile | `udp_receive_does_not_repeat_previous_datagram` | FAIL |
| cdmaOne libmobile | `udp_receive_keeps_datagram_tail_for_next_response` | FAIL |
| cdmaOne libmobile | `udp_receive_returns_server_payload` | PASS |
| cdmaOne libmobile | `udp_receive_without_data_replies_immediately` | FAIL |
| cdmaOne libmobile | `udp_second_connection_keeps_its_own_endpoint` | FAIL |
| cdmaOne libmobile | `udp_send_to_other_endpoint_has_no_peer_echo` | FAIL |
| cdmaOne libmobile | `unknown_command_is_rejected_in_packet_ack` | PASS |
| cdmaOne AT original | `command_chain_does_not_depend_on_caller_stack_word_sixteen` | PASS |
| cdmaOne AT original | `command_chain_does_not_depend_on_caller_stack_word_two` | PASS |
| cdmaOne AT original | `echo_does_not_depend_on_caller_stack_word_sixteen` | PASS |
| cdmaOne AT original | `echo_does_not_depend_on_caller_stack_word_two` | PASS |
| cdmaOne AT original | `echo_one_enables_echo` | PASS |
| cdmaOne AT original | `echo_outside_boolean_range_is_rejected_without_change` | PASS |
| cdmaOne AT original | `echo_without_number_defaults_to_zero` | PASS |
| cdmaOne AT original | `echo_zero_disables_echo` | PASS |
| cdmaOne AT original | `empty_body_succeeds` | PASS |
| cdmaOne AT original | `error_stops_later_commands` | PASS |
| cdmaOne AT original | `extended_results_accept_four` | PASS |
| cdmaOne AT original | `extended_results_reject_five_without_change` | PASS |
| cdmaOne AT original | `last_echo_command_wins` | PASS |
| cdmaOne AT original | `leading_zeroes_in_number_are_accepted` | PASS |
| cdmaOne AT original | `lowercase_command_disables_echo` | PASS |
| cdmaOne AT original | `quiet_one_preserves_existing_suppression` | PASS |
| cdmaOne AT original | `quiet_one_stages_result_suppression` | PASS |
| cdmaOne AT original | `quiet_then_echo_updates_both_settings` | PASS |
| cdmaOne AT original | `quiet_two_is_rejected_without_change` | PASS |
| cdmaOne AT original | `quiet_zero_enables_results` | PASS |
| cdmaOne AT original | `spaces_between_commands_are_ignored` | PASS |
| cdmaOne AT original | `unknown_command_letter_is_rejected` | PASS |
| cdmaOne AT original | `verbose_one_selects_text_results` | PASS |
| cdmaOne AT original | `verbose_two_is_rejected_without_change` | PASS |
| cdmaOne AT original | `verbose_zero_selects_numeric_results` | PASS |
