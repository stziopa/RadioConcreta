/*

2025-11-13

Radio Concreta
2025, Stefano Manconi

Premio Nazionale delle Arti - XIX edizione
Sezione Interpretazione Musicale
Sottosezione “Musica Elettronica e Nuove
Tecnologie”
Categoria D: Installazioni Sonore d’Arte
Sassari, 13-14 novembre 2025


CREDITS:
The sketch is based on the WiFi microphone application by: 
M. Mahdi K. Kanan - WiCardTech Engineering Group (https://wicard.net)
Sleep modes and power management based on: 
PALM01 by Vaclav Pelousek: https://github.com/vaclav-bastl/PALM01

Changelog v5:
* Server.handleClient() moved at beginning of loop()
* removed LOOP_10_S, MEM_SAVED and ipTimer

*/

#include <time.h>
#include <Arduino.h>
#include <WiFi.h>         //from AC.h
#include <WebServer.h>    //from AC.h

WebServer  Server;        //from AC.h

//Wifi credentials
const char* ssid = "SSID";      //replace with wifi credentials
const char* password = "PASSWORD";
//const char* MyHostName = "HOSTNAME";     //rename accordingly

#define MIC_INPUT 1 //corresponds to A0
#define LDO_EN D3   //pin to control LDO

#define BATTPWR //comment out when using USB as main power source

#define NUMBER_OF_BUTTONS 1
bool buttonState[NUMBER_OF_BUTTONS];
bool lastButtonState[NUMBER_OF_BUTTONS];
bool justPressed[NUMBER_OF_BUTTONS];
bool justReleased[NUMBER_OF_BUTTONS];

#define BUTTON_C 0  //number of the button in the array NOT GPIO!

#define WAKE_BUTTON_PIN 9

long longPressTime = 0;

const uint8_t buttonPin[NUMBER_OF_BUTTONS] = {D9};  //4, 36, 39, 34 };

void readButtons() {
  for (uint8_t i = 0; i < NUMBER_OF_BUTTONS; i++) {
    lastButtonState[i] = buttonState[i];
    buttonState[i] = !digitalRead(buttonPin[i]);
    justPressed[i] = buttonState[i] && !lastButtonState[i];
    justReleased[i] = !buttonState[i] && lastButtonState[i];

    //if (justPressed[i]) Serial.println(i);   
    if (justPressed[i]) Serial.println("button pressed");
    if (justReleased[i]) Serial.println("button released");
  }
}

void handleButtons() {
  if (justPressed[BUTTON_C]) {

    longPressTime = millis();
    //esp_sleep_enable_timer_wakeup(10000000);  // 1 sec
  }
  if (buttonState[BUTTON_C] && ((millis() - longPressTime)) > 2000) {
//enable sleep functions only if battery was defined
#ifdef BATTPWR
    Serial.println("Entering sleep mode");
    sleep();
#endif
    Serial.println("Can't sleep: check BATTPWR");
    //LED error indication
    digitalWrite(LED_BUILTIN, LOW);
    delay(500);
    digitalWrite(LED_BUILTIN, HIGH);
    delay(500);
  }
}


void wakeUp() {
  pinMode(GPIO_NUM_8, INPUT_PULLUP);  // make sure pull-up is active

  if (esp_sleep_get_wakeup_cause() == ESP_SLEEP_WAKEUP_EXT0) {
    // Give time for voltage to stabilize
    delay(50);  // debounce, helps with false wake

    pinMode(LED_BUILTIN, OUTPUT);
    digitalWrite(LED_BUILTIN, LOW); //turn LED on

    unsigned long t0 = millis();

    // Wait to see if button is held long enough
    while (digitalRead(D9) == LOW) {
      if (millis() - t0 > 1000) {
        // Long press confirmed
        break;
      }
      delay(10);
    }

    // If button released too early → go back to sleep
    if (millis() - t0 < 1000) {
      digitalWrite(LED_BUILTIN, HIGH); //turn LED off

      esp_sleep_enable_ext0_wakeup(GPIO_NUM_8, 0);  // re-enable wake
      esp_deep_sleep_start();
    }
    // Otherwise continue booting
  }
}


