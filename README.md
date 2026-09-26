# OFAL R2S 2026 — Mission Control

## FILES

OFal.ino
Flight computer on the rocket.

MISSION_CONTROL.py
Live Mission Control dashboard.

REQUIREMENTS.txt
Python packages.

## ROCKET CONNECTIONS

### LoRa

NSS  -> D3
DIO0 -> D1
RESET -> D4
SCK -> D8
MISO -> D9
MOSI -> D10

### GPS

GPS TX -> XIAO D7
GPS RX -> XIAO D6
GPS GND -> GND

### BUTTON

Button -> D2
Other side -> GND

## STARTING THE SYSTEM

1. Upload FLIGHT_TRACKER to XIAO.
2. Upload GROUND_STATION to the ground MCU.
3. Connect ground MCU to PC by USB.
4. Install Python packages.
5. Run Mission Control.
6. Select REAL LoRa.
7. Select COM port.
8. Press CONNECT.

## DEMO MODE

DEMO MODE works without hardware.

The rocket marker moves on the map using simulated coordinates.

The demo coordinates are NOT real rocket coordinates.

## REAL MODE

REAL LoRa uses actual GPS coordinates received from the rocket.

The map updates with every received telemetry packet.

## PYTHON

Install:

pip install -r REQUIREMENTS.txt

Run:

streamlit run MISSION_CONTROL.py
