"""Deterministic STM32 nominal CAN timing. Python 3.9+, standard library only.

Import Request, enumerate_candidates and best_candidates independently of the CLI.
See README.md for the physical assumptions and source references.
"""
from dataclasses import dataclass
import argparse
import heapq
import math
import re
from typing import Iterator


# Prescaler, Seg1, Seg2, SJW maxima; decoded TQ values, not register encodings.
LIMITS = {'fdcan': (512, 256, 128, 128), 'bxcan': (1024, 16, 8, 4)}


@dataclass(frozen=True)
class Request:
    clock_hz: float
    bitrate_hz: float
    peripheral: str = 'fdcan'
    sample_point_percent: float = 87.5
    tolerance_ppm: float = 100
    node_delay_ns: float = 0
    propagation_delay_ns: float = 0
    max_bitrate_error_ppm: float = 0

    def __post_init__(self):
        if self.peripheral not in LIMITS:
            raise ValueError('peripheral must be fdcan or bxcan')
        for name in ('clock_hz', 'bitrate_hz', 'sample_point_percent',
                     'tolerance_ppm', 'node_delay_ns', 'propagation_delay_ns',
                     'max_bitrate_error_ppm'):
            if not math.isfinite(getattr(self, name)):
                raise ValueError(name + ' must be finite')
        if self.clock_hz <= 0 or self.bitrate_hz <= 0:
            raise ValueError('clock and bitrate must be positive')
        if not 0 < self.sample_point_percent < 100:
            raise ValueError('sample point must be between 0 and 100 percent')
        if not 0 <= self.tolerance_ppm < 1000000:
            raise ValueError('tolerance must be in [0, 1000000) ppm')
        if min(self.node_delay_ns, self.propagation_delay_ns,
               self.max_bitrate_error_ppm) < 0:
            raise ValueError('delays and bitrate error budget must be nonnegative')


@dataclass(frozen=True)
class Candidate:
    prescaler: int
    seg1: int
    seg2: int
    sjw: int
    tq_ns: float
    bitrate_hz: float
    bitrate_error_ppm: float
    sample_point_percent: float
    sample_point_error_percent: float
    propagation_tq: int
    phase1_tq: int
    oscillator_limit_ppm: float
    oscillator_margin_ppm: float
    propagation_margin_ns: float
    feasible: bool

    @property
    def total_tq(self):
        return 1 + self.seg1 + self.seg2


def enumerate_candidates(request: Request) -> Iterator[Candidate]:
    """Yield every timing/SJW within the bitrate budget and profile limits.

    Includes physically unsuitable candidates, marked feasible=False. Searching
    total TQ first prunes whole groups without skipping any matching tuple.
    SJW is constrained to <= both segments. Prop/Phase1 are a conceptual split
    of the combined STM32 Seg1, not independently programmed registers.
    """
    max_p, max_s1, max_s2, max_j = LIMITS[request.peripheral]
    delay = 2 * (request.node_delay_ns + request.propagation_delay_ns)
    tolerance = request.tolerance_ppm / 1000000
    for p in range(1, max_p + 1):
        tq = 1e9 * p / request.clock_hz
        # Use the shortest TQ at the fastest allowed oscillator for delay sizing.
        fastest_tq = tq / (1 + tolerance)
        prop = max(1, math.ceil(delay / fastest_tq))
        for total in range(3, 2 + max_s1 + max_s2):
            bitrate = request.clock_hz / (p * total)
            error = (bitrate / request.bitrate_hz - 1) * 1e6
            if abs(error) > request.max_bitrate_error_ppm:
                continue
            for s2 in range(max(1, total - 1 - max_s1), min(max_s2, total - 2) + 1):
                s1 = total - 1 - s2
                phase1 = s1 - prop
                sample = 100 * (1 + s1) / total
                for sjw in range(1, min(max_j, s1, s2) + 1):
                    # Classical CAN oscillator conditions (per-node +/- bound).
                    oscillator_limit = max(0, min(
                        sjw / (20 * total),
                        min(phase1, s2) / (2 * (13 * total - s2)))) * 1e6
                    # Reserve enough Phase1 for SJW and the requested drift.
                    reserve = max(1, sjw, math.ceil(2 * tolerance * (13 * total - s2)))
                    margin = (s1 - reserve) * fastest_tq - delay
                    feasible = (phase1 >= sjw and margin >= 0 and
                                oscillator_limit >= request.tolerance_ppm)
                    yield Candidate(p, s1, s2, sjw, tq, bitrate, error, sample,
                                    sample - request.sample_point_percent,
                                    prop, phase1, oscillator_limit,
                                    oscillator_limit - request.tolerance_ppm,
                                    margin, feasible)


def rank_key(candidate: Candidate):
    """Accuracy first, then oscillator margin, then a 16-TQ tie preference."""
    return (abs(candidate.bitrate_error_ppm),
            round(abs(candidate.sample_point_error_percent), 10),
            -round(candidate.oscillator_margin_ppm, 8),
            abs(candidate.total_tq - 16),
            -candidate.propagation_margin_ns,
            -candidate.sjw, candidate.prescaler, candidate.seg1, candidate.seg2)


def best_candidates(request: Request, count: int = 6):
    """Return at most count feasible candidates in stable priority order.

    Uses bounded memory even for a broad bitrate error budget. Call
    enumerate_candidates directly to inspect all tuples, including rejections.
    """
    if not isinstance(count, int) or count < 1:
        raise ValueError('count must be a positive integer')
    return heapq.nsmallest(count, (c for c in enumerate_candidates(request)
                                  if c.feasible), key=rank_key)


