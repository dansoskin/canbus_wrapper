# STM32 CAN bit-timing calculator

Standalone Python 3.9+ utility with no third-party dependencies. It does not
participate in the C build or change the wrapper's configuration at runtime.
Run from the repository root (or use an absolute path to the script).

```console
python tools/can_timing/can_timing.py --clock 64M --bitrate 1M
python tools/can_timing/can_timing.py --peripheral bxcan --clock 144M --bitrate 1M
python tools/can_timing/can_timing.py --clock 64M --bitrate 1M --sample-point 87.5 --tolerance-ppm 100 --node-delay-ns 200 --propagation-delay-ns 100
python tools/can_timing/can_timing.py --help
python -B -m unittest discover -s tools/can_timing -v
```

On Windows, `py -3` can replace `python`. Frequencies accept Hz/bit/s numbers,
scientific notation, and decimal k/M suffixes. Sample point is a percentage,
e.g. `87.5`, not `0.875`. Defaults: FDCAN, 87.5%, +/-100 ppm, zero delays,
exact bitrate, and five alternatives. Zero delays are explicitly idealized.

## Clock and controller profiles

Supply the clock reaching the CAN bit prescaler: for FDCAN, **after** any
kernel/common clock divider. For bxCAN, use its peripheral clock, not an
APB timer's multiplied clock. A 64 MHz input here means the bit prescaler
receives 64 MHz. Clock-tree setup remains the calling project's responsibility.

| Profile | Prescaler | Seg1 | Seg2 | SJW |
| --- | --- | --- | --- | --- |
| `fdcan` nominal | 1..512 | 1..256 | 1..128 | 1..128 |
| `bxcan` | 1..1024 | 1..16 | 1..8 | 1..4 |

SJW also must not exceed either segment. Values are decoded TQ counts;
register fields generally encode count minus one. The CLI prints HAL names
and bxCAN symbolic constants. These profiles follow the ST
[G4 FDCAN HAL](https://github.com/STMicroelectronics/stm32g4xx-hal-driver/blob/master/Inc/stm32g4xx_hal_fdcan.h)
and [F4 bxCAN HAL](https://github.com/STMicroelectronics/stm32f4xx-hal-driver/blob/master/Inc/stm32f4xx_hal_can.h).
Check the exact MCU's reference manual and HAL version for narrower limits
and maximum clock rates. Supporting a mathematical input clock does not certify
that a particular chip can run at that clock.

## Calculation and physical model

With `N = 1 + Seg1 + Seg2`, `TQ = prescaler / clock`, bitrate is
`clock / (prescaler * N)` and sample point is `100 * (1 + Seg1) / N`.

The Classical CAN delay/tolerance model is based on
[NXP AN1798, equations 9–11](https://www.nxp.com/docs/en/application-note/AN1798.pdf):

- Node delay is the worst-case TX plus RX delay of one node, including its
  transceiver and any isolators. Bus propagation delay is **one-way** cable delay.
  Required round-trip delay `D = 2 * (node_delay + propagation_delay)` assumes
  the same worst-case node bound at both ends. If you already have an aggregate
  round-trip measurement, set node delay to half of it and bus delay to zero.
- Tolerance is the worst-case **+/- bound per node**, including temperature,
  ageing and other clock errors; it is not a two-node combined ppm value.
- Let `t = tolerance_ppm / 1e6`. Use fastest-clock TQ `q = TQ / (1+t)`.
  Conceptually split Seg1: `Prop = max(1, ceil(D/q))`, `Phase1 = Seg1 - Prop`.
- Allowed per-node fraction is
  `min(SJW/(20*N), min(Phase1,Seg2)/(2*(13*N-Seg2)))`.
  Require `Phase1 >= SJW` and allowed tolerance >= requested tolerance.

The conceptual split does not add programmable registers. The tool uses STM32's
combined Seg1 limits, not the separate segment limits of the older controller
in AN1798. Margins are engineering estimates for Classical CAN, not a complete
CAN FD/BRS validation. FD data-phase timing, transceiver delay compensation,
signal integrity, jitter and topology effects are outside scope.

Oscillator margin is allowed ppm minus requested ppm. Propagation margin is
`(Seg1 - reserve)*q - D`, where
`reserve = max(1, SJW, ceil(2*t*(13*N-Seg2)))`. This is remaining round-trip
delay capacity while preserving the phase reserve, not simply time until sampling.
Feasible results have nonnegative margins. A zero margin has no spare allowance.

Bitrate error is signed ppm relative to the requested bitrate; sample-point
error is signed percentage points. By default only exact bitrates qualify.
Use `--max-bitrate-error-ppm 1000` to allow an inexact nominal rate. That budget
is independent of oscillator tolerance: if peers use different achieved nominal
bitrates, their mismatch needs a separate network-level assessment.

## Search and ranking

The engine enumerates all profile-legal prescaler/Seg1/Seg2/SJW tuples within
the bitrate budget, including every SJW choice. It prunes totals outside that
budget before enumerating their segments. The CLI selects only feasible tuples
and retains a bounded number of results in memory.

Priority: absolute bitrate error, absolute sample-point error, larger oscillator
margin, closeness to 16 TQ, larger propagation margin, then larger SJW and stable
numeric tie-breakers. Negligible floating-point differences in sample point and
oscillator margin are rounded for ranking. The 16-TQ preference is a convenience
tie-breaker, not a statement that 16 TQ is always physically best. Alternatives
may differ only in SJW.

With default requirements, 64 MHz / 1 Mbit/s selects `(4, 13, 2, 2)` and
144 MHz selects `(9, 13, 2, 2)` in `(prescaler, Seg1, Seg2, SJW)` order.
Both have 62.5 ns TQ, 16 TQ/bit, and 87.5% sampling. Their oscillator limit
is approximately +/-4854.37 ppm; requesting 5000 ppm will favor another timing.

Exit codes: **0** results, **1** no feasible timing, **2** invalid input.
An empty result is reported explicitly; constraints are never silently relaxed.

## Reuse and GUI

From Python, put this directory on your module search path, then:

```python
from can_timing import Request, best_candidates, enumerate_candidates

request = Request(clock_hz=64_000_000, bitrate_hz=1_000_000,
                  tolerance_ppm=100, node_delay_ns=200)
best = best_candidates(request, count=6)
# enumerate_candidates(request) also yields rejected tuples with feasible=False.
```

Keep the CLI as the initial interface: it is easy to version, automate, and use
from any project consuming this submodule. A GUI becomes worthwhile for frequent
interactive comparisons or users unfamiliar with a terminal. It can later call
the same engine; no GUI framework or dependency is needed now.
