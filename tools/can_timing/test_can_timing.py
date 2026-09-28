"""Run with python -m unittest discover -s tools/can_timing -v."""
import contextlib
import io
import unittest

import can_timing as timing


class TimingTests(unittest.TestCase):
    def test_known_clocks_and_all_sjw_choices(self):
        for peripheral in ('fdcan', 'bxcan'):
            for clock, prescaler in ((64000000, 4), (144000000, 9)):
                with self.subTest(peripheral=peripheral, clock=clock):
                    request = timing.Request(clock, 1000000, peripheral=peripheral)
                    candidates = list(timing.enumerate_candidates(request))
                    matches = [c for c in candidates if
                               (c.prescaler, c.seg1, c.seg2) == (prescaler, 13, 2)]
                    self.assertEqual({c.sjw for c in matches}, {1, 2})
                    c = next(c for c in matches if c.sjw == 2)
                    self.assertEqual(c.bitrate_hz, 1000000)
                    self.assertEqual(c.sample_point_percent, 87.5)
                    self.assertEqual(c.tq_ns, 62.5)
                    self.assertAlmostEqual(c.oscillator_limit_ppm, 4854.368932038835)
                    best = timing.best_candidates(request, 1)[0]
                    self.assertEqual((best.prescaler, best.seg1, best.seg2, best.sjw),
                                     (prescaler, 13, 2, 2))

    def test_delay_reserves_phase_segment_and_reports_margins(self):
        request = timing.Request(64000000, 1000000, node_delay_ns=200,
                                 propagation_delay_ns=100, tolerance_ppm=100)
        c = next(c for c in timing.enumerate_candidates(request)
                 if (c.prescaler, c.seg1, c.seg2, c.sjw) == (4, 13, 2, 2))
        self.assertEqual(c.propagation_tq, 10)  # ceil(600 / 62.5)
        self.assertEqual(c.phase1_tq, 3)
        # Fastest clock (+100 ppm) slightly reduces the nominal 87.5 ns margin.
        self.assertAlmostEqual(c.propagation_margin_ns, 87.43125687431257)
        self.assertAlmostEqual(c.oscillator_margin_ppm, 4754.368932038835)
        self.assertTrue(c.feasible)

    def test_excess_delay_and_tolerance_are_rejected(self):
        self.assertEqual(timing.best_candidates(timing.Request(
            64000000, 1000000, node_delay_ns=1000), 5), [])
        self.assertEqual(timing.best_candidates(timing.Request(
            64000000, 1000000, tolerance_ppm=20000), 5), [])

    def test_nonexact_bitrate_requires_explicit_error_budget(self):
        self.assertEqual(timing.best_candidates(timing.Request(64000000, 999000), 1), [])
        c = timing.best_candidates(timing.Request(
            64000000, 999000, max_bitrate_error_ppm=1100), 1)[0]
        self.assertAlmostEqual(c.bitrate_error_ppm, 1001.001001001)

    def test_bxcan_search_matches_independent_brute_force(self):
        request = timing.Request(8000000, 500000, peripheral='bxcan')
        expected = {(p, s1, s2, j) for p in range(1, 1025)
                    for s1 in range(1, 17) for s2 in range(1, 9)
                    if p * (1 + s1 + s2) == 16
                    for j in range(1, min(4, s1, s2) + 1)}
        actual = {(c.prescaler, c.seg1, c.seg2, c.sjw)
                  for c in timing.enumerate_candidates(request)}
        self.assertEqual(actual, expected)

    def test_invalid_inputs_fail_cleanly(self):
        for changes in ({'clock_hz': 0}, {'bitrate_hz': -1},
                        {'clock_hz': float('inf')}, {'sample_point_percent': float('nan')},
                        {'sample_point_percent': 100}, {'sample_point_percent': 0},
                        {'tolerance_ppm': -1}, {'tolerance_ppm': 1000000},
                        {'node_delay_ns': -1}, {'propagation_delay_ns': -1},
                        {'peripheral': 'other'}, {'max_bitrate_error_ppm': -1}):
            values = dict(clock_hz=64000000, bitrate_hz=1000000)
            values.update(changes)
            with self.subTest(changes=changes), self.assertRaises(ValueError):
                list(timing.enumerate_candidates(timing.Request(**values)))

    def test_extended_fdcan_segments_and_prescaler_endpoints(self):
        for peripheral, clock, expected in (
                ('fdcan', 64000000, (512, 13, 2, 2)),
                ('bxcan', 128000000, (1024, 13, 2, 2)),
                ('fdcan', 385000000, (1, 256, 128, 128))):
            rate = 1000000 if expected[0] == 1 else 7812.5
            tuples = {(c.prescaler, c.seg1, c.seg2, c.sjw)
                      for c in timing.enumerate_candidates(timing.Request(
                          clock, rate, peripheral=peripheral))}
            self.assertIn(expected, tuples)

    def test_fast_clock_boundary_and_5000_ppm_requirement(self):
        for ppm, phase1, feasible in ((0, 2, True), (100, 1, False), (5000, 1, False)):
            request = timing.Request(64000000, 1000000, tolerance_ppm=ppm,
                                     node_delay_ns=343.75)
            c = next(c for c in timing.enumerate_candidates(request)
                     if (c.prescaler, c.seg1, c.seg2, c.sjw) == (4, 13, 2, 2))
            self.assertEqual(c.phase1_tq, phase1)
            self.assertEqual(c.feasible, feasible)
        best = timing.best_candidates(timing.Request(
            64000000, 1000000, tolerance_ppm=5000), 1)[0]
        self.assertGreaterEqual(best.oscillator_limit_ppm, 5000)
        self.assertNotEqual(best.sample_point_percent, 87.5)

    def test_cli_bxcan_units_help_and_bad_alternatives(self):
        self.assertEqual(timing.frequency('64e6'), 64000000)
        self.assertEqual(timing.frequency('500k'), 500000)
        with contextlib.redirect_stdout(io.StringIO()) as output:
            self.assertEqual(timing.main(['--clock', '144M', '--bitrate', '1M',
                                          '--peripheral', 'bxcan', '--alternatives', '0']), 0)
        self.assertIn('Prescaler = 9', output.getvalue())
        self.assertIn('CAN_BS1_13TQ', output.getvalue())
        with contextlib.redirect_stdout(io.StringIO()), self.assertRaises(SystemExit) as cm:
            timing.main(['--help'])
        self.assertEqual(cm.exception.code, 0)
        with contextlib.redirect_stderr(io.StringIO()), self.assertRaises(SystemExit) as cm:
            timing.main(['--clock', '64M', '--bitrate', '1M', '--alternatives', '-1'])
        self.assertEqual(cm.exception.code, 2)

    def test_cli_help_results_and_failure_exit_codes(self):
        output = io.StringIO()
        with contextlib.redirect_stdout(output):
            self.assertEqual(timing.main(['--clock', '64M', '--bitrate', '1M']), 0)
        self.assertIn('NominalPrescaler = 4', output.getvalue())
        self.assertIn('Alternatives', output.getvalue())
        with contextlib.redirect_stdout(io.StringIO()):
            self.assertEqual(timing.main(['--clock', '64M', '--bitrate', '1M',
                                          '--node-delay-ns', '1000']), 1)
        with contextlib.redirect_stderr(io.StringIO()), self.assertRaises(SystemExit) as cm:
            timing.main(['--clock', 'nan', '--bitrate', '1M'])
        self.assertEqual(cm.exception.code, 2)


if __name__ == '__main__':
    unittest.main()
