# Six limit-switch inputs

The six normally-open mechanical switches use STM32 pins PE8 through PE13.
They are adjacent on header J3, share GPIOE for an efficient six-bit read, and
leave PA2/PA3 available for a future USART2 motor connection.

## Bit and connector assignment

| Bit | Meaning | STM32 pin | Board header |
| ---: | --- | --- | --- |
| 0 | left | PE8 | J3-19 |
| 1 | right | PE9 | J3-18 |
| 2 | left-up | PE10 | J3-17 |
| 3 | left-down | PE11 | J3-16 |
| 4 | right-up | PE12 | J3-15 |
| 5 | right-down | PE13 | J3-14 |

The stable software mask is displayed as b5..b0, so its visual order is:

    [right-down right-up left-down left-up right left]

A set bit means that switch is pressed. For example, left plus left-up is
000101b (0x05).

## Wiring

Each switch is wired as a dry contact:

    STM32 PE8..PE13 input ---- NO contact
                               switch
    STM32 GND --------------- COM contact

Use the terminals marked COM and NO; leave NC disconnected and insulated if it
exists. The six COM terminals may share a ground wire connected to J3-3 or
J3-4. Do not connect a switch input to 5 V.

The firmware enables the STM32 internal pull-up, so an open switch reads high
and a closed switch reads low. limit_switches.c inverts that electrical level
and applies 20 ms software debounce, producing an active-high six-bit mask.

For a short bench harness the internal pull-ups are sufficient. For the final
machine, where motor wiring can inject noise, fit a 4.7 kOhm to 10 kOhm pull-up
to 3.3 V and a 10 nF to 100 nF capacitor from each input to GND near the STM32,
and route each signal with a ground return. If these are high-current silver
contact switches rather than low-level/gold-contact switches, verify reliable
operation at the small sensing current or use an isolated/conditioned input
board.

## Firmware API

- LimitSwitches_GetRawMask() returns the immediate input state.
- LimitSwitches_GetStableMask() returns the 20 ms debounced state.
- App_GetRuntime()->limit_switch_mask records the current stable six-bit
  mechanism state.

The former CAN1 configuration and PD0/PD1 assignment are no longer part of the
CubeMX or build configuration. USART2 has deliberately not been configured yet;
PA2/PA3 remain free for the separate motor-protocol work.
