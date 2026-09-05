# lampk

ESP32-S3 firmware for the KeeMASH lamp relay node. The firmware keeps the
legacy `lam`, `lamech`, `La0` and `La1` application protocol while adding the
shared reliable ESP-MESH core, typed telemetry, remote OTA and active ping.

## ESP32-S3 wiring

| Signal | ESP32-S3 pin | Connection |
|---|---:|---|
| Relay drive | GPIO6 | Board pad marked `6`; 3.3 V-compatible active-high module input |
| Button | GPIO5 | Board pad marked `5`; button to GND with internal pull-up enabled |
| Relay power | 5V | Relay module `VCC` |
| Common ground | GND | ESP32-S3 and relay module grounds together |
| Board power | 5V/VBUS/VIN | Regulated 5V supply; never feed 5V into `3V3` |

The replacement relay uses 3.3 V-compatible active-high logic: GPIO6 LOW is
OFF and GPIO6 HIGH is ON. Firmware preloads OFF before enabling the output
and starts OFF after every reboot. Temporary GPIO probe commands are removed.
High impedance does not isolate a GPIO from external voltage; never apply
5 V to an ESP32-S3 signal pad.

GPIO0, GPIO3, GPIO45 and GPIO46 are strapping pins and are intentionally not
used. GPIO19/GPIO20 remain available for native USB. GPIO26-GPIO37 remain free
for flash/PSRAM compatibility.

## Build

The current board has a 16 MB flash chip and 8 MB embedded octal PSRAM. The
partition table provides two 8128 KB OTA application slots. The project pins
`keemash_mesh_core` to public tag `v0.6.5`. Local Wi-Fi and mesh credentials
belong in ignored configuration; they must never be committed.

```powershell
tools\idf.cmd -ProjectPath "C:\Users\kennet\Desktop\To Git\lampk" reconfigure
tools\idf.cmd -ProjectPath "C:\Users\kennet\Desktop\To Git\lampk" build
```

## License

Apache License 2.0. See `LICENSE`.
