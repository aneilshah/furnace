// Included Library Files
#include <Arduino.h>
#include <WiFi.h>

// My Libraries
#include "event.h"

// My Files
#include "global.h"
#include "ledFunc.h"
#include "oledFunc.h"
#include "furnace.h"
#include "serverFunc.h"
#include "wifiFunctions.h"
#include "firebase.h"
#include "ntp.h"

// Global Constants
#define LOG_TIME 0  // serial log for execution times

// Configurations
#define FB_WRITE_TIME_MIN 480

const char* VERSION = "V0.01";

// PIN MAPPINGS
#define  ADC1_PIN 35
#define  BTN_PIN 0
#define  DAC_PIN 26
#define  D1_PIN 0
#define  ISR_PIN 35
#define  LED_PIN 25 
#define  TS1_PIN 14  // Touch Switch

// System States
String CONN_STATUS = "OFF";
int LOOP_COUNT = 0;
int LOOP_TIME = 1000;
int ISR_CNT = 0;
unsigned int DELAY = 92;  // avg code run time is 8ms

// Error Counts
int WIFI_ERR = 0;
int FB_ERR = 0;

// Hardware States
int ADC1_COUNT = 0;
float ADC1_VOLT = 0; 
int BTN_VAL = 0;
int D1_VAL = 2;
int TS1_VAL = 0;

// Events
Event testEvent;


void IRAM_ATTR isrFunction() {
    ISR_CNT++;
}

void setup()
{
    Serial.begin(115200);

    // Init ADC
    adcAttachPin(ADC1_PIN);
    //analogSetClockDiv(255); // 1338mS

    // Init LED
    pinMode(LED_PIN, OUTPUT); // Set GPIO25 as digital output pin

    // Init ISR
    //pinMode(ISR_PIN, INPUT_PULLUP);
    //attachInterrupt(ISR_PIN, isrFunction, RISING);

    // Init OLED
    initDisplay();

    // Set WiFi to station mode and disconnect from an AP if it was previously connected
    WiFi.mode(WIFI_STA);
    WiFi.disconnect();
    delay(500);

    Serial.println();
    Serial.println();
    Serial.println("### STARTING FURNACE MONITOR: " + String(VERSION) + " ###");
    Serial.println("Setup Complete");
    connectFurnaceWifi();

    // NTP Time Server
    setupNTP();

  // Firebase
  if (waitForWifiStable()) {
    Serial.println("Post-WiFi settle...");
    ledOn();
    delay(2000);
    ledOff();
    setupFirebase();
  } else {
    Serial.println("Deferring Firebase init to 1-minute loop");
  }

    // Init Furnace (do this last)
    initFurnace();

    // Test Functions
}

void loop()
{
    static unsigned long loopCount = 0;

    webServer();   // SERVICE HTTP AS FAST AS POSSIBLE

    loop100ms();                            // Run the 100ms loop
    if (loopCount%10 == 0) loop1Sec();      // Run the 1 sec loop
    if (loopCount%100 == 0) loop10Sec();    // Run the 10 sec loop 
    if (loopCount%600 == 0) loop1Min();     // Run the 1 minute loop 
    if (loopCount%36000 == 0) loop1Hour();  // Run the 1 hour loop 
    if (loopCount%864000 == 0) loop1Day();  // Run the 1 day loop 

    // Complete the loop
    loopCount++;
    LOOP_COUNT++;

    delay(DELAY);  // avg code run time is 8ms
}

void loop100ms() {
    readDigitalButton(); 
    readADC();
    displayText();
    processFurnaceEvent();
    updatePopupScreen();
    processLED();

    // process Events
    processLoopCheck();
    processTestEvent();
    processFBWriteEvent();
}

void loop1Sec() {
    static long count;

    // process 1 second tasks  
    readDigital();
    readTouchSwitch();
    writeDAC();
    if (wifiOK()) updateNTP();
    
    // update loop counter
    count++;
}

void loop10Sec() {
    ensureServerStarted();

    static unsigned long startTime = millis();
}

void loop1Min() {
    // reconnect if needed
    if (!firebaseOK()) {setupFirebase(); FB_ERR++;}
    if (!wifiOK()) {connectFurnaceWifi(); WIFI_ERR++;}
}


