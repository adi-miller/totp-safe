// See https://www.instructables.com/Fingerprint-Safe/ for additional info. 

#include <ESP8266WiFi.h>
#include <Keypad.h>
#include <TOTP.h>
#include <ezTime.h>
#include <Adafruit_Fingerprint.h>

// Uncomment the restore support for Serial to see debug output.
//#define WithSerial

// Wifi
const char* ssid     = "<SSID>";
const char* password = "<PASSWORD>";
WiFiClient WiFiclient;

// GPIO Configuration
#define relayLockPin D1
#define buzzer D5

const int beepFactor = 10;
#define TONE_ENROLL 2093
#define TONE_ERROR 4699
#define TONE_OPEN 3136

// Fingerprint reader
SoftwareSerial mySerial(D6, D7);
Adafruit_Fingerprint finger = Adafruit_Fingerprint(&mySerial);
unsigned long lastTry = 0;
bool fpsAvailable = true;

// Keypad Configuration
const byte COLS = 3; 
#ifdef WithSerial
const byte ROWS = 2;
#else
const byte ROWS = 4;
#endif

char hexaKeys[ROWS][COLS] = {
#ifndef WithSerial
  {'1', '2', '3'},
  {'4', '5', '6'},
#endif
  {'7', '8', '9'},
  {'A', '0', 'B'}
};

byte colPins[COLS] = {D8, D0, D3}; 
#ifdef WithSerial
byte rowPins[ROWS] = {/*TX, RX, */D2, D4}; 
#else
byte rowPins[ROWS] = {TX, RX, D2, D4}; 
#endif

unsigned long openMillis = 0;
int timer = 0;
unsigned long lastClickMillis = 0;
char code[6];
int codeIndex = 0;
bool opened = false;

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
#ifdef WithSerial
  Serial.begin(9600);
  Serial.println("asd");
#else
  pinMode(TX, FUNCTION_3); 
  pinMode(RX, FUNCTION_3); 
#endif

  connectWifi(ssid, password);
  
  pinMode(buzzer, OUTPUT);
  pinMode(relayLockPin, OUTPUT);
  digitalWrite(relayLockPin, HIGH); 

  // ezTime
  waitForSync();

  finger.begin(57600);
  delay(5);

  if (!finger.verifyPassword()) {
    Serial.println("Fingerprint reader not found!");
    for (int i = 0; i < 3; i++) {
      tone(buzzer, TONE_ERROR, 500/beepFactor);
      delay(1000);
    }
    fpsAvailable = false;
  }

  finger.getTemplateCount();
  if (finger.templateCount == 0) {
    Serial.println("Fingerprint reader doesn't have any signatures!");
    for (int i = 0; i < 2; i++) {
      tone(buzzer, TONE_ERROR, 500/beepFactor);
      delay(1000);
    }
  }
}

