#include <WiFi.h>
#include <PubSubClient.h>

// ==========================================
// 1. ตั้งค่า WiFi และ MQTT
// ==========================================
const char* ssid        = "YOUR_WIFI_NAME";
const char* password    = "YOUR_WIFI_PASSWORD";
const char* mqtt_server = "broker.hivemq.com";
const int   mqtt_port   = 1883;

// Topic รับคำสั่งไปชั้นไหน
const char* mqtt_topic_cmd    = "myproject/elevator/command";
// Topic ส่งสถานะชั้นปัจจุบันกลับไปแอป
const char* mqtt_topic_status = "myproject/elevator/status";

WiFiClient   espClient;
PubSubClient client(espClient);

// สำหรับ Non-blocking Reconnect
unsigned long lastReconnectAttempt = 0;

// สถานะ WiFi
bool wifiConnected = false;

// ==========================================
// 2. Callback เมื่อมีคำสั่งจาก MQTT
// ==========================================
void callback(char* topic, byte* payload, unsigned int length) {
  if (length == 0) return;

  char cmd = (char)payload[0];
  Serial.print("[MQTT] Received: ");
  Serial.println(cmd);

  if (cmd == '1' || cmd == '2' || cmd == '3') {
    Serial2.write(cmd);
    Serial.println("[MQTT] -> Sent to FPGA");
  } else {
    Serial.println("[MQTT] -> Unknown command, ignored");
  }
}

// ==========================================
// 3. เชื่อมต่อ WiFi
// ==========================================
bool setup_wifi() {
  Serial.print("[WiFi] Connecting to ");
  Serial.println(ssid);

  WiFi.begin(ssid, password);

  // รอ 10 วินาที ถ้าไม่ได้ก็ใช้ offline
  unsigned long startTime = millis();
  while (WiFi.status() != WL_CONNECTED) {
    if (millis() - startTime > 10000) {
      Serial.println("[WiFi] Failed. Running OFFLINE mode.");
      return false;
    }
    delay(500);
    Serial.print(".");
  }

  Serial.println("");
  Serial.print("[WiFi] Connected. IP: ");
  Serial.println(WiFi.localIP());
  return true;
}

// ==========================================
// 4. เชื่อมต่อ MQTT (Non-blocking)
// ==========================================
bool reconnect() {
  // เช็ค WiFi ก่อนเสมอ
  if (WiFi.status() != WL_CONNECTED) {
    wifiConnected = setup_wifi();
    if (!wifiConnected) return false;
  }

  Serial.print("[MQTT] Connecting...");

  String clientId = "ESP32Elevator-";
  clientId += String(random(0xffff), HEX);

  if (client.connect(clientId.c_str())) {
    Serial.println("connected");
    client.subscribe(mqtt_topic_cmd);
    return true;
  } else {
    Serial.print("[MQTT] Failed, rc=");
    Serial.println(client.state());
    return false;
  }
}

// ==========================================
// 5. ส่งสถานะชั้นขึ้น MQTT
// ==========================================
void publishStatus(char floorChar) {
  if (!wifiConnected || !client.connected()) return;

  char msg[2] = {floorChar, '\0'};
  client.publish(mqtt_topic_status, msg, true);  // retain=true แอปจะได้ค่าล่าสุดทันทีที่เปิด

  Serial.print("[MQTT] Status published: Floor ");
  Serial.println(floorChar);
}

// ==========================================
// 6. Setup
// ==========================================
void setup() {
  // Serial0: Debug ผ่านคอม
  Serial.begin(115200);

  // Serial2: คุยกับ FPGA (TX=17 ส่งคำสั่ง, RX=16 รับสถานะ)
  Serial2.begin(9600, SERIAL_8N1, 16, 17);

  Serial.println("=== Elevator ESP32 Starting ===");

  // พยายามต่อ WiFi (ถ้าไม่ได้ก็ offline)
  wifiConnected = setup_wifi();

  if (wifiConnected) {
    client.setServer(mqtt_server, mqtt_port);
    client.setCallback(callback);
  } else {
    Serial.println("[INFO] Offline mode: Use Serial Monitor to send commands (1/2/3)");
  }
}

// ==========================================
// 7. Loop หลัก
// ==========================================
void loop() {

  // --- Online Mode: จัดการ MQTT ---
  if (wifiConnected) {
    if (!client.connected()) {
      unsigned long now = millis();
      // พยายาม reconnect ทุก 5 วินาที (Non-blocking)
      if (now - lastReconnectAttempt > 5000) {
        lastReconnectAttempt = now;
        reconnect();
      }
    } else {
      client.loop();
    }
  }

  // --- Offline Mode: รับคำสั่งจาก Serial Monitor ---
  if (!wifiConnected && Serial.available()) {
    char cmd = (char)Serial.read();
    if (cmd == '1' || cmd == '2' || cmd == '3') {
      Serial2.write(cmd);
      Serial.print("[Offline] Sent to FPGA: Floor ");
      Serial.println(cmd);
    }
  }

  // --- รับสถานะชั้นจาก FPGA ผ่าน Serial2 ---
  // FPGA ส่ง ASCII '1','2','3' มาเมื่อชั้นเปลี่ยน
  if (Serial2.available()) {
    char status = (char)Serial2.read();
    if (status == '1' || status == '2' || status == '3') {
      Serial.print("[FPGA] Current Floor: ");
      Serial.println(status);

      // ส่งสถานะขึ้น MQTT ให้แอปรู้
      publishStatus(status);
    }
  }
}
