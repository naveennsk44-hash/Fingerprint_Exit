/***********************************************************************
 * HOSTEL FINGERPRINT SCANNER – GATE VERIFICATION v3
 * ──────────────────────────────────────────────────
 * Logic:
 *   1. Scan fingerprint → get fingerprint_id
 *   2. Look up student roll_no from students_master
 *   3. Check hostel_entries for: status=approved AND scanned_out=false
 *   4. If found → GREEN LED + mark scanned_out=true
 *   5. If not found → RED LED (no approved request or already used)
 *
 * Wiring:
 *   R307 TX  (yellow) → ESP32 GPIO 16
 *   R307 RX  (green)  → ESP32 GPIO 17
 *   R307 VCC (red)    → ESP32 VB (5V)
 *   R307 GND (black)  → ESP32 GND
 *   Green LED         → GPIO 4 → 220Ω → GND
 *   Red LED           → GPIO 26 → 220Ω → GND
 ***********************************************************************/

#include <Adafruit_Fingerprint.h>
#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <WiFi.h>


// ══════════════════════════════════════════════════════════════════════
//  ★  CONFIGURATION  ★
// ══════════════════════════════════════════════════════════════════════

const char *WIFI_SSID = "A15";
const char *WIFI_PASSWORD = "12345678";

const char *SUPABASE_URL = "https://sycvsagfudixrbnqfmzv.supabase.co";
const char *SUPABASE_KEY =
    "eyJhbGciOiJIUzI1NiIsInR5cCI6IkpXVCJ9."
    "eyJpc3MiOiJzdXBhYmFzZSIsInJlZiI6InN5Y3ZzYWdmdWRpeHJibnFmbXp2Iiwicm9sZSI6Im"
    "Fub24iLCJpYXQiOjE3NzExMjk5NzksImV4cCI6MjA4NjcwNTk3OX0."
    "AEU9YEJYd224UWX56TioAj3W4Y6azMv1rUDflw7zhVU";

// ══════════════════════════════════════════════════════════════════════
//  PINS
// ══════════════════════════════════════════════════════════════════════
#define FINGERPRINT_RX 16
#define FINGERPRINT_TX 17
#define GREEN_LED 4
#define RED_LED 26

// ══════════════════════════════════════════════════════════════════════
//  GLOBALS
// ══════════════════════════════════════════════════════════════════════
HardwareSerial mySerial(2);
Adafruit_Fingerprint finger = Adafruit_Fingerprint(&mySerial);
const unsigned long SCAN_COOLDOWN_MS = 3000;
unsigned long lastScanTime = 0;

// ── LED helpers ──────────────────────────────────────────────────────
void flashSuccess() {
  digitalWrite(RED_LED, LOW);
  digitalWrite(GREEN_LED, HIGH);
  delay(2000);
  digitalWrite(GREEN_LED, LOW);
}

void flashFailure() {
  digitalWrite(GREEN_LED, LOW);
  digitalWrite(RED_LED, HIGH);
  delay(2000);
  digitalWrite(RED_LED, LOW);
}

void flashConnecting() {
  for (int i = 0; i < 6; i++) {
    digitalWrite(GREEN_LED, i % 2);
    digitalWrite(RED_LED, !(i % 2));
    delay(250);
  }
  digitalWrite(GREEN_LED, LOW);
  digitalWrite(RED_LED, LOW);
}

// ── WiFi ─────────────────────────────────────────────────────────────
void connectWiFi() {
  Serial.print("Connecting to WiFi: ");
  Serial.println(WIFI_SSID);
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 40) {
    delay(500);
    Serial.print(".");
    attempts++;
  }
  Serial.println();

  if (WiFi.status() == WL_CONNECTED) {
    Serial.print("✓ Connected!  IP: ");
    Serial.println(WiFi.localIP());
    flashSuccess();
  } else {
    Serial.println("✗ WiFi FAILED.");
    flashFailure();
  }
}

// ══════════════════════════════════════════════════════════════════════
//  Step 1: Look up student roll_no by fingerprint_id
// ══════════════════════════════════════════════════════════════════════
String getStudentRollNo(int fingerprintId) {
  if (WiFi.status() != WL_CONNECTED)
    return "";

  HTTPClient http;
  String url = String(SUPABASE_URL) +
               "/rest/v1/students_master"
               "?fingerprint_id=eq." +
               String(fingerprintId) + "&select=roll_no";

  http.begin(url);
  http.addHeader("apikey", SUPABASE_KEY);
  http.addHeader("Authorization", String("Bearer ") + SUPABASE_KEY);

  int httpCode = http.GET();
  String rollNo = "";

  if (httpCode == 200) {
    String response = http.getString();
    Serial.print("  Student: ");
    Serial.println(response);

    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, response);
    if (!err && doc.is<JsonArray>() && doc.size() > 0) {
      rollNo = doc[0]["roll_no"].as<String>();
    }
  }
  http.end();
  return rollNo;
}

