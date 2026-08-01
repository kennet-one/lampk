# lampk

ESP32-S3 firmware for the KeeMASH lamp relay node. The firmware keeps the
legacy `lam`, `lamech`, `La0` and `La1` application protocol while adding the
shared reliable ESP-MESH core, typed telemetry, remote OTA and active ping.

## ESP32-S3 wiring

| Signal | ESP32-S3 pin | Connection |
|---|---:|---|
| Relay drive | GPIO4 | Through a transistor or logic-level MOSFET to relay `IN` |
| Button | GPIO5 | Button between GPIO5 and GND; internal pull-up is enabled |
| Relay power | 5V | Relay module `VCC` |
| Common ground | GND | ESP32-S3, transistor and relay module grounds together |
| Board power | 5V/VBUS/VIN | Regulated 5V supply; never feed 5V into `3V3` |

The recommended relay interface is GPIO4 through a 1 kOhm to 2.2 kOhm
resistor into an NPN base (or a logic-level MOSFET gate), with a 10 kOhm
pulldown to ground. Connect the transistor collector/drain to the active-low
relay `IN`, and emitter/source to GND. This keeps the relay off while the
ESP32-S3 is booting and prevents a 5V pull-up on the relay board from reaching
the ESP32-S3 GPIO.

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
