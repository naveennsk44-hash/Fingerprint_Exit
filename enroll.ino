/***********************************************************************
 * FINGERPRINT ENROLLMENT SKETCH
 * ─────────────────────────────
 * Purpose : Register new fingerprints into the R307/AS608 sensor.
 *           Each fingerprint gets an ID (1-127) that you later map
 *           to a student roll_no in Supabase.
 *
 * Wiring  :
 *   R307 TX  (green)  → ESP32 GPIO 16 (RX2)
 *   R307 RX  (white)  → ESP32 GPIO 17 (TX2)
 *   R307 VCC (red)    → ESP32 5V  (Vin)
 *   R307 GND (black)  → ESP32 GND
 *   Green LED (+)     → GPIO 4  → 220Ω → GND
 *   Red   LED (+)     → GPIO 5  → 220Ω → GND
 *
 * Usage   : Upload → Open Serial Monitor (115200 baud)
 *           → Type the ID number (1-127) → Place finger twice
 *           → Green LED = success, Red LED = failure
 *
 * Note    : No buzzer is used in this project.
 ***********************************************************************/

#include <Adafruit_Fingerprint.h>

// ── Pin Definitions ──────────────────────────────────────────────────
#define FINGERPRINT_RX 16 // ESP32 RX2 ← Sensor TX
#define FINGERPRINT_TX 17 // ESP32 TX2 → Sensor RX
#define GREEN_LED 4
#define RED_LED 5

// ── Sensor Setup ─────────────────────────────────────────────────────
HardwareSerial mySerial(2); // UART2
Adafruit_Fingerprint finger = Adafruit_Fingerprint(&mySerial);

// ── Helper: LED / Buzzer Feedback ────────────────────────────────────
void flashSuccess() {
  digitalWrite(GREEN_LED, HIGH);
  delay(500);
  digitalWrite(GREEN_LED, LOW);
}

void flashFailure() {
  digitalWrite(RED_LED, HIGH);
  delay(1000);
  digitalWrite(RED_LED, LOW);
}

// ── Setup ────────────────────────────────────────────────────────────
void setup() {
  Serial.begin(115200);
  while (!Serial)
    ;
  delay(100);

  Serial.println("\n=================================");
  Serial.println("  Fingerprint Enrollment Tool");
  Serial.println("=================================\n");

  // LED / Buzzer pins
  pinMode(GREEN_LED, OUTPUT);
  pinMode(RED_LED, OUTPUT);

  // Start sensor serial
  mySerial.begin(57600, SERIAL_8N1, FINGERPRINT_RX, FINGERPRINT_TX);
  finger.begin(57600);

  if (finger.verifyPassword()) {
    Serial.println("✓ Fingerprint sensor found!");
    flashSuccess();
  } else {
    Serial.println("✗ Sensor not found. Check wiring.");
    flashFailure();
    while (1) {
      delay(1);
    }
  }

  finger.getTemplateCount();
  Serial.print("Sensor contains ");
  Serial.print(finger.templateCount);
  Serial.println(" template(s).\n");

  Serial.println("Type a fingerprint ID (1-127) and press Enter to enroll...");
}

// ── Read ID from Serial Monitor ──────────────────────────────────────
uint8_t readID() {
  uint8_t id = 0;
  while (true) {
    while (!Serial.available())
      ;
    id = Serial.parseInt();
    if (id >= 1 && id <= 127)
      return id;
    Serial.println("Invalid ID – must be 1-127. Try again:");
  }
}

// ── Enrollment Flow ──────────────────────────────────────────────────
uint8_t enrollFingerprint(uint8_t id) {
  int p = -1;

  // ── First capture ──
  Serial.print("Place finger for ID #");
  Serial.println(id);
  while (p != FINGERPRINT_OK) {
    p = finger.getImage();
    switch (p) {
    case FINGERPRINT_OK:
      Serial.println("Image taken");
      break;
    case FINGERPRINT_NOFINGER:
      break; // waiting
    case FINGERPRINT_PACKETRECIEVEERR:
      Serial.println("Communication error");
      return p;
    case FINGERPRINT_IMAGEFAIL:
      Serial.println("Imaging error");
      return p;
    default:
      Serial.println("Unknown error");
      return p;
    }
  }

  p = finger.image2Tz(1);
  if (p != FINGERPRINT_OK) {
    Serial.println("Could not convert image (slot 1)");
    return p;
  }

  // ── Remove finger ──
  Serial.println("Remove finger...");
  delay(2000);
  while (finger.getImage() != FINGERPRINT_NOFINGER)
    ;
  Serial.println("Place the SAME finger again...");

  // ── Second capture ──
  p = -1;
  while (p != FINGERPRINT_OK) {
    p = finger.getImage();
    switch (p) {
    case FINGERPRINT_OK:
      Serial.println("Image taken");
      break;
    case FINGERPRINT_NOFINGER:
      break;
    default:
      Serial.println("Error");
      return p;
    }
  }

  p = finger.image2Tz(2);
  if (p != FINGERPRINT_OK) {
    Serial.println("Could not convert image (slot 2)");
    return p;
  }

  // ── Create model ──
  p = finger.createModel();
  if (p != FINGERPRINT_OK) {
    Serial.println("Fingerprints did not match");
    return p;
  }

  // ── Store ──
  p = finger.storeModel(id);
  if (p == FINGERPRINT_OK) {
    Serial.println("✓ Stored successfully!");
    return p;
  } else {
    Serial.println("✗ Storage error");
    return p;
  }
}

// ── Main Loop ────────────────────────────────────────────────────────
void loop() {
  uint8_t id = readID();

  Serial.print("\nEnrolling ID #");
  Serial.println(id);

  if (enrollFingerprint(id) == FINGERPRINT_OK) {
    flashSuccess();
    Serial.println("──────────────────────────────");
    Serial.print("SUCCESS: Fingerprint stored as ID #");
    Serial.println(id);
    Serial.println("Now map this ID to a student roll_no in Supabase.");
    Serial.println("──────────────────────────────\n");
  } else {
    flashFailure();
    Serial.println("FAILED – try again.\n");
  }

  Serial.println("Type another ID to enroll, or reset the board.\n");
}
