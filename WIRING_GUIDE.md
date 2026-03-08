# ESP32 Fingerprint Scanner – Complete Wiring & Setup Guide

> **System**: Hostel Leave Verification with Fingerprint Gate Access

---

## Part 1: Hardware Required

| Component | Qty | Notes |
|---|---|---|
| ESP32-WROOM DevKit | 1 | With CP2102 USB chip |
| R307 Fingerprint Sensor | 1 | 6-wire variant (red, black, yellow, green, white, blue) |
| Green LED | 1 | Gate pass approved indicator |
| Red LED | 1 | Access denied indicator |
| 220Ω Resistors | 2 | One per LED |
| Breadboard | 1 | For connections |
| Jumper Wires | ~10 | Female-to-male recommended for sensor |
| USB Data Cable | 1 | Must support data (not charge-only) |

---

## Part 2: Wiring Diagram

```
┌──────────────────────────────────────────────────────────┐
│                      ESP32 DevKit                        │
│                                                          │
│  VB (5V)     ────► Red wire    ── R307 VCC               │
│  GND         ────► Black wire  ── R307 GND               │
│  GPIO 16     ◄──── Yellow wire ── R307 TX (sensor out)   │
│  GPIO 17     ────► Green wire  ── R307 RX (sensor in)    │
│                    White wire  ── NOT CONNECTED           │
│                    Blue wire   ── NOT CONNECTED           │
│                                                          │
│  GPIO 4  ──► [220Ω] ──► Green LED (+) ──► GND           │
│  GPIO 26 ──► [220Ω] ──► Red LED   (+) ──► GND           │
└──────────────────────────────────────────────────────────┘
```