void sleep() {
  digitalWrite(LDO_EN, LOW);        //disable LDO
  digitalWrite(LED_BUILTIN, HIGH);  //turn LED off
  delay(125);

  //blink 4 times
  digitalWrite(LED_BUILTIN, LOW);
  delay(125);
  digitalWrite(LED_BUILTIN, HIGH);
  delay(125);
  digitalWrite(LED_BUILTIN, LOW);
  delay(125);
  digitalWrite(LED_BUILTIN, HIGH);
  delay(125);
  digitalWrite(LED_BUILTIN, LOW);
  delay(125);
  digitalWrite(LED_BUILTIN, HIGH);
  delay(125);
  digitalWrite(LED_BUILTIN, LOW);
  delay(125);
  digitalWrite(LED_BUILTIN, HIGH);


  pinMode(GPIO_NUM_8, INPUT_PULLUP);
  esp_sleep_enable_ext0_wakeup(GPIO_NUM_8, 0);  // Wake on button press
  Serial.println("Going to sleep... bye");
  esp_deep_sleep_start();                      //longpress to sleep
  Serial.println("if you read this something went wrong");
}

void initHw() {
  for (uint8_t i = 0; i < NUMBER_OF_BUTTONS; i++) pinMode(buttonPin[i], INPUT_PULLUP);
  pinMode(LDO_EN, OUTPUT);
  digitalWrite(LDO_EN, HIGH);   //enable LDO

  pinMode(MIC_INPUT, INPUT);    //set microphone Pin as input ?

  //init LED as OFF
  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, HIGH);
  delay(250);

  //blink two times
  digitalWrite(LED_BUILTIN, LOW);
  delay(250);
  digitalWrite(LED_BUILTIN, HIGH);
  delay(250);
  digitalWrite(LED_BUILTIN, LOW);
  delay(250);
  digitalWrite(LED_BUILTIN, HIGH);
  delay(250);

  Serial.println("Hardware initialized");
}


//******************** AC lines pasted here ***********************
//#define LOOP_10_S 5500000//2800000        //possibly remove?
//#define MEM_SAVED   0xAB                  //possibly remove?

#define AUDIO_60KBPS_TIMING     163 //166
#define AUDIO_70KBPS_TIMING     139 //142
#define AUDIO_80KBPS_TIMING     121 //125

//we initialize the values as we don't store them to the EEPROM
unsigned char ucWMBitrate = 0;      //0=72Kbps, 1=84Kbps, 2=96Kbps
unsigned char ucWMPacketLen = 1;    //0=1min, 1=10min, 2=30min, 3=60min
unsigned char ucWMAmp = 0;          //0=x1 gain, 1=x2 gain, 2=x4 gain

//******************** End of AC lines ***********************
unsigned char buff[8000];
unsigned long int  ulTimer1;
unsigned long int  ulTimer2;


//Connection event handlers:
void ConnectedToAP_Handler(WiFiEvent_t wifi_event, WiFiEventInfo_t wifi_info) {
  Serial.println("Connected To The WiFi Network");
  digitalWrite(LED_BUILTIN, HIGH);
}
 
void GotIP_Handler(WiFiEvent_t wifi_event, WiFiEventInfo_t wifi_info) {
  Serial.print("ESP32 MAC Address: ");
  Serial.println(WiFi.macAddress());
  Serial.print("Local ESP32 IP: ");
  Serial.println(WiFi.localIP());
  // Print The RSSI (Received Signal Strength Indicator)
  Serial.print("RRSI: ");
  Serial.print(WiFi.RSSI());
  Serial.println(" dBm");
  digitalWrite(LED_BUILTIN, HIGH);  
}
 
void WiFi_Disconnected_Handler(WiFiEvent_t wifi_event, WiFiEventInfo_t wifi_info) {
  Serial.println("Disconnected From WiFi Network");
  // Attempt Re-Connection
  WiFi.begin(ssid, password);
  digitalWrite(LED_BUILTIN, LOW);
}


void setup()
{
//enable sleep functions only if battery was defined
#ifdef BATTPWR
  wakeUp();
#endif

  Serial.begin(115200);
  delay(500);
  Serial.println("Starting");
  Serial.print("Current time is: ");
  Serial.println(millis());

  initHw();

  //WiFi.setHostname(MyHostName); //enable later
  WiFi.mode(WIFI_STA);
  WiFi.onEvent(ConnectedToAP_Handler, ARDUINO_EVENT_WIFI_STA_CONNECTED);
  WiFi.onEvent(GotIP_Handler, ARDUINO_EVENT_WIFI_STA_GOT_IP);
  WiFi.onEvent(WiFi_Disconnected_Handler, ARDUINO_EVENT_WIFI_STA_DISCONNECTED);
  WiFi.begin(ssid, password);
  Serial.println("Connecting to WiFi Network ..");

  //enable server
  Server.on("/", rootPage);
  Server.on("/s", streamPage);
  Server.on("/cal", calPage);

  delay(1000);  
  Server.begin();
  Serial.println("HTTP server started");
}

void loop()
{
  Server.handleClient();

  //this functions are perfomed only if handleClient() is free
  readButtons();
  handleButtons();
}