void loop() {
  // Used to occasionally update the internal clock. 
  events();  

  // Handle Keypad clicks
  if (!opened && timer == 0) {
    char customKey = customKeypad.getKey();
    if (customKey) { 
      tone(buzzer, TONE_OPEN, 100/beepFactor);
      if (fpsAvailable)
        finger.LEDcontrol(FINGERPRINT_LED_FLASHING, 25, FINGERPRINT_LED_BLUE, 1);
      
      lastClickMillis = millis();
      code[codeIndex] = customKey;
      codeIndex = codeIndex + 1;
      Serial.print("Pin code: ");
      Serial.println(code);
  
      // Check Keypad pin
      if (codeIndex == 6) { 
        char* tot = totp.getCode(UTC.now());
        if (strcmp(code, tot) == 0) {
          openSafe();
        } else {
          wrongPin();
        }
        codeIndex = 0;
        memset(code, 0, 8);
      }
    }
  }

  // Reset pin on idle
  if (codeIndex != 0 && (millis() - lastClickMillis > 5000)) {
    Serial.println("Idle timeout. Resetting ignoring pin code.");
    codeIndex = 0;   
    memset(code, 0, 8);
    for (int i = 0; i < 2; i++) {
      tone(buzzer, TONE_ERROR, 100/beepFactor);
      delay(200);
    }
  }

  // Close lock once count down ends
  if (timer == 1) {
    Serial.println("Closed.");
    digitalWrite(relayLockPin, HIGH); 
    openMillis = 0;
    timer = 0;
    if (fpsAvailable)
      finger.LEDcontrol(FINGERPRINT_LED_BREATHING, 100, FINGERPRINT_LED_PURPLE);
  }

  // Beep while open (count down)
  if (openMillis != 0 && (millis() - openMillis > 1000)) {
    Serial.print(".");
    tone(buzzer, TONE_OPEN, 100/beepFactor);
    timer = timer - 1;
    openMillis = millis();
  } 

  // Fingerprint enroll
  if (fpsAvailable && timer == 0 && opened && millis() <= lastTry) {
    char customKey = customKeypad.getKey();
    if (customKey) { 
      tone(buzzer, TONE_ENROLL, 100/beepFactor);
      finger.LEDcontrol(FINGERPRINT_LED_FLASHING, 25, FINGERPRINT_LED_PURPLE, 1);
      
      lastClickMillis = millis();
      lastTry = millis() + 10000;
      if (customKey != 'A' && customKey != 'B' && codeIndex <= 3) {
        code[codeIndex] = customKey;
        codeIndex = codeIndex + 1;
        Serial.print("Fingerprint Id: ");
        Serial.println(code);
      } else {
        uint16_t slotId = atoi(code);
        codeIndex = 0;
        memset(code, 0, 8);
        if (slotId >= 1 && slotId <= 127) {
          Serial.print("Fingerprint Id to enrol: ");
          Serial.println(slotId);
          if (getFingerprintEnroll(slotId)) {
            tone(buzzer, 2093, 80);
            delay(120);
            tone(buzzer, 2349, 80);
            delay(120);
            tone(buzzer, 2637, 80);
          } else {
            errorBeep();
            finger.LEDcontrol(FINGERPRINT_LED_BREATHING, 100, FINGERPRINT_LED_PURPLE);
          }
        } else if (slotId == 999) {
          finger.emptyDatabase();          
          errorBeep();
          errorBeep();
        } else {
          Serial.println("Invalid fingerprint Id");
          errorBeep();
          finger.LEDcontrol(FINGERPRINT_LED_BREATHING, 100, FINGERPRINT_LED_PURPLE);
        }
      }
    }
  }

  // Check fingerprint
  if (fpsAvailable && codeIndex == 0 && timer == 0 && millis() >= lastTry) /* 5 sec after successful try, 1 sec after failed try */ {
    finger.LEDcontrol(FINGERPRINT_LED_OFF, 0, FINGERPRINT_LED_BLUE);
    opened = false;
    int fingerId = getFingerprintID();
    if (fingerId > 0) {
      openSafe();
    } else if (fingerId == -1) {
      wrongPin();
    }
  }
}

void connectWifi(const char* ssid, const char* password) {
  WiFi.mode(WIFI_STA);
  WiFi.hostname("mysafe");
  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
  }
}

void openSafe() {
  if (fpsAvailable)
    finger.LEDcontrol(FINGERPRINT_LED_GRADUAL_ON, 200, FINGERPRINT_LED_BLUE);
  digitalWrite(relayLockPin, LOW); 
  delay(100);
  Serial.print("Safe open. Couting down.");
  tone(buzzer, TONE_OPEN, 500/beepFactor);
  openMillis = millis();
  lastTry = millis() + 10000;
  opened = true;
  timer = 5;
}

void errorBeep() {
  if (fpsAvailable)
    finger.LEDcontrol(FINGERPRINT_LED_FLASHING, 25, FINGERPRINT_LED_RED, 3);
  for (int i = 0; i < 2; i++) {
    tone(buzzer, TONE_ERROR, 100/beepFactor);
    delay(200);
  }
  tone(buzzer, TONE_ERROR, 300/beepFactor);
  delay(400);
}

