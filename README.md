# Microbial Fuel Cell Automated Dosing Control

Automated dosing control system for maintaining optimal microbial fuel cell (MFC) operating conditions using Arduino GIGA R1.

## Overview

This system monitors pH, electrical conductivity (EC), and oxidation-reduction potential (ORP) to automatically dose reagents that maintain ideal conditions for microbial activity in a fuel cell.

## Monitored Parameters

| Parameter | Target Range | Sensor Pin |
|-----------|-------------|------------|
| pH | 6.8 - 7.2 | A0 |
| EC | 4.0 - 5.0 mS/cm | A2 |
| ORP | -250 to -150 mV | A1 |
| MFC Voltage | (monitored only) | A3 |

## Dosing Agents

| Pump Pin | Agent | Effect |
|----------|-------|--------|
| D2 | Sodium Bicarbonate | ↑ pH, ↑ EC |
| D3 | Citric Acid | ↓ pH |
| D4 | Deionized Water | ↓ EC, ↑ ORP |
| D5 | Molasses | ↓ ORP, ↑ EC |
| D6 | Urine | ↑ EC, ↑ pH |
| D7 | Spirulina | ↑ EC, ↑ pH |

## Control Logic

The system uses prioritized control with cross-correction:

1. **Critical Safety Check**: If EC > 10.0 mS/cm, all dosing halts to prevent salt toxicity
2. **pH Control** (Priority 1): Sodium bicarbonate or citric acid
3. **EC Control** (Priority 2): Nutrients or dilution
4. **ORP Control** (Priority 3): Oxidation/reduction balance

## Timing

- Sensor readings: Every 5 seconds
- Dosing checks: Every 5 minutes
- Dose duration: 1 second pulse

## Hardware

- Arduino GIGA R1
- pH sensor (analog)
- EC/TDS sensor
- ORP sensor
- 6x peristaltic pumps (12V, active HIGH)

## Calibration

Adjust these constants for your sensors:

```cpp
const float PH_SLOPE     = -5.59047;
const float PH_INTERCEPT = 15.835;
const float EC_SLOPE_MSPERMV = 0.00864;
const float EC_OFFSET_MS     = 0.033;
const float ORP_OFFSET = -1524.0;
```

## Usage

1. Upload `dosing_controller.ino` to Arduino GIGA R1
2. Connect sensors to pins A0-A3
3. Connect pumps to pins D2-D7
4. Open Serial Monitor at 115200 baud
