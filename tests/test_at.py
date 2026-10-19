"""Original AT parser behavior and a narrowly scoped DDI repair hypothesis.

These are firmware diagnostics. libmobile has no equivalent internal AT parser.
Expectations concern command acceptance, setting changes, and parser progress.
"""


class ATParserTests:
    def setUp(self):
        self.adapter = self.factory()
        self.addCleanup(self.adapter.close)

    def accepted(self, body, **initial):
        result = self.adapter.command(body, **initial)
        self.assertTrue(result.completed, 'AT parser exceeded 10,000 instructions')
        self.assertEqual(result.error, 0, 'valid command was rejected')
        self.assertEqual(result.success, 1, 'missing success flag')
        self.assertEqual(result.cursor, len(body) + 1, 'terminator was not consumed')
        self.assertEqual(self.adapter.cpu.substitutions, [], 'AT code must run without hooks')
        return result

    def rejected(self, body, **initial):
        result = self.adapter.command(body, **initial)
        self.assertTrue(result.completed, 'AT parser exceeded 10,000 instructions')
        self.assertEqual(result.error, 1, 'invalid command was accepted')
        return result

    def test_empty_body_succeeds(self):
        self.accepted('')

    def test_echo_zero_disables_echo(self):
        self.assertEqual(self.accepted('E0').echo, 0)

    def test_echo_one_enables_echo(self):
        self.assertEqual(self.accepted('E1', echo=0).echo, 1)

    def test_echo_without_number_defaults_to_zero(self):
        self.assertEqual(self.accepted('E').echo, 0)

    def test_lowercase_command_disables_echo(self):
        self.assertEqual(self.accepted('e0').echo, 0)

    def test_spaces_between_commands_are_ignored(self):
        self.assertEqual(self.accepted(' E1 E0 ').echo, 0)

    def test_last_echo_command_wins(self):
        self.assertEqual(self.accepted('E0E1', echo=0).echo, 1)

    def test_leading_zeroes_in_number_are_accepted(self):
        self.assertEqual(self.accepted('E000').echo, 0)

    def test_echo_outside_boolean_range_is_rejected_without_change(self):
        self.assertEqual(self.rejected('E2').echo, 1)

    def test_unknown_command_letter_is_rejected(self):
        self.rejected('C')

    def test_error_stops_later_commands(self):
        """E0 must not run after the unsupported C command."""
        self.assertEqual(self.rejected('CE0').echo, 1)

    def test_quiet_zero_enables_results(self):
        self.assertEqual(self.accepted('Q0', quiet=1).quiet, 0)

    def test_quiet_one_stages_result_suppression(self):
        """The parser stores 0x11; the outer transport handles its transition."""
        self.assertEqual(self.accepted('Q1').quiet, 0x11)

    def test_quiet_one_preserves_existing_suppression(self):
        self.assertEqual(self.accepted('Q1', quiet=1).quiet, 1)

    def test_quiet_two_is_rejected_without_change(self):
        self.assertEqual(self.rejected('Q2').quiet, 0)

    def test_quiet_then_echo_updates_both_settings(self):
        result = self.accepted('Q0E0', quiet=1)
        self.assertEqual((result.quiet, result.echo), (0, 0))

    def test_verbose_zero_selects_numeric_results(self):
        self.assertEqual(self.accepted('V0').verbose, 0)

    def test_verbose_one_selects_text_results(self):
        self.assertEqual(self.accepted('V1', verbose=0).verbose, 1)

    def test_verbose_two_is_rejected_without_change(self):
        """Both dispatch tables route V to a boolean handler, including cdmaOne."""
        self.assertEqual(self.rejected('V2').verbose, 1)

    def test_extended_results_accept_four(self):
        """X, rather than V, reaches cdmaOne's handler with range 0..4."""
        self.assertEqual(self.accepted('X4').extended, 4)

    def test_extended_results_reject_five_without_change(self):
        self.assertEqual(self.rejected('X5').extended, 0)

    def test_echo_does_not_depend_on_caller_stack_word_two(self):
        """In the broken dump, 2 indexes the terminator and silently skips E0."""
        self.assertEqual(self.accepted('E0', caller_stack=2).echo, 0)

    def test_echo_does_not_depend_on_caller_stack_word_sixteen(self):
        self.assertEqual(self.accepted('E0', caller_stack=16).echo, 0)

    def test_command_chain_does_not_depend_on_caller_stack_word_two(self):
        self.assertEqual(self.accepted('Q0E0', caller_stack=2).echo, 0)

    def test_command_chain_does_not_depend_on_caller_stack_word_sixteen(self):
        self.assertEqual(self.accepted('Q0E0', caller_stack=16).echo, 0)
