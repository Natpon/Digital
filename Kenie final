#define BLYNK_PRINT Serial

#define BLYNK_TEMPLATE_ID "TMPL6-MZnj3UR"
#define BLYNK_TEMPLATE_NAME "Lift"
#define BLYNK_AUTH_TOKEN "IrWjBFXhqlxCxD40uim8VP1jFr6quIEt"

#include <WiFi.h>
#include <WiFiClient.h> // เพิ่มเข้ามาเพื่อความเสถียร
#include <BlynkSimpleEsp32.h>

// ==========================================
// 1. ตั้งค่า WiFi
// ==========================================
char ssid[] = "......";
char pass[] = "1212312121"; 

// ==========================================
// 2. ตัวแปร Global
// ==========================================
char currentFloor  = '1';
char userFromFloor = '0';
bool waitingForDest = false;

// ==========================================
// 3. Blynk Virtual Pin Handlers
// ==========================================

// V1: เลือกชั้นที่คนรอลิฟต์อยู่
BLYNK_WRITE(V1) {
  int floor = param.asInt();
  if (floor < 1 || floor > 3) return;

  userFromFloor = '0' + floor;
  waitingForDest = true;

  Serial.print("[Blynk] User waiting at: ");
  Serial.println(userFromFloor);

  Serial2.write(userFromFloor); // ส่ง '1', '2', หรือ '3'
  
  Blynk.virtualWrite(V3, String("Calling lift to floor ") + userFromFloor);
}

// V2: เลือกชั้นปลายทางที่อยากไป
BLYNK_WRITE(V2) {
  if (!waitingForDest) {
    Blynk.virtualWrite(V3, "Please call lift (V1) first!");
    return;
  }

  int floor = param.asInt();
  if (floor < 1 || floor > 3) return;

  char destFloor = '0' + floor;
  waitingForDest = false;

  Serial.print("[Blynk] Set destination to: ");
  Serial.println(destFloor);

  Serial2.write(destFloor); 
  
  Blynk.virtualWrite(V3, String("Going to floor ") + destFloor);
}

// ==========================================
// 4. Setup
// ==========================================
void setup() {
  Serial.begin(115200);

  // Serial2: ขา RX=16, TX=17 สำหรับคุยกับ FPGA
  Serial2.begin(9600, SERIAL_8N1, 16, 17);

  Serial.println("=== Elevator ESP32 Started ===");

  // แก้จาก password เป็น pass ให้ตรงกับข้างบน
  Blynk.begin(BLYNK_AUTH_TOKEN, ssid, pass); 

  Blynk.virtualWrite(V3, "System Ready - Elevator at floor 1");
}

// ==========================================
// 5. Loop หลัก
// ==========================================
void loop() {
  Blynk.run();

  if (Serial2.available()) {
    char status = (char)Serial2.read();

    // เช็คว่าเป็นตัวเลข 1-3 หรือไม่
    if (status >= '1' && status <= '3') {
      currentFloor = status;
      Serial.print("[FPGA Feedback] Current Floor: ");
      Serial.println(currentFloor);

      Blynk.virtualWrite(V3, String("Elevator now at floor ") + currentFloor);
    }
  }
}
