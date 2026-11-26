# canbus_wrapper

STM32 CAN wrapper library.

## Using the protocol layer on another platform

`kcm_can.h`, `kcm_can_master.*` and `kcm_can_slave.*` don't depend on the
STM32 FDCAN driver except through `can.h`. A platform with a different CAN
driver (e.g. the ESP32-S3 KCM Master with an MCP2518FD over SPI) defines
`KCM_CAN_PORT` and supplies its own `kcm_can_port.h`, providing `myCAN_t`,
`can_send()`, `HAL_StatusTypeDef` (`HAL_OK`, `HAL_ERROR`), `HAL_GetTick()`,
`__weak` and `UNUSED`. It then compiles only the protocol `.c` files it needs,
not `can.c`. STM32 builds don't define `KCM_CAN_PORT` and are unchanged.

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