def frequency(text):
    """Parse Hz/bit/s numbers with optional k or M suffix (e.g. 64M, 500k)."""
    match = re.fullmatch(r'\s*([0-9]+(?:\.[0-9]*)?(?:[eE][+-]?[0-9]+)?)\s*([kKmM]?)\s*', text)
    if not match:
        raise argparse.ArgumentTypeError('use a number with optional k/M suffix, e.g. 64M')
    value = float(match[1]) * {'': 1, 'k': 1000, 'm': 1000000}[match[2].lower()]
    if not math.isfinite(value) or value <= 0:
        raise argparse.ArgumentTypeError('frequency must be finite and positive')
    return value


def main(argv=None):
    parser = argparse.ArgumentParser(
        description='STM32 Classical CAN / FDCAN nominal bit-timing calculator.',
        epilog='Exit: 0 = feasible results, 1 = no feasible timing, 2 = bad input. '
               'Does not validate CAN FD data timing/BRS. See tools/can_timing/README.md.')
    parser.add_argument('--peripheral', choices=tuple(LIMITS), default='fdcan')
    parser.add_argument('--clock', required=True, type=frequency,
                        help='CAN clock after any kernel divider, before bit prescaler (Hz, k/M)')
    parser.add_argument('--bitrate', required=True, type=frequency, help='target bit/s, e.g. 1M')
    parser.add_argument('--sample-point', type=float, default=87.5, help='target percent (default 87.5)')
    parser.add_argument('--tolerance-ppm', type=float, default=100,
                        help='worst-case +/- clock tolerance of EACH node (default 100)')
    parser.add_argument('--node-delay-ns', type=float, default=0,
                        help='worst-case TX+RX delay per node in ns (default 0, idealized)')
    parser.add_argument('--propagation-delay-ns', type=float, default=0,
                        help='one-way bus/cable delay in ns (default 0, idealized)')
    parser.add_argument('--max-bitrate-error-ppm', type=float, default=0,
                        help='allowed nominal bitrate error; distinct from oscillator tolerance (default 0)')
    parser.add_argument('--alternatives', type=int, default=5, help='number of alternatives (default 5)')
    args = parser.parse_args(argv)
    try:
        request = Request(args.clock, args.bitrate, args.peripheral, args.sample_point,
                          args.tolerance_ppm, args.node_delay_ns, args.propagation_delay_ns,
                          args.max_bitrate_error_ppm)
        if args.alternatives < 0:
            raise ValueError('alternatives must be nonnegative')
        results = best_candidates(request, args.alternatives + 1)
    except ValueError as exc:
        parser.error(str(exc))
    print(f'{request.peripheral.upper()} nominal timing | clock {request.clock_hz:g} Hz | '
          f'target {request.bitrate_hz:g} bit/s | sample {request.sample_point_percent:g}%')
    print(f'Per-node clock tolerance: +/-{request.tolerance_ppm:g} ppm; '
          f'round-trip delay: {2 * (request.node_delay_ns + request.propagation_delay_ns):g} ns')
    if request.node_delay_ns == 0 or request.propagation_delay_ns == 0:
        print('Zero delay inputs are idealized; enter actual worst-case delays for hardware assessment.')
    if not results:
        print('No feasible timing within the bitrate, oscillator and propagation constraints.\n'
              'Check the clock, delay budget and tolerance; an inexact bitrate needs an explicit error budget.')
        return 1
    best = results[0]
    print('\nBest configuration (decoded CubeMX/HAL values):')
    if request.peripheral == 'fdcan':
        for name, value in (('NominalPrescaler', best.prescaler), ('NominalTimeSeg1', best.seg1),
                            ('NominalTimeSeg2', best.seg2), ('NominalSyncJumpWidth', best.sjw)):
            print(f'  {name} = {value}')
    else:
        print(f'  Prescaler = {best.prescaler}\n  TimeSeg1 = CAN_BS1_{best.seg1}TQ\n'
              f'  TimeSeg2 = CAN_BS2_{best.seg2}TQ\n  SyncJumpWidth = CAN_SJW_{best.sjw}TQ')
    print(f'  TQ = {best.tq_ns:.6f} ns; TQ/bit = {best.total_tq}; '
          f'bit time = {best.tq_ns * best.total_tq:.6f} ns')
    print('\nBest + Alternatives (margins are model estimates):')
    print(' #   Pre  Seg1 Seg2 SJW       bit/s    err ppm     SP%  SPerr pp  osc margin ppm  prop margin ns')
    for i, c in enumerate(results):
        print(f'{i:2} {c.prescaler:5} {c.seg1:5} {c.seg2:4} {c.sjw:3} '
              f'{c.bitrate_hz:11.3f} {c.bitrate_error_ppm:10.3f} '
              f'{c.sample_point_percent:7.3f} {c.sample_point_error_percent:9.3f} '
              f'{c.oscillator_margin_ppm:15.3f} {c.propagation_margin_ns:15.3f}')
    print(f'\nBest oscillator limit: +/-{best.oscillator_limit_ppm:.3f} ppm per node.\n'
          'Rank: bitrate error, sample-point error, oscillator margin, closeness to 16 TQ, propagation margin.\n'
          'Check your exact MCU clock limits and reference manual; CAN FD data phase/BRS is outside this model.')
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
