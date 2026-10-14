#include <Arduino.h>
#include <RF24.h>
#include "esp_bt.h"
#include "esp_wifi.h"
#include <SPI.h>

const byte RX_CHANNEL = 120;
const byte FORWARDING_CHANNEL = 92;
const byte addresses[][6] = {"1Node", "2Node"};
byte ch = 45;
// --- Định nghĩa 2 module RF24 ---
RF24 rxRadio(4, 5);
SPIClass *hspi = nullptr;
RF24 utilityRadio(16, 15);

struct Payload {
  byte id;
  char data[31];
};

// Enum cho các chế độ
enum JammerMode { NONE, JAMMING };
JammerMode currentMode = NONE;
unsigned long n = 0;

// Hàm cấu hình utilityRadio cho chế độ FORWARDING
void configureForForwarding() {
  Serial.println("Action: Configuring utilityRadio for FORWARDING...");
  utilityRadio.setAutoAck(true); 
  utilityRadio.setRetries(15, 5);
  utilityRadio.setDataRate(RF24_250KBPS);
  utilityRadio.setCRCLength(RF24_CRC_16);
}

// Hàm cấu hình utilityRadio cho chế độ JAMMING
void configureForJamming() {
  Serial.println("Action: Configuring utilityRadio for JAMMING...");
  utilityRadio.setAutoAck(false); 
  utilityRadio.setRetries(0, 0);
  utilityRadio.setDataRate(RF24_2MBPS);
  utilityRadio.setCRCLength(RF24_CRC_DISABLED);
}


// Hàm forward payload đến anten đích
void forwardPayload(const Payload& payload) {
  utilityRadio.stopConstCarrier(); 
  utilityRadio.powerUp();
  delay(5);
  configureForForwarding();
  utilityRadio.stopListening();
  utilityRadio.setChannel(FORWARDING_CHANNEL);
  utilityRadio.openWritingPipe(addresses[1]);

  bool ok = utilityRadio.write(&payload.data, sizeof(payload.data));
  Serial.printf("Forwarding Payload: ID=0x%02X, Data='%s' -> %s\n", payload.id, payload.data, ok ? "OK" : "FAIL");
  utilityRadio.stopListening();
}


// Hàm init SPI & cấu hình utilityRadio
void initUtilityRadio(void) {
  hspi = new SPIClass(HSPI);
  hspi->begin();
  if(utilityRadio.begin(hspi)) {
    Serial.println(">> Utility Radio (Jam/Fwd) init OK");
    // Các cài đặt chung ban đầu
    utilityRadio.setPALevel(RF24_PA_HIGH, true);
    utilityRadio.stopListening();
  } else {
    Serial.println(">> Utility Radio init FAIL");
  }
}

// Hàm Tắt jamming trên utilityRadio
void stopJamming() {
  utilityRadio.stopConstCarrier();
  currentMode = NONE;
  Serial.println("Action: Stopping Jammer");
}

//  Hàm phá sóng từ kênh 1 đến 80
void performJamming() {
  for (int ch = 1; ch <= 80; ch++) {
    utilityRadio.setChannel(ch);
  }
}

void setup(void) {
  Serial.begin(115200);
  while (!Serial);
  Serial.println("\n--- RX/Jammer Initializing ---");
  esp_bt_controller_deinit();
  esp_wifi_stop();
  esp_wifi_deinit();
  
  initUtilityRadio();

  // Cấu hình RX-only radio
  if (rxRadio.begin()) {
    Serial.println(">> RX Radio init OK");
    rxRadio.setAutoAck(true);
    rxRadio.setPALevel(RF24_PA_MAX);
    rxRadio.setDataRate(RF24_250KBPS);
    rxRadio.setCRCLength(RF24_CRC_16);
    rxRadio.setChannel(RX_CHANNEL);
    rxRadio.openReadingPipe(1, addresses[0]);
    rxRadio.startListening();
    Serial.print(">> RX Radio listening on channel: ");
    Serial.println(RX_CHANNEL);
  } else {
    Serial.println(">> RX Radio init FAIL");
  }
  Serial.println("--- Setup complete, waiting for commands... ---");
}

  void loop(void) {
  if (rxRadio.available()) {
    Payload receivedPayload;
    rxRadio.read(&receivedPayload, sizeof(receivedPayload));

    n++;
    Serial.printf("[Pkt %lu] Received Payload: ID=0x%02X, Data='%s'\n", n, receivedPayload.id, receivedPayload.data);
    if (receivedPayload.id == 0x00) {
      if (strcmp(receivedPayload.data, "Start jamming") == 0) {
        if (currentMode != JAMMING) {
          configureForJamming();
          utilityRadio.powerUp();
          utilityRadio.startConstCarrier(RF24_PA_HIGH, ch);
          delayMicroseconds(150);
          currentMode = JAMMING;
          Serial.println("Action: Starting Jammer Sweep");
        }
      } else if (strcmp(receivedPayload.data, "Stop jamming") == 0) {
        stopJamming();
      }
    } 
    else if (receivedPayload.id == 0x01) { 
        if (currentMode == JAMMING) {
            stopJamming();
        }
        Serial.println("Action: Forwarding data packet...");
        forwardPayload(receivedPayload);
    }
  }
  if (currentMode == JAMMING) {
    performJamming();
  }
}



 

