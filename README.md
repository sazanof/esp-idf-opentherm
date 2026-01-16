# Installation

`idf.py add-dependency "sazanof/opentherm^1.0.3"`

| Supported Targets | ESP32 | ESP32-C2 | ESP32-C3 | ESP32-C6 | ESP32-H2 | ESP32-S2 | ESP32-S3 |
| ----------------- | ----- | -------- | -------- | -------- | -------- | -------- | -------- |
|                   |   ✓   |    ?     |    ?     |     ?    |     ?    |     ?    |     ?    |

# ESP-IDF Opentherm

This component provides an implementation of Opentherm protocol with ESP-IDF. Tested on ESP-IDF > 5 version.

## How to Use Example

Before project configuration and build, be sure to set the correct chip target using `idf.py set-target <chip_name>`.

### Hardware Required

* A development board (e.g., ESP32-S3-DevKitC, ESP32-C6-DevKitC etc.)
* A USB cable for Power supply and programming
* An opentherm adapter

See [Development Boards](https://www.espressif.com/en/products/devkits) for more information about it.

### Configure the Project

Open the project configuration menu (`idf.py menuconfig`).

In the `OpenTherm Configuration` menu:

* Select GPIO in pin
* Select GPIO out pin

### Build and Flash

Run `idf.py -p PORT flash monitor` to build, flash and monitor the project.

(To exit the serial monitor, type ``Ctrl-]``.)

See the [Getting Started Guide](https://docs.espressif.com/projects/esp-idf/en/latest/get-started/index.html) for full steps to configure and use ESP-IDF to build projects.

## Example Output

```text
I (9369) OT: ====== OPENTHERM =====
I (9369) OT: Free heap size before: 293684
I (9369) OT: Central Heating: OFF
I (9369) OT: Hot Water: OFF
I (9369) OT: Flame: OFF
I (9379) OT: Fault: NO
I (9659) OT: Set CH Temp to: 60
I (9929) OT: Set DHW Temp to: 59
I (10199) OT: DHW Temp: 0.0
I (10469) OT: CH Temp: 44.3
I (10739) OT: Slave OT Version: 0.0
I (11009) OT: Slave Version: C07FA308
I (11279) OT: Slave OT Version: 3.0
I (11279) OT: Free heap size after: 293684
I (11279) OT: ====== OPENTHERM =====
```

## Troubleshooting

For any technical queries, please open an [issue](https://github.com/sazanof/esp-idf-opentherm/issues) on GitHub. We will get back to you soon.