// ══════════════════════════════════════════════════════════════════════
//  Step 2: Check for approved + unused leave request
//  Returns the entry ID if found, empty string if not
// ══════════════════════════════════════════════════════════════════════
String checkApprovedEntry(String rollNo) {
  if (WiFi.status() != WL_CONNECTED)
    return "";

  HTTPClient http;
  // Find any entry that is APPROVED and NOT YET SCANNED OUT
  String url = String(SUPABASE_URL) +
               "/rest/v1/hostel_entries"
               "?roll_no=eq." +
               rollNo +
               "&status=eq.approved"
               "&scanned_out=eq.false"
               "&limit=1"
               "&select=id,status,scanned_out";

  Serial.print("  Check URL: ");
  Serial.println(url);

  http.begin(url);
  http.addHeader("apikey", SUPABASE_KEY);
  http.addHeader("Authorization", String("Bearer ") + SUPABASE_KEY);

  int httpCode = http.GET();
  String entryId = "";

  if (httpCode == 200) {
    String response = http.getString();
    Serial.print("  Response: ");
    Serial.println(response);

    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, response);
    if (!err && doc.is<JsonArray>() && doc.size() > 0) {
      entryId = doc[0]["id"].as<String>();
      Serial.print("  ★ Found approved entry: ");
      Serial.println(entryId);
    } else {
      Serial.println("  No approved+unused entry found");
    }
  } else {
    Serial.print("  HTTP error: ");
    Serial.println(httpCode);
  }

  http.end();
  return entryId;
}

// ══════════════════════════════════════════════════════════════════════
//  Step 3: Mark the entry as scanned_out = true (used)
// ══════════════════════════════════════════════════════════════════════
void markAsScannedOut(String entryId) {
  if (WiFi.status() != WL_CONNECTED)
    return;

  HTTPClient http;
  // PATCH the specific entry to set scanned_out = true
  String url = String(SUPABASE_URL) +
               "/rest/v1/hostel_entries"
               "?id=eq." +
               entryId;

  http.begin(url);
  http.addHeader("Content-Type", "application/json");
  http.addHeader("apikey", SUPABASE_KEY);
  http.addHeader("Authorization", String("Bearer ") + SUPABASE_KEY);

  String payload = "{\"scanned_out\":true}";
  int httpCode = http.PATCH(payload);

  if (httpCode == 200 || httpCode == 204) {
    Serial.println("  ✓ Marked as scanned out");
  } else {
    Serial.print("  ✗ Mark failed, HTTP: ");
    Serial.println(httpCode);
  }
  http.end();
}

// ── Fingerprint scan ─────────────────────────────────────────────────
int scanFingerprint() {
  int p = finger.getImage();
  if (p != FINGERPRINT_OK)
    return -1;

  p = finger.image2Tz();
  if (p != FINGERPRINT_OK)
    return -1;

  p = finger.fingerSearch();
  if (p == FINGERPRINT_OK)
    return finger.fingerID;

  return -1;
}

// ══════════════════════════════════════════════════════════════════════
//  SETUP
// ══════════════════════════════════════════════════════════════════════
void setup() {
  Serial.begin(115200);
  delay(100);

  Serial.println("\n=======================================");
  Serial.println("  Hostel Gate Verification v3.0");
  Serial.println("=======================================\n");

  pinMode(GREEN_LED, OUTPUT);
  pinMode(RED_LED, OUTPUT);
  digitalWrite(GREEN_LED, LOW);
  digitalWrite(RED_LED, LOW);

  mySerial.begin(57600, SERIAL_8N1, FINGERPRINT_RX, FINGERPRINT_TX);
  finger.begin(57600);

  if (finger.verifyPassword()) {
    Serial.println("✓ Fingerprint sensor detected");
  } else {
    Serial.println("✗ Sensor NOT found – check wiring!");
    while (1) {
      flashFailure();
      delay(2000);
    }
  }

  finger.getTemplateCount();
  Serial.print("Templates stored: ");
  Serial.println(finger.templateCount);

  flashConnecting();
  connectWiFi();

  Serial.println("\n── Ready. Place finger to verify gate exit. ──\n");
}

// ══════════════════════════════════════════════════════════════════════
//  LOOP
// ══════════════════════════════════════════════════════════════════════
void loop() {
  if (millis() - lastScanTime < SCAN_COOLDOWN_MS) {
    delay(100);
    return;
  }

  int id = scanFingerprint();

  if (id > 0) {
    lastScanTime = millis();

    Serial.println("────────────────────────────────");
    Serial.print("Finger matched! ID #");
    Serial.print(id);
    Serial.print("  confidence: ");
    Serial.println(finger.confidence);

    // Step 1: Get student roll_no
    Serial.println("Step 1: Looking up student...");
    String rollNo = getStudentRollNo(id);

    if (rollNo == "") {
      Serial.println("  ✗ No student mapped to this fingerprint");
      flashFailure();
      Serial.println("────────────────────────────────\n");
      return;
    }

    Serial.print("  Roll: ");
    Serial.println(rollNo);

    // Step 2: Check for approved + unused leave
    Serial.println("Step 2: Checking approved leave...");
    String entryId = checkApprovedEntry(rollNo);

    if (entryId != "") {
      // APPROVED — allow exit
      Serial.println("  ★ GATE OPEN — Approved exit! ★");
      flashSuccess();

      // Step 3: Mark as used so they can't scan again
      Serial.println("Step 3: Marking as scanned out...");
      markAsScannedOut(entryId);

    } else {
      // DENIED — no approved leave or already used
      Serial.println("  ✗ ACCESS DENIED — No approved leave");
      flashFailure();
    }

    Serial.println("────────────────────────────────\n");

  } else if (finger.getImage() == FINGERPRINT_OK) {
    lastScanTime = millis();
    Serial.println("Unknown fingerprint — Access denied");
    flashFailure();
  }

  delay(100);
}
