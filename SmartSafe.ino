// See https://www.instructables.com/Time-Based-One-Time-Password-TOTP-Smart-Safe/ for additional info. 

#include <ESP8266WiFi.h>
#include <Keypad.h>
#include <TOTP.h>
#include <ezTime.h>

// Wifi Configuration
const char* ssid     = "<SSID>";
const char* password = "<PASSWORD>";
WiFiClient WiFiclient;

// GPIO Configuration
#define relayLockPin D0
#define buzzer D7

// Keypad Configuration
const byte ROWS = 4;
const byte COLS = 3; 

char hexaKeys[ROWS][COLS] = {
  {'1', '2', '3'},
  {'4', '5', '6'},
  {'7', '8', '9'},
  {'A', '0', 'B'}
};

byte rowPins[ROWS] = {TX, RX, D2, D4}; 
byte colPins[COLS] = {D5, D1, D3}; 

unsigned long lastClickMillis = 0;
char code[6];
int codeIndex = 0;

uint8_t hmacKey[] = {
  // This key translates to My safe. Use hex representation of the plaintext key
  0x4D, 0x79, 0x20, 0x73, 0x61, 0x66, 0x65
};

TOTP totp = TOTP(
  hmacKey, 
  7 // Update this based on the length of your key. 
);

Keypad customKeypad = Keypad(makeKeymap(hexaKeys), rowPins, colPins, ROWS, COLS); 

void setup() {
  // To use TX and TX as regular GPIO. Comment if not needed. With these lines uncommented
  // you will not be able to use the Serial for debugging. 
  pinMode(TX, FUNCTION_3); 
  pinMode(RX, FUNCTION_3); 
  
  connectWifi(ssid, password);

  pinMode(relayLockPin, OUTPUT);
  digitalWrite(relayLockPin, HIGH); 
  pinMode(buzzer, OUTPUT);
  digitalWrite(buzzer, LOW);

  // ezTime
  waitForSync();
}

void connectWifi(const char* ssid, const char* password) {
  WiFi.mode(WIFI_STA);
  WiFi.hostname("mysafe");
  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
  }
}

void loop() {
  // Used to occasionally update the internal clock. 
  events();  

  char customKey = customKeypad.getKey();
  if (customKey) {
    lastClickMillis = millis();
    tone(buzzer, 2500, 20);
    code[codeIndex] = customKey;
    codeIndex = codeIndex + 1;

    if (codeIndex == 6) {
      char* tot = totp.getCode(UTC.now());
      if (strcmp(code, tot) == 0) {
        openSafe();
      } else {
        delay(100);
        tone(buzzer, 4500, 150);
        delay(200);
        tone(buzzer, 4500, 150);
        delay(200);
        tone(buzzer, 4500, 300);
      }
      codeIndex = 0;
      memset(code, 0, 8);
    }
  }

  // Ignore input and reset current password on idle. 
  if (codeIndex != 0 && (millis() - lastClickMillis > 5000)) {
    codeIndex = 0;   
    tone(buzzer, 4500, 120);
   }
}

void openSafe() {
  tone(buzzer, 3000, 1500);
  digitalWrite(relayLockPin, LOW); 
  delay(5000);            
  digitalWrite(relayLockPin, HIGH);  
}
