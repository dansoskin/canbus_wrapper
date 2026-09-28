# canbus_wrapper

STM32 CAN wrapper library.

## Host utilities

The standalone [CAN bit-timing calculator](tools/can_timing/README.md) supports
STM32 FDCAN nominal timing and bxCAN. It requires Python 3.9+ and no packages.

```console
python tools/can_timing/can_timing.py --clock 64M --bitrate 1M
python tools/can_timing/can_timing.py --help
python -B -m unittest discover -s tools/can_timing -v
```

The utility is independent of the embedded library and can be run from a
submodule checkout without changing any consuming project's build.
