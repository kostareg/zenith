## Zenith Keyboard Peripheral v0.1.0

The keyboard peripheral exposes a pollable key-event FIFO. Software reads key events from MMIO registers.

The hardware controller is responsible for receiving keyboard protocol data, translating it into key events, and
pushing those events into the FIFO.

All keyboard registers are little-endian 32-bit MMIO registers.

### Memory Map

The v0.1.0 keyboard device uses the following MMIO address range:

| Address range                                     | Region  | Description |
|---------------------------------------------------|---------|-------------|
| `0xFFFF_FFFF_FFA0_0000` - `0xFFFF_FFFF_FFA0_0FFF` | control | keyboard control, status, and event registers |

### Registers

| Address                 | Name             | Access | Description |
|-------------------------|------------------|--------|-------------|
| `0xFFFF_FFFF_FFA0_0000` | `STATUS_CONTROL` | R/W    | keyboard status when read, keyboard control when written |
| `0xFFFF_FFFF_FFA0_0004` | `KEY_EVENT`      | R      | read and pop the oldest pending key event |
| `0xFFFF_FFFF_FFA0_0008` | `FIFO_COUNT`     | R      | number of pending key events |

#### STATUS_CONTROL

When read, `STATUS_CONTROL` returns keyboard status:

| Bit | Name       | Description |
|-----|------------|-------------|
| 0   | `READY`    | set to 1 when `KEY_EVENT` can be read |
| 1   | `FULL`     | set to 1 when the event FIFO is full |
| 2   | `OVERFLOW` | set to 1 if one or more key events were dropped |
| 3   | `ENABLED`  | set to 1 when the keyboard controller is enabled |
| 4-31| reserved   | read as zero |

When written, `STATUS_CONTROL` updates keyboard control:

| Bit | Name       | Description |
|-----|------------|-------------|
| 0   | reserved   | must be written as zero |
| 1   | reserved   | must be written as zero |
| 2   | `CLR_OVER` | write 1 to clear `OVERFLOW` |
| 3   | `ENABLE`   | write 1 to enable the keyboard controller, or 0 to disable it |
| 4   | `CLR_FIFO` | write 1 to discard all pending key events |
| 5-31| reserved   | must be written as zero |

Disabling the keyboard controller stops new events from being pushed into the FIFO. It does not clear pending events
unless `CLR_FIFO` is also written.

#### KEY_EVENT

Reading `KEY_EVENT` returns and removes the oldest pending event from the FIFO. If `READY` is zero, reading `KEY_EVENT`
returns zero and does not change controller state.

| Bits  | Name       | Description |
|-------|------------|-------------|
| 0-15  | `KEY_CODE` | key code |
| 16    | `PRESSED`  | 1 for key press, 0 for key release |
| 17    | `REPEAT`   | 1 if the event is an auto-repeat key press |
| 18-31 | reserved   | read as zero |

Key codes are USB HID Keyboard/Keypad usage IDs. For example, `0x04` is `A`, `0x05` is `B`, `0x1E` is `1`, `0x28` is
Enter, and `0x2C` is Space.

#### FIFO_COUNT

`FIFO_COUNT` returns the number of pending key events in the FIFO. The v0.1.0 FIFO depth is 16 events.

### Polling

Software can poll for keyboard input by repeatedly reading `STATUS_CONTROL`. When `READY` is set, software reads
`KEY_EVENT` to consume the next key event.

```text
status = load32(0xFFFF_FFFF_FFA0_0000)
if status & 1:
    event = load32(0xFFFF_FFFF_FFA0_0004)
```