void wrongPin() {
  Serial.println("Wrong pin.");
  errorBeep();
}

int getFingerprintID() {
  uint8_t p = finger.getImage();
  if (p == FINGERPRINT_NOFINGER || finger.image2Tz() != FINGERPRINT_OK) { 
    return 0;
  }

  p = finger.fingerSearch();
  if (p == FINGERPRINT_OK) {
    Serial.println("Match");
    return finger.fingerID;
  } else {
    Serial.println("No match");
    lastTry = millis() + 1000;
  }

  return -1;
}

bool getFingerprintEnroll(uint8_t id) {
  Serial.print("Enroll fingerprint Id: ");
  Serial.println(id);
  
  finger.LEDcontrol(FINGERPRINT_LED_ON, 0, FINGERPRINT_LED_PURPLE);
  if (sampleFinger(1)) {
    finger.LEDcontrol(FINGERPRINT_LED_FLASHING, 25, FINGERPRINT_LED_BLUE, 1);
    tone(buzzer, TONE_ENROLL, 250);
    
    Serial.print("Remove finger.");
    while (finger.getImage() != FINGERPRINT_NOFINGER) {
      Serial.print(".");
      delay(10);
    }
    Serial.println();
    delay(500);

    finger.LEDcontrol(FINGERPRINT_LED_ON, 0, FINGERPRINT_LED_PURPLE);
    if (sampleFinger(2)) {
      finger.LEDcontrol(FINGERPRINT_LED_FLASHING, 25, FINGERPRINT_LED_BLUE, 1);
      tone(buzzer, TONE_ENROLL, 250);
      delay(500);

      Serial.println("Creating model");
      int p = finger.createModel();
      if (p == FINGERPRINT_OK) {
        Serial.println("Prints matched!");
        p = finger.storeModel(id);
        if (p == FINGERPRINT_OK) {
          finger.LEDcontrol(FINGERPRINT_LED_FLASHING, 25, FINGERPRINT_LED_BLUE, 3);
          Serial.println("Stored!");
          return true;
        } else {
          Serial.println("Error storing fingerprint model");
        }      
      } else {
        Serial.println("Fingerprints did not match or other error");
      }
    }
  }
  return false;
}

bool sampleFinger(uint8_t slot) {
  for (int i = 0; i < slot; i++) {
    tone(buzzer, TONE_ENROLL, 500);
    delay(800);
  }
  int p = -1;
  while (p != FINGERPRINT_OK && p != FINGERPRINT_PACKETRECIEVEERR) {
    p = finger.getImage();
//    tone(buzzer, 2093, 100);
    switch (p) {
    case FINGERPRINT_OK:
      Serial.println("Image taken");
      break;
    case FINGERPRINT_NOFINGER:
      Serial.print(".");
      break;
    case FINGERPRINT_PACKETRECIEVEERR:
      Serial.println("Communication error");
      break;
    case FINGERPRINT_IMAGEFAIL:
      Serial.println("Imaging error");
      return false;
    default:
      Serial.println("Unknown error");
      return false;
    }
  }

  // OK success!

  p = finger.image2Tz(slot);
  switch (p) {
    case FINGERPRINT_OK:
      Serial.println("Image converted");
      break;
    case FINGERPRINT_IMAGEMESS:
      Serial.println("Image too messy");
      break;
    case FINGERPRINT_PACKETRECIEVEERR:
      Serial.println("Communication error");
      break;
    case FINGERPRINT_FEATUREFAIL:
      Serial.println("Could not find fingerprint features");
      break;
    case FINGERPRINT_INVALIDIMAGE:
      Serial.println("Could not find fingerprint features");
      break;
    default:
      Serial.println("Unknown error");
      break;
  }
  return (p == FINGERPRINT_OK);
}
