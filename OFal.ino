/*
  ============================================================
  OFAL R2S 2026 - PATRICK
  FLIGHT TELEMETRY COMPUTER
  ============================================================

  MCU:
    Seeed Studio XIAO nRF52840 Sense

  PERIPHERALS:
    - BN-220 GPS
    - SX1276 LoRa 868 MHz
    - Push Button on D2

  ============================================================
  PINOUT
  ============================================================

  GPS:
    GPS TX -> XIAO D7 (RX)
    GPS RX -> XIAO D6 (TX)
    GPS VCC -> 3.3V
    GPS GND -> GND

  SX1276:
    NSS  -> D3
    DIO0 -> D1
    RES  -> D4
    SCK  -> D8
    MISO -> D9
    MOSI -> D10
    VCC  -> 3.3V
    GND  -> GND
    ANT  -> Antenna

  BUTTON:
    D2 -> Push Button -> GND

  ============================================================
  BUTTON FUNCTION
  ============================================================

  PRESS:
    ON  -> System starts (GPS + LoRa transmission active)
    OFF -> System stops (GPS + LoRa disabled)

  Note:
    This version does NOT use SYSTEM OFF sleep mode.
    The MCU stays awake but stops all activity.

  ============================================================
  GPS BAUD RATE
  ============================================================

  BN-220 default is usually 9600.
  If no data appears, try 115200.

  ============================================================
*/

#include <Arduino.h>
#include <SPI.h>
#include <LoRa.h>

// ============================================================
// PINS
// ============================================================

// ---------- Button ----------
#define BUTTON_PIN  D2

// ---------- LoRa ----------
#define LORA_SS     D3
#define LORA_DIO0   D1
#define LORA_RESET  D4

// ---------- GPS ----------
#define GPS_SERIAL  Serial1
#define GPS_BAUD    9600      // Change to 115200 if no data appears

// ============================================================
// LORA SETTINGS
// ============================================================

#define LORA_FREQUENCY        868E6
#define LORA_SPREADING_FACTOR 10
#define LORA_BANDWIDTH        125E3
#define LORA_CODING_RATE      8
#define LORA_SYNC_WORD        0x12
#define LORA_TX_POWER         17

// ============================================================
// TELEMETRY SETTINGS
// ============================================================

#define TELEMETRY_INTERVAL 1000   // Send every 1 second

unsigned long lastTelemetry = 0;
unsigned long packetNumber = 0;

// ============================================================
// GPS DATA
// ============================================================

double latitude   = 0.0;
double longitude  = 0.0;
double altitude   = 0.0;
double speedKmh   = 0.0;

int    satellites = 0;
bool   gpsFix     = false;

String nmeaBuffer = "";

// ============================================================
// SYSTEM STATE
// ============================================================

bool systemActive = true;

// ============================================================
// BUTTON DEBOUNCE
// ============================================================

bool lastButtonState = HIGH;
unsigned long lastButtonChange = 0;

#define BUTTON_DEBOUNCE 300


// ============================================================
// SETUP
// ============================================================
void setup() {

  // ----------------------------------------------------------
  // BUTTON
  // ----------------------------------------------------------
  pinMode(BUTTON_PIN, INPUT_PULLUP);
  delay(100);

  // ----------------------------------------------------------
  // USB SERIAL (for debug)
  // ----------------------------------------------------------
  Serial.begin(115200);
  delay(1000);

  Serial.println();
  Serial.println("============================================");
  Serial.println("   OFAL R2S 2026 - PATRICK");
  Serial.println("   Flight Telemetry Computer");
  Serial.println("============================================");

  // ----------------------------------------------------------
  // GPS
  // ----------------------------------------------------------
  GPS_SERIAL.begin(GPS_BAUD);
  GPS_SERIAL.setTimeout(10);

  Serial.print("GPS   : READY (baud ");
  Serial.print(GPS_BAUD);
  Serial.println(")");

  // ----------------------------------------------------------
  // LORA
  // ----------------------------------------------------------
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
  Serial.println("Button: READY");
  Serial.println();

  Serial.println(">>> SYSTEM ACTIVE <<<");
  Serial.println(">>> Press button to toggle ON/OFF <<<");
  Serial.println();
}


// ============================================================
// MAIN LOOP
// ============================================================
void loop() {

  // ----------------------------------------------------------
  // CHECK BUTTON
  // ----------------------------------------------------------
  checkButton();

  // ----------------------------------------------------------
  // IF SYSTEM IS OFF, DO NOTHING
  // ----------------------------------------------------------
  if (!systemActive) {
    delay(100);
    return;
  }

  // ----------------------------------------------------------
  // READ GPS
  // ----------------------------------------------------------
  readGPS();

  // ----------------------------------------------------------
  // SEND TELEMETRY EVERY SECOND
  // ----------------------------------------------------------
  if (millis() - lastTelemetry >= TELEMETRY_INTERVAL) {
    lastTelemetry = millis();
    sendTelemetry();
  }
}


// ============================================================
// BUTTON HANDLER
// ============================================================
void checkButton() {

  bool currentState = digitalRead(BUTTON_PIN);

  // Detect HIGH -> LOW transition (button pressed)
  if (lastButtonState == HIGH && currentState == LOW) {

    if (millis() - lastButtonChange > BUTTON_DEBOUNCE) {

      lastButtonChange = millis();

      // Toggle system state
      systemActive = !systemActive;

      Serial.println();

      if (systemActive) {
        // --------------------------------------------------
        // TURN ON
        // --------------------------------------------------
        Serial.println(">>> BUTTON: SYSTEM ON");
        Serial.println(">>> Starting GPS + LoRa transmission...");

        // Restart GPS
        GPS_SERIAL.begin(GPS_BAUD);
        GPS_SERIAL.setTimeout(10);

        // Wake up LoRa (re-init if needed)
        LoRa.idle();

      } else {
        // --------------------------------------------------
        // TURN OFF
        // --------------------------------------------------
        Serial.println(">>> BUTTON: SYSTEM OFF");
        Serial.println(">>> Stopping GPS + LoRa...");

        // Stop GPS
        GPS_SERIAL.end();
        nmeaBuffer = "";

        // Put LoRa to sleep
        LoRa.sleep();
      }

      Serial.println();
    }
  }

  lastButtonState = currentState;
}


