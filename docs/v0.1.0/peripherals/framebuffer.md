## Zenith Framebuffer Peripheral v0.1.0

The framebuffer peripheral exposes a single-buffered framebuffer. It is located within the MMIO range defined by the
architecture memory map.

All framebuffer registers are little-endian 32-bit MMIO registers.

### Memory Map

The v0.1.0 framebuffer device uses the following MMIO address ranges:

| Address range                                     | Region        | Description |
|---------------------------------------------------|---------------|-------------|
| `0xFFFF_FFFF_FFA0_1400` - `0xFFFF_FFFF_FFA1_13FF` | control       | framebuffer control and status registers |
| `0xFFFF_FFFF_FFA1_1400` - `0xFFFF_FFFF_FFFF_FFFF` | pixel buffer  | 1920 x 1080 x 24-bit single framebuffer |

### Display Format

The framebuffer is a fixed 1920 x 1080 linear image. Each pixel is 24 bits and stored in row-major order.

Pixel byte layout:

| Byte offset | Channel |
|-------------|---------|
| 0           | red     |
| 1           | green   |
| 2           | blue    |

The byte address for a pixel is:

```text
pixel_address = 0xFFFF_FFFF_FFA1_1400 + ((y * 1920) + x) * 3
```

where `x` is in the range 0 through 1919 and `y` is in the range 0 through 1079.

### Registers

The control region contains a small pixel buffer controller. It exposes the framebuffer resolution and a combined
status/control register. Since this version of the framebuffer is single-buffered, there is no buffer swapping logic.

| Address                 | Name             | Access | Description |
|-------------------------|------------------|--------|-------------|
| `0xFFFF_FFFF_FFA0_1400` | `RESOLUTION`     | R      | framebuffer width and height |
| `0xFFFF_FFFF_FFA0_1404` | `STATUS_CONTROL` | R/W    | display status when read, display control when written |

#### RESOLUTION

| Bits  | Name     | Description |
|-------|----------|-------------|
| 0-15  | `WIDTH`  | framebuffer width, always 1920 |
| 16-31 | `HEIGHT` | framebuffer height, always 1080 |

#### STATUS_CONTROL

When read, `STATUS_CONTROL` returns display status:

| Bit | Name     | Description |
|-----|----------|-------------|
| 0   | `VSYNC`  | set to 1 during the vertical synchronization interval |
| 1   | `ENABLE` | set to 1 when the framebuffer output is enabled |
| 2-31| reserved | read as zero |

When written, `STATUS_CONTROL` updates display control:

| Bit | Name     | Description |
|-----|----------|-------------|
| 0   | reserved | must be written as zero |
| 1   | `ENABLE` | write 1 to enable framebuffer output, or 0 to disable it |
| 2-31| reserved | must be written as zero |

Software can poll the `VSYNC` bit to synchronize framebuffer writes with display refresh. The `VSYNC` bit is not
latched; it reflects the current display timing state.

### Single-Buffered Behavior

The pixel buffer is single-buffered. Writes to the pixel buffer update the only framebuffer image. The display may read
from the same buffer while software is writing to it, so tearing is possible.

Software that wants to reduce tearing should update the framebuffer while the `VSYNC` bit is set.
