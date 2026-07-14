# Zenith PCBA

This directory contains the schematic and layout of the Zenith printed circuit board and assembly. It is meant to be
opened as a KiCAD project. Most components are from the default KiCAD library. For custom components, see `imports/`.

## Importing EasyEDA parts

In `imports`, run:

```bash
easyeda2kicad --lcsc_id C3446176 --full --output .
```
