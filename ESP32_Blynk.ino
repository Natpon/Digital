// ==========================================
// Elevator ESP32 - Blynk IoT Version
// ==========================================
// Virtual Pins:
//   V1 = ชั้นที่ผู้ใช้อยู่ (user กดในแอป)
//   V2 = ชั้นที่ต้องการไป (user กดในแอป)
//   V3 = สถานะชั้นปัจจุบันของลิฟต์ (แสดงในแอป)
// ==========================================

#define BLYNK_TEMPLATE_ID   "YOUR_TEMPLATE_ID"   // ใส่ Template ID จาก Blynk
#define BLYNK_TEMPLATE_NAME "Elevator"
#define BLYNK_AUTH_TOKEN    "YOUR_AUTH_TOKEN"     // ใส่ Auth Token จาก Blynk

#include <WiFi.h>
#include <BlynkSimpleEsp32.h>

// ==========================================
// 1. ตั้งค่า WiFi
// ==========================================
const char* ssid     = "YOUR_WIFI_NAME";
const char* password = "YOUR_WIFI_PASSWORD";

// ==========================================
// 2. ตัวแปร Global
// ==========================================
char currentFloor  = '1';  // ชั้นปัจจุบันของลิฟต์
char userFromFloor = '0';  // ชั้นที่ผู้ใช้อยู่ (รอกด V1 ก่อน)
bool waitingForDest = false;  // รอให้ user กด V2 อยู่มั้ย

// ==========================================
// 3. Blynk Virtual Pin Handlers
// ==========================================

// V1: ผู้ใช้กดบอกว่าตัวเองอยู่ชั้นไหน
// แอปควรเป็น Button/Selector ค่า 1, 2, 3
BLYNK_WRITE(V1) {
  int floor = param.asInt();

  if (floor < 1 || floor > 3) return;

  userFromFloor = '0' + floor;  // แปลงเป็น char '1','2','3'
  waitingForDest = true;

  Serial.print("[Blynk] User at floor: ");
  Serial.println(userFromFloor);

  // ส่งให้ FPGA ไปรับคนก่อน
  Serial2.write(userFromFloor);
  Serial.println("[FPGA] -> Sent pickup floor");

  // อัพเดทแอปว่ากำลังไปรับที่ชั้นนี้
  Blynk.virtualWrite(V3, String("Going to pick up at floor ") + (char)userFromFloor);
}

// V2: ผู้ใช้กดบอกว่าอยากไปชั้นไหน
// แอปควรเป็น Button/Selector ค่า 1, 2, 3
BLYNK_WRITE(V2) {
  // ต้องกด V1 ก่อนเสมอ
  if (!waitingForDest) {
    Blynk.virtualWrite(V3, "Please select your current floor first (V1)");
    return;
  }

  int floor = param.asInt();
  if (floor < 1 || floor > 3) return;

  char destFloor = '0' + floor;
  waitingForDest = false;

  Serial.print("[Blynk] Destination floor: ");
  Serial.println(destFloor);

  // ส่งชั้นปลายทางให้ FPGA
  Serial2.write(destFloor);
  Serial.println("[FPGA] -> Sent destination floor");

  Blynk.virtualWrite(V3, String("Going to floor ") + (char)destFloor);
}

// ==========================================
// 4. Setup
// ==========================================
void setup() {
  // Serial0: Debug
  Serial.begin(115200);

  // Serial2: คุยกับ FPGA (RX=16, TX=17, 9600 baud)
  Serial2.begin(9600, SERIAL_8N1, 16, 17);

  Serial.println("=== Elevator ESP32 (Blynk IoT) Starting ===");

  // เชื่อมต่อ Blynk (จัดการ WiFi ให้อัตโนมัติ)
  Blynk.begin(BLYNK_AUTH_TOKEN, ssid, password);

  Serial.println("[Blynk] Connected!");

  // แสดงสถานะเริ่มต้นในแอป
  Blynk.virtualWrite(V3, "Elevator at floor 1");
}

// ==========================================
// 5. Loop หลัก
// ==========================================
void loop() {
  // ให้ Blynk ทำงานอยู่เบื้องหลัง
  Blynk.run();

  // รับสถานะชั้นจาก FPGA ผ่าน Serial2
  // FPGA จะส่ง '1', '2', '3' มาเมื่อลิฟต์ถึงชั้นใหม่
  if (Serial2.available()) {
    char status = (char)Serial2.read();

    if (status == '1' || status == '2' || status == '3') {
      currentFloor = status;

      Serial.print("[FPGA] Elevator now at floor: ");
      Serial.println(currentFloor);

      // อัพเดทสถานะในแอป Blynk
      String msg = String("Elevator at floor ") + currentFloor;
      Blynk.virtualWrite(V3, msg);
    }
  }
}
