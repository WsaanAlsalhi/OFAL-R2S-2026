/*
  ============================================================
  OFAL R2S 2026 - PATRICK
  FLIGHT TELEMETRY COMPUTER
  ============================================================

  MCU: Seeed Studio XIAO nRF52840 Sense

  GPS: BN-220 @ 9600
  LoRa: SX1276 @ 868 MHz

  ============================================================
  PINOUT
  ============================================================

  GPS:
    TX  -> D7 (RX)
    RX  -> D6 (TX)
    VCC -> 3.3V
    GND -> GND

  SX1276:
    NSS  -> D3
    DIO0 -> D1
    RES  -> D4
    SCK  -> D8
    MISO -> D9
    MOSI -> D10
    VCC  -> 3.3V
    GND  -> GND

  POWER:
    Battery (+) -> Switch -> XIAO 3.3V
    Battery (-) -> XIAO GND

  ============================================================
*/

#include <Arduino.h>
#include <SPI.h>
#include <LoRa.h>

#define LORA_SS     D3
#define LORA_DIO0   D1
#define LORA_RESET  D4
#define GPS_SERIAL  Serial1
#define GPS_BAUD    9600

// ---------- LORA ----------
#define LORA_FREQUENCY        868E6
#define LORA_SPREADING_FACTOR 10
#define LORA_BANDWIDTH        125E3
#define LORA_CODING_RATE      8
#define LORA_SYNC_WORD        0x12
#define LORA_TX_POWER         17

// ---------- TELEMETRY ----------
#define TELEMETRY_INTERVAL 1000
unsigned long lastTelemetry = 0;
unsigned long packetNumber = 0;

// ---------- GPS ----------
double latitude   = 0.0;
double longitude  = 0.0;
double altitude   = 0.0;
double speedKmh   = 0.0;
int    satellites = 0;
bool   gpsFix     = false;
String nmeaBuffer = "";


// ============================================================
// SETUP
// ============================================================
void setup() {

  // Serial
  Serial.begin(115200);
  delay(1000);

  Serial.println();
  Serial.println("============================================");
  Serial.println("   OFAL R2S 2026 - PATRICK");
  Serial.println("============================================");

  // GPS
  pinMode(D6, INPUT);
  pinMode(D7, INPUT);

  GPS_SERIAL.begin(GPS_BAUD);
  GPS_SERIAL.setTimeout(10);
  Serial.println("GPS   : READY (9600)");

  // LoRa
  LoRa.setPins(LORA_SS, LORA_RESET, LORA_DIO0);

  if (!LoRa.begin(LORA_FREQUENCY)) {
    Serial.println("LoRa  : INIT FAILED!");
    while (true) delay(1000);
  }

  LoRa.setSpreadingFactor(LORA_SPREADING_FACTOR);
  LoRa.setSignalBandwidth(LORA_BANDWIDTH);
  LoRa.setCodingRate4(LORA_CODING_RATE);
  LoRa.setSyncWord(LORA_SYNC_WORD);
  LoRa.enableCrc();
  LoRa.setTxPower(LORA_TX_POWER);

  Serial.println("LoRa  : READY (868 MHz)");
  Serial.println();
  Serial.println(">>> SYSTEM ACTIVE <<<");
  Serial.println();
}


// ============================================================
// LOOP
// ============================================================
void loop() {

  readGPS();

  if (millis() - lastTelemetry >= TELEMETRY_INTERVAL) {
    lastTelemetry = millis();
    sendTelemetry();
  }
}


// ============================================================
// GPS READER
// ============================================================
void readGPS() {
  while (GPS_SERIAL.available()) {
    char c = GPS_SERIAL.read();

    if (c == '\n') {
      nmeaBuffer.trim();

      if (nmeaBuffer.startsWith("$GPGGA") || nmeaBuffer.startsWith("$GNGGA")) {
        parseGGA(nmeaBuffer);
      }
      if (nmeaBuffer.startsWith("$GPRMC") || nmeaBuffer.startsWith("$GNRMC")) {
        parseRMC(nmeaBuffer);
      }

      nmeaBuffer = "";
    } else if (c != '\r') {
      nmeaBuffer += c;
    }
  }
}


// ============================================================
// NMEA HELPERS
// ============================================================
String getNMEAField(String s, int n) {
  int f = 0, start = 0;
  for (int i = 0; i <= s.length(); i++) {
    if (i == s.length() || s[i] == ',') {
      if (f == n) return s.substring(start, i);
      f++;
      start = i + 1;
    }
  }
  return "";
}

double convertNMEA(String v, String d) {
  if (v.length() == 0) return 0.0;
  double raw = v.toDouble();
  int deg = (int)(raw / 100.0);
  double min = raw - (deg * 100.0);
  double dec = deg + min / 60.0;
  if (d == "S" || d == "W") dec = -dec;
  return dec;
}

void parseGGA(String s) {
  String lat  = getNMEAField(s, 2);
  String latD = getNMEAField(s, 3);
  String lon  = getNMEAField(s, 4);
  String lonD = getNMEAField(s, 5);
  String fix  = getNMEAField(s, 6);
  String sat  = getNMEAField(s, 7);
  String alt  = getNMEAField(s, 9);

  if (lat.length() > 0 && lon.length() > 0) {
    latitude  = convertNMEA(lat, latD);
    longitude = convertNMEA(lon, lonD);
  }
  if (alt.length() > 0) altitude = alt.toDouble();
  satellites = sat.toInt();
  gpsFix = fix.toInt() > 0;
}

void parseRMC(String s) {
  String st = getNMEAField(s, 2);
  if (st != "A") return;

  String lat  = getNMEAField(s, 3);
  String latD = getNMEAField(s, 4);
  String lon  = getNMEAField(s, 5);
  String lonD = getNMEAField(s, 6);
  String sp   = getNMEAField(s, 7);

  if (lat.length() > 0 && lon.length() > 0) {
    latitude  = convertNMEA(lat, latD);
    longitude = convertNMEA(lon, lonD);
  }
  if (sp.length() > 0) speedKmh = sp.toDouble() * 1.852;
  gpsFix = true;
}


// ============================================================
// TELEMETRY
// ============================================================
String createTelemetry() {
  String j = "{";
  j += "\"type\":\"telemetry\",";
  j += "\"rocket\":\"PATRICK\",";
  j += "\"team\":\"OFAL\",";
  j += "\"packet\":" + String(packetNumber) + ",";
  j += "\"time_ms\":" + String(millis()) + ",";
  j += "\"gps_fix\":" + String(gpsFix ? "true" : "false") + ",";
  j += "\"lat\":" + String(latitude, 7) + ",";
  j += "\"lon\":" + String(longitude, 7) + ",";
  j += "\"alt\":" + String(altitude, 2) + ",";
  j += "\"speed_kmh\":" + String(speedKmh, 2) + ",";
  j += "\"satellites\":" + String(satellites);
  j += "}";
  return j;
}

void sendTelemetry() {
  packetNumber++;
  String t = createTelemetry();

  LoRa.beginPacket();
  LoRa.print(t);
  int r = LoRa.endPacket();

  Serial.print("#");
  Serial.print(packetNumber);
  if (gpsFix) {
    Serial.print(" | FIX (");
    Serial.print(satellites);
    Serial.print(") ");
    Serial.print(latitude, 5);
    Serial.print(",");
    Serial.print(longitude, 5);
    Serial.print(" | ALT ");
    Serial.print(altitude, 1);
    Serial.print("m");
  } else {
    Serial.print(" | NO FIX");
  }
  Serial.print(" | ");
  Serial.println(r == 1 ? "TX OK" : "TX FAIL");
}
