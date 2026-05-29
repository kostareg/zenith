<!---
Markdown description for SystemRDL register map.

Don't override. Generated from: keyboard_mmio
  - rdl/keyboard.rdl
-->

## keyboard_mmio address map

- Absolute Address: 0x0
- Base Offset: 0x0
- Size: 0xC

<p>Zenith v0.1.0 keyboard controller control, status, and event registers.</p>

|Offset|  Identifier  |     Name     |
|------|--------------|--------------|
|  0x0 |status_control|Status/Control|
|  0x4 |   key_event  |   Key Event  |
|  0x8 |  fifo_count  |  FIFO Count  |

### status_control register

- Absolute Address: 0x0
- Base Offset: 0x0
- Size: 0x4

<p>Reads return keyboard status. Writes update keyboard control.</p>

|Bits|Identifier|  Access |Reset|Name|
|----|----------|---------|-----|----|
|  0 |   ready  |    r    | 0x0 |  — |
|  1 |   full   |    r    | 0x0 |  — |
|  2 | overflow |rw, woclr| 0x0 |  — |
|  3 |  enable  |    rw   | 0x0 |  — |
|  4 |clear_fifo|    w    | 0x0 |  — |

#### ready field

<p>READY: set to 1 when KEY_EVENT can be read.</p>

#### full field

<p>FULL: set to 1 when the event FIFO is full.</p>

#### overflow field

<p>Read as OVERFLOW: set if one or more key events were dropped. Write 1 as CLR_OVER to clear OVERFLOW.</p>

#### enable field

<p>Read as ENABLED and write as ENABLE. 1 enables the keyboard controller; 0 disables it.</p>

#### clear_fifo field

<p>CLR_FIFO: write 1 to discard all pending key events. Reads return zero.</p>

### key_event register

- Absolute Address: 0x4
- Base Offset: 0x4
- Size: 0x4

<p>Read and pop the oldest pending key event. If READY is 0, reads return zero and do not change controller state.</p>

|Bits|Identifier|Access|Reset|Name|
|----|----------|------|-----|----|
|15:0| key_code |   r  | 0x0 |  — |
| 16 |  pressed |   r  | 0x0 |  — |
| 17 |  repeat  |   r  | 0x0 |  — |

#### key_code field

<p>KEY_CODE: USB HID Keyboard/Keypad usage ID.</p>

#### pressed field

<p>PRESSED: 1 for key press, 0 for key release.</p>

#### repeat field

<p>REPEAT: 1 if the event is an auto-repeat key press.</p>

### fifo_count register

- Absolute Address: 0x8
- Base Offset: 0x8
- Size: 0x4

<p>Number of pending key events in the FIFO. The v0.1.0 FIFO depth is 16 events.</p>

|Bits|Identifier|Access|Reset|Name|
|----|----------|------|-----|----|
| 4:0|   count  |   r  | 0x0 |  — |

#### count field

<p>COUNT: pending key-event count, 0 through 16.</p>