void loop1Hour() {
    Serial.println();
    Serial.println("*** RUNNING 1 HOUR LOOP");

}

void loop1Day() {
    Serial.println();
    Serial.println("*** RUNNING 1 DAY LOOP");

}

void writeDAC() {
    dacWrite(DAC_PIN, LOOP_COUNT%250);
}

void readADC() {
    long startTime=micros();
    ADC1_COUNT = analogRead(ADC1_PIN);
    ADC1_VOLT = 3.3 * (ADC1_COUNT / 4095.0);
    TLog("ADC read time: ", startTime);
    TLog("ADC: " + String(ADC1_COUNT), startTime);
    if (TEST_MODE) {

    }
    else {
      if (ADC1_COUNT > int(0.1 / 3.3 * 4096)) {
      }
    }
}

void readDigital() {
    long startTime=micros();
    D1_VAL = digitalRead(D1_PIN);
    TLog("Digital read time: ", startTime);
}

void readDigitalButton() {
    const int oldBtn = BTN_VAL;
    BTN_VAL = digitalRead(BTN_PIN);
    if (oldBtn == 1 && BTN_VAL == 0) processButton();
}

void readTouchSwitch() {
    long startTime=micros();
    TS1_VAL = touchRead(TS1_PIN);
    TLog("TS read time: ", startTime);
}

void processButton()
{
  displayPopupScreen("BUTTON PRESSED","PRG Button");
  delay(1000);
  oledMain(MAIN_TIMEOUT_SEC);
}

void processLoopCheck() {
   static unsigned long loopCount = 0;
   static unsigned long startTime = 0;
   if (loopCount % 999 == 0) {
      LOOP_TIME = int((millis() - startTime) / 100);
      Serial.println();
      Serial.println("*** Total time for 1000 loops:" + String(LOOP_TIME));
      startTime = millis();
   }
   loopCount++;
}

void processTestEvent() {
   if (testEvent.check()) {
      Serial.println();
      Serial.println("*** TEST EVENT ***");
      Serial.println("Total time for Test Event:" + String(testEvent.getDelta()) + "ms");
      Serial.println();
      testEvent.setSec(73);
   }
}

void VLog(String text) {
  if (VERBOSE) Serial.println(text);
}

void TLog(String text, long startTime) {
  if (LOG_TIME) Serial.println(text + String(micros()-startTime));
}

void textStatus() {
    float deltaMin = AVG_DELTA_MIN;
    if (deltaMin < 0.1) deltaMin = 5.0;   // default to 5 mins if there is no data
    int cph = 60 / deltaMin;
    int cpd = (int)((60.0f * 24.0f) / AVG_DELTA_MIN);
    
    String body = "Cycles: " + String(FURNACE_EVENT_CNT) + "\n";

    float monMin = float(LOOP_COUNT / 60.0 / LOOPS_PER_SEC);
    float monDay = float(LOOP_COUNT / 3600.0 / 24 / LOOPS_PER_SEC);
    if (monDay < 1) {
      body += "Monitor Time: " + String(monMin) + "m\n";
    }
    else {
      body += "Monitor Time: " + String(monDay) + "d\n";      
    }

    float furnaceOnMin = float(FURNACE_ON_TIME_LOOPS / 60.0 / LOOPS_PER_SEC);
    float furnaceOnHour = float(FURNACE_ON_TIME_LOOPS / 3600.0 / LOOPS_PER_SEC);  
    if (furnaceOnHour < 1) {
       body += "Furnace Time: " + String(furnaceOnMin) + "m\n";
    }
    else {
       body += "Furnace Time: " + String(furnaceOnHour) + "h\n";      
    }
    body += "Avg Cycle: " + String(AVG_DELTA_MIN) + "m\n";  
    body += "StDev: " + String(STDEV_DELTA_MIN) + "m\n"; 
    body += "CPD: " + String(cpd) + "\n";
    body += "TS: " + getTimestamp() + "\n";
    
    //email.sendEmail("aneilshah@yahoo.com", "Furnace Status", body);
    //email.sendEmail("7343555141@vtext.com", "Furnace", "\n" + body);     
}