// ============================================================
// GPS READER
// ============================================================
void readGPS() {

  while (GPS_SERIAL.available()) {

    char c = GPS_SERIAL.read();

    if (c == '\n') {

      nmeaBuffer.trim();

      // ------------------------------------------------------
      // Parse GGA sentence (position + altitude + satellites)
      // ------------------------------------------------------
      if (nmeaBuffer.startsWith("$GPGGA") ||
          nmeaBuffer.startsWith("$GNGGA")) {
        parseGGA(nmeaBuffer);
      }

      // ------------------------------------------------------
      // Parse RMC sentence (position + speed)
      // ------------------------------------------------------
      if (nmeaBuffer.startsWith("$GPRMC") ||
          nmeaBuffer.startsWith("$GNRMC")) {
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

// Extract a specific field from an NMEA sentence
String getNMEAField(String sentence, int fieldNumber) {

  int field = 0;
  int start = 0;

  for (int i = 0; i <= sentence.length(); i++) {

    if (i == sentence.length() || sentence[i] == ',') {

      if (field == fieldNumber) {
        return sentence.substring(start, i);
      }

      field++;
      start = i + 1;
    }
  }

  return "";
}

// Convert NMEA coordinate format to decimal degrees
double convertNMEA(String value, String direction) {

  if (value.length() == 0) return 0.0;

  double raw = value.toDouble();
  int degrees = (int)(raw / 100.0);
  double minutes = raw - (degrees * 100.0);
  double decimal = degrees + minutes / 60.0;

  if (direction == "S" || direction == "W") {
    decimal = -decimal;
  }

  return decimal;
}

// Parse GGA sentence
void parseGGA(String sentence) {

  String lat    = getNMEAField(sentence, 2);
  String latDir = getNMEAField(sentence, 3);
  String lon    = getNMEAField(sentence, 4);
  String lonDir = getNMEAField(sentence, 5);
  String fix    = getNMEAField(sentence, 6);
  String sat    = getNMEAField(sentence, 7);
  String alt    = getNMEAField(sentence, 9);

  if (lat.length() > 0 && lon.length() > 0) {
    latitude  = convertNMEA(lat, latDir);
    longitude = convertNMEA(lon, lonDir);
  }

  if (alt.length() > 0) {
    altitude = alt.toDouble();
  }

  satellites = sat.toInt();
  gpsFix = fix.toInt() > 0;
}

// Parse RMC sentence
void parseRMC(String sentence) {

  String status = getNMEAField(sentence, 2);

  if (status != "A") return;

  String lat    = getNMEAField(sentence, 3);
  String latDir = getNMEAField(sentence, 4);
  String lon    = getNMEAField(sentence, 5);
  String lonDir = getNMEAField(sentence, 6);
  String speed  = getNMEAField(sentence, 7);

  if (lat.length() > 0 && lon.length() > 0) {
    latitude  = convertNMEA(lat, latDir);
    longitude = convertNMEA(lon, lonDir);
  }

  if (speed.length() > 0) {
    double knots = speed.toDouble();
    speedKmh = knots * 1.852;
  }

  gpsFix = true;
}


// ============================================================
// TELEMETRY
// ============================================================

// Build JSON telemetry string (sent via LoRa only)
String createTelemetry() {

  String json = "{";

  json += "\"type\":\"telemetry\",";
  json += "\"rocket\":\"PATRICK\",";
  json += "\"team\":\"OFAL\",";
  json += "\"packet\":" + String(packetNumber) + ",";
  json += "\"time_ms\":" + String(millis()) + ",";
  json += "\"trigger\":\"AUTO\",";
  json += "\"gps_fix\":" + String(gpsFix ? "true" : "false") + ",";
  json += "\"lat\":" + String(latitude, 7) + ",";
  json += "\"lon\":" + String(longitude, 7) + ",";
  json += "\"alt\":" + String(altitude, 2) + ",";
  json += "\"speed_kmh\":" + String(speedKmh, 2) + ",";
  json += "\"satellites\":" + String(satellites);

  json += "}";

  return json;
}

// Send telemetry over LoRa and print short report to Serial
void sendTelemetry() {

  if (!systemActive) return;

  packetNumber++;

  String telemetry = createTelemetry();

  // ---------- Send via LoRa ----------
  LoRa.beginPacket();
  LoRa.print(telemetry);
  int result = LoRa.endPacket();

  // ---------- Short Serial Report ----------
  Serial.print("#");
  Serial.print(packetNumber);

  // GPS status
  if (gpsFix) {
    Serial.print(" | GPS: FIX (");
    Serial.print(satellites);
    Serial.print(" sats)");
    Serial.print(" | Pos: ");
    Serial.print(latitude, 5);
    Serial.print(", ");
    Serial.print(longitude, 5);
    Serial.print(" | Alt: ");
    Serial.print(altitude, 1);
    Serial.print(" m");
  } else {
    Serial.print(" | GPS: NO FIX");
  }

  // LoRa status
  Serial.print(" | LoRa: ");
  Serial.println(result == 1 ? "OK" : "FAIL");
}