### ⚠️ Critical Tips
- **Each LED needs its OWN GPIO pin** (don't put both on GPIO 4!)
- **R307 needs 5V** — connect red wire to **VB**, not 3V3
- **GND must be firm** — if sensor flickers, use a different jumper wire
- **Disconnect yellow & green wires** before uploading code to ESP32
- **GPIO 5 goes HIGH during boot** — don't use it for LEDs

---

## Part 3: Software Setup

### 3.1 Install Arduino IDE
1. Download from [arduino.cc](https://www.arduino.cc/en/software)
2. Install and open

### 3.2 Add ESP32 Board Support
1. **File → Preferences → Additional Board Manager URLs**, add:
   ```
   https://raw.githubusercontent.com/espressif/arduino-esp32/gh-pages/package_esp32_index.json
   ```
2. **Tools → Board → Boards Manager** → Search "ESP32" → Install **esp32 by Espressif**

### 3.3 Install Required Libraries
**Tools → Manage Libraries**, install:
- **Adafruit Fingerprint Sensor Library** (by Adafruit)
- **ArduinoJson** (by Benoit Blanchon)

### 3.4 Install CP2102 USB Driver
- Download from [Silicon Labs](https://www.silabs.com/developers/usb-to-uart-bridge-vcp-drivers)
- Install → Restart computer
- ESP32 should appear as a COM port in **Device Manager → Ports**

### 3.5 Arduino IDE Settings
- **Tools → Board**: ESP32 Dev Module
- **Tools → Port**: Select the COM port (e.g., COM5)
- **Tools → Upload Speed**: 115200

---

## Part 4: Supabase Database Setup

### 4.1 Create Tables
Run in **Supabase Dashboard → SQL Editor**:

```sql
-- Students Master Table
CREATE TABLE IF NOT EXISTS students_master (
  roll_no       TEXT PRIMARY KEY,
  student_name  TEXT NOT NULL,
  parent_phone  TEXT,
  email         TEXT NOT NULL UNIQUE,
  fingerprint_id INT
);

-- Hostel Entries Table (leave requests)
CREATE TABLE IF NOT EXISTS hostel_entries (
  id               UUID DEFAULT gen_random_uuid() PRIMARY KEY,
  roll_no          TEXT NOT NULL REFERENCES students_master(roll_no),
  student_name     TEXT,
  phone            TEXT,
  entry_time       TIMESTAMPTZ DEFAULT now(),
  reason           TEXT,
  verified         BOOLEAN DEFAULT true,
  status           TEXT DEFAULT 'pending' CHECK (status IN ('pending', 'approved', 'rejected')),
  approval_token   UUID DEFAULT gen_random_uuid() UNIQUE,
  responded_at     TIMESTAMPTZ,
  warden_remarks   TEXT,
  scanned_out      BOOLEAN DEFAULT false
);

-- Biometric Logs
CREATE TABLE IF NOT EXISTS biometric_logs (
  id              UUID DEFAULT gen_random_uuid() PRIMARY KEY,
  fingerprint_id  INT NOT NULL,
  confidence      INT,
  result          TEXT DEFAULT 'match',
  scanned_at      TIMESTAMPTZ DEFAULT now()
);
```

### 4.2 Enable RLS & Policies
```sql
ALTER TABLE students_master ENABLE ROW LEVEL SECURITY;
ALTER TABLE hostel_entries  ENABLE ROW LEVEL SECURITY;
ALTER TABLE biometric_logs  ENABLE ROW LEVEL SECURITY;

-- Students: readable by all
CREATE POLICY "anon read students" ON students_master FOR SELECT TO anon USING (true);
CREATE POLICY "auth read students" ON students_master FOR SELECT TO authenticated USING (true);

-- Hostel entries: read/write for auth, read/update for anon (ESP32)
CREATE POLICY "auth insert entries" ON hostel_entries FOR INSERT TO authenticated WITH CHECK (true);
CREATE POLICY "auth read entries" ON hostel_entries FOR SELECT TO authenticated USING (true);
CREATE POLICY "anon read entries" ON hostel_entries FOR SELECT TO anon USING (true);
CREATE POLICY "anon update entries" ON hostel_entries FOR UPDATE TO anon USING (true) WITH CHECK (true);

-- Biometric logs: anon can insert (ESP32), auth can read
CREATE POLICY "anon insert logs" ON biometric_logs FOR INSERT TO anon WITH CHECK (true);
CREATE POLICY "auth read logs" ON biometric_logs FOR SELECT TO authenticated USING (true);
```

### 4.3 Add Sample Students
```sql
INSERT INTO students_master (roll_no, student_name, parent_phone, email) VALUES
  ('2403717712021018', 'Your Name', '+919876543210', 'your.email@example.com')
ON CONFLICT (roll_no) DO NOTHING;
```

---

## Part 5: Enroll Fingerprints

1. **Disconnect** yellow & green wires from ESP32
2. Open `firmware/enroll/enroll.ino` in Arduino IDE
3. Click **Upload**
4. **Reconnect** yellow → GPIO 16, green → GPIO 17
5. Open **Serial Monitor** (115200 baud)
6. Press **EN** button on ESP32
7. Should see: `"✓ Fingerprint sensor found!"`
8. Type `1` → press Enter → place finger twice when prompted
9. Repeat with ID `2`, `3`, etc. for more students

### Map Fingerprint to Student (Supabase SQL):
```sql
UPDATE students_master SET fingerprint_id = 1 WHERE roll_no = '2403717712021018';
```

---

## Part 6: Deploy Gate Scanner

1. **Disconnect** yellow & green wires
2. Open `firmware/hostel_fingerprint/hostel_fingerprint.ino`
3. **Edit WiFi credentials** at the top (lines 31-32):
   ```cpp
   const char *WIFI_SSID = "YourWiFiName";
   const char *WIFI_PASSWORD = "YourWiFiPassword";
   ```
4. Verify Supabase URL and Key are correct (lines 34-39)
5. Click **Upload**
6. **Reconnect** yellow → GPIO 16, green → GPIO 17
7. Open **Serial Monitor** → Press **EN**
8. Should see:
   ```
   ✓ Fingerprint sensor detected
   ✓ Connected! IP: 192.168.x.x
   ── Ready. Place finger to verify gate exit. ──
   ```

---

## Part 7: Full System Flow

```
Student                    Web App              Warden           ESP32 Gate
   │                          │                    │                  │
   ├─ Submit leave request ──►│                    │                  │
   │                          ├─ OTP verify ──────►│                  │
   │                          ├─ Insert entry ────►│ (DB: pending)    │
   │                          ├─ N8N webhook ─────►│                  │
   │                          │                    ├─ Approve/Reject  │
   │                          │                    ├─ Update DB ─────►│
   │                          │                    │                  │
   ├─ Place finger ──────────────────────────────────────────────────►│
   │                          │                    │  Check DB:       │
   │                          │                    │  approved +      │
   │                          │                    │  scanned_out=F?  │
   │                          │                    │    ├─YES→ GREEN  │
   │                          │                    │    └─NO → RED    │
```

---

## Part 8: Troubleshooting

| Problem | Solution |
|---|---|
| ESP32 not detected by PC | Install CP2102 driver, use data cable |
| Upload fails "TX path down" | Disconnect yellow & green wires first |
| Sensor not found | Check GND wire is firm, use 5V (VB) |
| Both LEDs light up | Each LED must be on a SEPARATE GPIO pin |
| WiFi won't connect | Check SSID/password, use 2.4GHz hotspot |
| Approved but shows red | Check `fingerprint_id` is set in `students_master` |
| Always shows green | Check `scanned_out` column exists in DB |
| Serial Monitor blank | Disconnect sensor data wires, press EN |

---

## File Structure
```
hostel-entry/
├── firmware/
│   ├── enroll/enroll.ino          ← Fingerprint enrollment
│   ├── hostel_fingerprint/
│   │   └── hostel_fingerprint.ino ← Gate scanner (main sketch)
│   ├── diagnostic/diagnostic.ino  ← Sensor troubleshooting
│   └── WIRING_GUIDE.md            ← This file
├── sql/
│   ├── setup.sql                  ← Initial DB setup
│   └── migrate-esp32-access.sql   ← ESP32 RLS policies
├── js/
│   ├── config.js                  ← Supabase + N8N config
│   └── app.js                     ← Web app logic
├── css/styles.css
├── index.html                     ← Leave request form
├── approve.html                   ← Warden approval page
└── n8n/                           ← N8N workflow files
```