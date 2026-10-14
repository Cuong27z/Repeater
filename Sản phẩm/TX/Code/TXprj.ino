#include <Arduino.h>
#include <U8g2lib.h>
#include <RF24.h>
#ifdef U8X8_HAVE_HW_I2C
#include <Wire.h>
#endif
#define BUTTON_UP_PIN     26
#define BUTTON_DOWN_PIN   27
#define BUTTON_SELECT_PIN 25
#define RF_CE_PIN   4
#define RF_CSN_PIN  5

const byte TX_CHANNEL = 120;

//Khởi tạo OLED và RF24
U8G2_SSD1306_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0, U8X8_PIN_NONE);
RF24 radio(RF_CE_PIN, RF_CSN_PIN);
const byte addresses[][6] = {"1Node","2Node"};


// Main menu items (2 lựa chọn)
const char* menu_items[] = {
  "Jammer",
  "Send packet"
};
const int NUM_MENU = sizeof(menu_items) / sizeof(menu_items[0]);

// Mỗi menu con có 3 trạng thái
const int NUM_STATUS = 3;
const char* status_items[NUM_MENU][NUM_STATUS] = {
  { "Start jamming",  "Stop jamming",   "Back" }, 
  { "Start send",     "Stop send",      "Back" } 
};


struct Payload {
  byte id;         
  char data[31];   
};

// State machine
enum State { MAIN_MENU, STATUS_MENU };
State state = MAIN_MENU;

int currentMenu = 0;
int currentStatus = 0;

bool upPressed = false;
bool downPressed = false;
bool selectPressed = false;
bool isSendingPackets = false;
int packetCounter = 1;
unsigned long lastSendTime = 0;
const unsigned long SEND_INTERVAL = 1500;

// Hiển thị kết quả gửi lên OLED
void displayResult(bool ok) {
  u8g2.clearBuffer();
  u8g2.setFont(u8g_font_7x14B);
  u8g2.drawStr(0, 32, ok ? "Command OK" : "Command FAIL");
  u8g2.sendBuffer();
}

// Vẽ menu chính
void displayMenu() {
  u8g2.clearBuffer();
  for (int i = 0; i < NUM_MENU; i++) {
    u8g2.setFont(i == currentMenu ? u8g_font_7x14B : u8g_font_7x14);
    int y = 20 + i * 20;
    const char* txt = menu_items[i];
    if (i == currentMenu) {
      int w = u8g2.getStrWidth(txt);
      int h = 14;
      int pad = 4;
      u8g2.drawFrame(0, y - h, w + pad, h + pad);
    }
    u8g2.drawStr(2, y, txt);
  }

  // Hiển thị trạng thái gửi
  if (isSendingPackets) {
    u8g2.setFont(u8g_font_6x10);
    u8g2.drawStr(70, 60, "Sending...");
  }
  u8g2.sendBuffer();
}

// Vẽ menu trạng thái
void displayStatusMenu() {
  u8g2.clearBuffer();
  for (int i = 0; i < NUM_STATUS; i++) {
    u8g2.setFont(i == currentStatus ? u8g_font_7x14B : u8g_font_7x14);
    int y = 20 + i * 20;
    const char* txt = status_items[currentMenu][i];
    if (i == currentStatus) {
      int w = u8g2.getStrWidth(txt);
      int h = 14;
      int pad = 4;
      u8g2.drawFrame(0, y - h, w + pad, h + pad);
    }
    u8g2.drawStr(2, y, txt);
  }
  u8g2.sendBuffer();
}


void sendCommand(const char* command) {
  Payload cmdPayload;
  cmdPayload.id = 0x00;
  strncpy(cmdPayload.data, command, sizeof(cmdPayload.data) - 1);
  cmdPayload.data[sizeof(cmdPayload.data) - 1] = '\0'; 
  bool ok = radio.write(&cmdPayload, sizeof(cmdPayload));
  Serial.printf("Sent Command: ID=0x%02X, Data='%s' -> %s\n", cmdPayload.id, cmdPayload.data, ok ? "OK" : "FAIL");
  displayResult(ok);
  delay(1000);
  state = MAIN_MENU;
}

// Hàm xử lý nhấn nút
void handleButtons() {
  int upState = digitalRead(BUTTON_UP_PIN);
  int downState = digitalRead(BUTTON_DOWN_PIN);
  int selState = digitalRead(BUTTON_SELECT_PIN);

  if (upState == LOW && !upPressed) {
    upPressed = true;
    if (state == MAIN_MENU) currentMenu = (currentMenu + NUM_MENU - 1) % NUM_MENU;
    else currentStatus = (currentStatus + NUM_STATUS - 1) % NUM_STATUS;
  } else if (upState == HIGH) upPressed = false;


  if (downState == LOW && !downPressed) {
    downPressed = true;
    if (state == MAIN_MENU) currentMenu = (currentMenu + 1) % NUM_MENU;
    else currentStatus = (currentStatus + 1) % NUM_STATUS;
  } else if (downState == HIGH) downPressed = false;


  if (selState == LOW && !selectPressed) {
    selectPressed = true;
    if (state == MAIN_MENU) {
      state = STATUS_MENU;
      currentStatus = 0;
    } else {
      const char* selected_action = status_items[currentMenu][currentStatus];
      
      if (strcmp(selected_action, "Back") == 0) {
        state = MAIN_MENU;
      } else if (strcmp(selected_action, "Start send") == 0) {
        if (!isSendingPackets) {
          isSendingPackets = true;
          packetCounter = 1;
          lastSendTime = millis(); 
        }
        sendCommand(selected_action); 
      } else if (strcmp(selected_action, "Stop send") == 0) {
        isSendingPackets = false;
        sendCommand(selected_action); 
      } else {
        sendCommand(selected_action);
      }
    }
  } else if (selState == HIGH) selectPressed = false;
}

void setup() {
  Serial.begin(115200);
  while (!Serial) {}

  u8g2.begin();
  u8g2.setContrast(128);

  pinMode(BUTTON_UP_PIN, INPUT_PULLUP);
  pinMode(BUTTON_DOWN_PIN, INPUT_PULLUP);
  pinMode(BUTTON_SELECT_PIN, INPUT_PULLUP);

  radio.begin();
  radio.setAutoAck(true);
  radio.setRetries(15, 5);
  radio.stopListening();
  radio.setPALevel(RF24_PA_MAX);
  radio.setDataRate(RF24_250KBPS);
  radio.setCRCLength(RF24_CRC_16);
  
  radio.setChannel(TX_CHANNEL);
  radio.openWritingPipe(addresses[0]);
  Serial.println("--- TX Controller Initialized ---");
}

void loop() {
  handleButtons();
  if (isSendingPackets) {
    if (millis() - lastSendTime >= SEND_INTERVAL) {
      Payload dataPayload;
      dataPayload.id = 0x01;
      sprintf(dataPayload.data, "%d", packetCounter);

      bool ok = radio.write(&dataPayload, sizeof(dataPayload));
      Serial.printf("Sending Packet: ID=0x%02X, Data='%s' -> %s\n", dataPayload.id, dataPayload.data, ok ? "OK" : "FAIL");

      lastSendTime = millis();
      packetCounter++;

      if (packetCounter > 500) {
        isSendingPackets = false;
        Serial.println("--- Finished sending 500 packets ---");
      }
    }
  }

  if (state == MAIN_MENU) {
    displayMenu();
  } else {
    displayStatusMenu();
  }
  delay(100);
}