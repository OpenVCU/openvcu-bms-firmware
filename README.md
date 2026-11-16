# OpenVCU BMS Firmware (`openvcu-bms-firmware`)

This repository contains the main embedded C firmware for the **OpenVCU High-Voltage Battery Management System (BMS) Master Controller**.

## Ecosystem & Related Repositories

* **[OpenVCU Core (`OpenVCU/openvcu`)](https://github.com/OpenVCU/openvcu):** Core Vehicle Control Unit (VCU) firmware and system code.
* **[OpenVCU BMS Hardware (`OpenVCU/openvcu-bms`)](https://github.com/OpenVCU/openvcu-bms):** BMS Master hardware source files, KiCad 10 schematics, PCB layouts, BOMs, and CAD models.
* **[OpenVCU BMS Firmware (`OpenVCU/openvcu-bms-firmware`)](https://github.com/OpenVCU/openvcu-bms-firmware):** Master BMS controller firmware (this repository).

---

## Hardware Architecture Overview

The OpenVCU BMS Master board (`OpenVCU/openvcu-bms`) is designed for high-reliability automotive and electric vehicle battery systems operating between -30C to 70C:

* **MCU:** STMicroelectronics **STM32G474CCTx** (170 MHz Cortex-M4F, 256 KB Flash, CAN-FD, hardware math accelerators).
* **Pack Current & Coulomb Monitoring:** Texas Instruments **INA228AIDGSR** (`driverSWINA228`) ultra-precise 20-bit I2C current/voltage/power monitor with 40-bit hardware Coulomb (`CHARGE`) and Joule (`ENERGY`) accumulation across a **100 µΩ** Vishay `CSM2F-8518-L100J01` shunt (`R29`).
* **High-Voltage Isolation & Sensing:** Dual/Triple **AMC1301** reinforced isolated delta-sigma modulators with **OPA2197** conditioning.
* **isoSPI Daisy-Chain Master:** Dual **LTC6820** reversible isoSPI transceivers (`ISOSPI1` / `ISOSPI2`) with **HM2108NL** pulse transformers for redundant ring battery module topologies.
* **Unified Slave Cell Monitor Support (`driverSWLTC681x`):** Native 15-bit PEC CRC protocol supporting:
  * **LTC6810-1 / LTC6810-2** (6-cell modules)
  * **LTC6804-1 / LTC6804-2** (12-cell modules)
  * **LTC6811-1 / LTC6811-2** (12-cell modules)
  * **LTC6812-1** (15-cell modules)
  * **LTC6813-1 & ADBMS1818** (12- to 18-cell modules)
* **Isolated CAN Interface:** **ISO1042DWVR** isolated CAN/CAN-FD transceiver.
* **Contactor & Precharge Control:** Automotive **LFPAK56 (`BUK9Y40-55B,115`)** low-side drivers, **SOT-23 (`DMN6075S-7`)** small-signal switches, and **SHV12-1A85-78L3K** HV precharge reed relay.

---

## Flash Memory Map & Bootloader Layout

Requires a microcontroller with at least **256 KB of Flash** (standardized on the **STM32G474CCTx**).

* **Main Application Start Address:** `0x08000000`
* **Bootloader Start Address:** `0x08032000`

| Base Address | Page (2 KB Layout) | Size | Description |
| :--- | :--- | :--- | :--- |
| **`0x08000000`** | Page 0 | 2 KB | Startup Code & Vector Table (Main Application) |
| **`0x08000800`** | Page 1 | 2 KB | `Page0` — Non-volatile EEPROM Emulation |
| **`0x08001000`** | Page 2 | 2 KB | `Page1` — Non-volatile EEPROM Emulation |
| **`0x08001800`** | Page 3 – 49 | 94 KB | Main Application Firmware Code |
| **`0x08019000`** | Page 50 – 99 | 100 KB | Staging Buffer for New Application Firmware (OTA/Bootloader Update) |
| **`0x08032000`** | Page 100 – 127 | 56 KB | Bootloader Firmware Space |

> **Hardware Note for `STM32G474CCTx` Flash Pages (`modFlash.h` / `modFlash.c`):**
> On the `STM32G474` (Category 3 device), verify the `DBANK` option bit configuration when erasing EEPROM emulation pages:
> * With `DBANK = 1` (Dual-Bank 2x128 KB mode), Flash pages are **2 KB (`0x800`)**, matching the legacy page offsets directly.
> * With `DBANK = 0` (Single-Bank 256 KB mode), Flash pages are **4 KB (`0x1000`)**, requiring `Page0` (`0x08001000`) and `Page1` (`0x08002000`) to align on 4 KB boundaries.

See `Modules/Inc/modFlash.h` and `Modules/Src/modFlash.c` for full Flash partitioning and EEPROM emulation details.

---

## Credits & Lineage

Originally derived from the open-source DieBieMS / ENNOID-BMS firmware architecture and modernized for the **OpenVCU** platform.
