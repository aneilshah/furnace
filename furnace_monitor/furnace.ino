// Libraries
#include <event.h>

// Local files
#include "global.h"
#include "ntp.h"
#include "firebase.h"
#include "ledFunc.h"
#include "test_mode.h"

extern void VLog(String);
extern void Log(String);

#define PREHEAT_TIME 70     // sec
#define COOLDOWN_TIME 100   // sec

// FURNACE Variables
float DELTA_MIN = 0.0;
long DELTA_SEC = 0;
int FURNACE_EVENT_CNT = 0;
int FURNACE_EVENT = FALSE;
long FURNACE_ON_TIME_LOOPS = 0;
int LAST_FURNACE_ON_TIME_LOOPS = 0;
long FURNACE_OFF_TIME_LOOPS = 0;
long LAST_FURNACE_OFF_TIME_LOOPS = 0;
long LAST_EVENT = 0;
long AVG_DELTA_SEC = 0;
float AVG_DELTA_MIN = 0;
float STDEV_DELTA_MIN = 0;
float STDEV_DELTA_SEC = 0;
long AVG_COUNT = 0;
int ON_PCT = 0;
int OFF_PCT = 0;
long LastFurnaceOnTimeLoops = 0;
long LastFurnaceOffTimeLoops = 0;


// Local Variables
int preHeatCount = 0;
int coolDownCount = 0;
int FurnaceState= FURN_OFF;
int firstEvent = 1;    // Dont update average on first event
int stateTimerLoops = 0;

Buffer CycleData;
Buffer OnData;
Buffer OffData;
Buffer CountData;
DataStore Counts;

// Events
Event writeFirebaseEvent;

int getFurnaceState() {
   return FurnaceState;
}

void processFurnaceEvent() {
    if (FurnaceState== FURN_OFF) processFurnOff();
    else if (FurnaceState== FURN_PRE_HEAT) processFurnPreHeat();
    else if (FurnaceState== FURN_ON) processFurnOn();
    else if (FurnaceState== FURN_COOL_DOWN) processFurnCoolDown();
    else gotoFurnOff();    // Should never get here, default to OFF

    stateTimerLoops++;
}

void processFurnOff() {
    FURNACE_OFF_TIME_LOOPS++;
    LAST_FURNACE_OFF_TIME_LOOPS++;
    if (ADC1_COUNT > FURN_ON_COUNTS) gotoFurnPreHeat();
}

void gotoFurnPreHeat() {
    FurnaceState= FURN_PRE_HEAT;
    ledBlinkFast();
    preHeatCount = PREHEAT_TIME * LOOPS_PER_SEC;    // 30 seconds of pre-heat 
    Log("Furnace Pre-Heat " + String(PREHEAT_TIME) + " sec");
    writeEventData("PreHeat");
    stateTimerLoops = 0;  
}

void processFurnPreHeat() {
    FURNACE_ON_TIME_LOOPS++;
    LAST_FURNACE_ON_TIME_LOOPS++;
    preHeatCount--;
    if (preHeatCount == 0) gotoFurnOn();
    else if (checkForOff()) gotoFurnOff();
}

void gotoFurnOn() {
    FurnaceState= FURN_ON;
    ledOn(); 
    Log("Furnace ON");
    writeEventData("On");      
    CountData.reset();
    Counts.reset();  
    stateTimerLoops = 0;
}

void processFurnOn() {
   FURNACE_ON_TIME_LOOPS++;
   LAST_FURNACE_ON_TIME_LOOPS++;
   CountData.addData(ADC1_COUNT);
   float avg = CountData.avg();
   Counts.addData(avg);
   if (checkForOff()) gotoFurnCoolDown();
}

bool checkForOff() {
   CountData.addData(ADC1_COUNT);
   float avg = CountData.avg();
   Counts.addData(avg);
   return ((avg < FURN_OFF_COUNTS) && (CountData.count() > 5));
}

void gotoFurnCoolDown() {
    FurnaceState= FURN_COOL_DOWN;
    ledBlinkSlow();
    coolDownCount = COOLDOWN_TIME * LOOPS_PER_SEC;    // 30 seconds of Cool Down   
    Log("Furnace Cool Down " + String(PREHEAT_TIME) + " sec");
    writeEventData("CoolDown");
    stateTimerLoops = 0;   
}

void processFurnCoolDown() {
    FURNACE_OFF_TIME_LOOPS++;
    LAST_FURNACE_OFF_TIME_LOOPS++;
    coolDownCount--;
    
    if (coolDownCount == 0) {
      saveCycleData();
      gotoFurnOff();
    }
}

void saveCycleData() {
    FURNACE_EVENT_CNT++;
    Serial.println("Cycle Complete: " + String(FURNACE_EVENT_CNT) + "  TIMESTAMP: " + String(float(LOOP_COUNT / 60.0 / LOOPS_PER_SEC)) + "min" +
      "  " + getTimestamp());
    DELTA_SEC = (LOOP_COUNT - LAST_EVENT) / 10;
    LAST_EVENT = LOOP_COUNT;
    DELTA_MIN = DELTA_SEC / 60.0;

    if (!firstEvent) updateAvgCycleTime();   // Dont average first event
    else {
      VLog("Ignoring first cycle");
      firstEvent = 0;                         // Clear flag
    }
    VLog("");

    LastFurnaceOnTimeLoops = LAST_FURNACE_ON_TIME_LOOPS;
    LAST_FURNACE_ON_TIME_LOOPS = 0;
    LastFurnaceOffTimeLoops = LAST_FURNACE_OFF_TIME_LOOPS;
    LAST_FURNACE_OFF_TIME_LOOPS = 0;
}

void gotoFurnOff() {
    FurnaceState= FURN_OFF;
    ledOff();
    preHeatCount = 0;
    coolDownCount = 0;
    Log("Furnace OFF");
    writeEventData("Off");       
    stateTimerLoops = 0;
}

void updateAvgCycleTime() {
    VLog("Updating Averages...");
    CycleData.addData(DELTA_SEC);
    OnData.addData(LAST_FURNACE_ON_TIME_LOOPS);
    OffData.addData(LAST_FURNACE_OFF_TIME_LOOPS);
    VLog(CycleData.dataText());
    AVG_DELTA_SEC = CycleData.avg();
    AVG_DELTA_MIN = AVG_DELTA_SEC / 60.0;
    STDEV_DELTA_SEC = CycleData.stdev();
    STDEV_DELTA_MIN = CycleData.stdev() / 60.0;
    ON_PCT = 0;
    if (CycleData.avg() > 0) ON_PCT = int(100.0 * OnData.avg() / CycleData.avg() / LOOPS_PER_SEC);
    OFF_PCT = 100 - ON_PCT;
    VLog("");
}

void initFurnace() {
    FurnaceState= FURN_OFF;
    
    // TEST MODE
    if (TEST_MODE) {
      initFurnaceSim();
      writeFirebaseEvent.setMin(6);   // First write after 6 mins in test mode
    }

    // NORMAL MODE
    else {
      writeFirebaseEvent.setMin(FB_WRITE_TIME_MIN);
    }
}

void processFBWriteEvent() {
    if (writeFirebaseEvent.check()) {
       int nextWrite = FB_WRITE_TIME_MIN;
       if (TEST_MODE) nextWrite = 5;  // 5 minutes for test mode
       writeFurnaceData();
       writeFirebaseEvent.setMin(nextWrite);
       Serial.println("Next FB Write: " + String(nextWrite) + "min");
       Serial.println();
    }
}

String FurnStateTxt() {
    if (FurnaceState== FURN_OFF) return String("Furn OFF");
    else if (FurnaceState== FURN_PRE_HEAT) return String("Pre-Heat");
    else if (FurnaceState== FURN_COOL_DOWN) return String("Fan Only");
    else if (FurnaceState== FURN_ON) return String("Furn ON"); 
    return "ERROR"; 
}

String StateClock() {
   int mins = stateTimerLoops / 60 / LOOPS_PER_SEC;
   int sec = ((stateTimerLoops / LOOPS_PER_SEC) % 60);
   if (sec == 0) return String(mins) + ":00";
   else if (sec < 10) return String(mins) + ":0" + String(sec);
   return String(mins) + ":" + String(sec);
}

Buffer::Buffer() {
   reset();
}

float Buffer::avg() {
   long sum = 0;
   
   for (int i = 0; i < _dataCount; i++) {
      sum += _data[i];
   }

   float result = 0.0;
   if (_dataCount) result = float(sum / _dataCount);
   return result;
}

float Buffer::stdev() {
   float devSum = 0;
   float delta = 0;
   const float Avg = avg();
   
   for (int i = 0; i < _dataCount; i++) {
      delta = float(_data[i] - Avg);
      devSum += delta * delta;
   }

   float result = 0;
   if (_dataCount) result = float(sqrt(devSum / _dataCount));
   return result;
}

void Buffer::reset() {
   _dataCount = 0;
   _counter = 0;
}

void Buffer::addData(long x) {
   int index = _counter % BUF_SIZE;
   _data[index] = x;
   if (_dataCount < BUF_SIZE) _dataCount++;
   _counter++;
}

int Buffer::count() {
   return _dataCount;
}

String Buffer::dataText() {
   float Avg = avg();
   float Stdev = stdev();
   String str = "";

   for (int i = 0; i < _dataCount; i++) {
      str += String(_data[i]) + " ";
   }
   str += " AVG: " + String(avg());
   str += " StDev: " + String(stdev());

   return str;
}

DataStore::DataStore() {
   reset();
}

float DataStore::avg() {
   long sum = 0;
   
   for (int i = 0; i < _dataCount; i++) {
      sum += _data[i];
   }

   float result = 0.0;
   if (_dataCount) result = float(sum / _dataCount);
   return result;
}

float DataStore::stdev() {
   float devSum = 0;
   float delta = 0;
   const float Avg = avg();
   
   for (int i = 0; i < _dataCount; i++) {
      delta = float(_data[i] - Avg);
      devSum += delta * delta;
   }

   float result = 0.0;
   if (_dataCount) result = float(sqrt(devSum / _dataCount));
   return result;
}

void DataStore::reset() {
   _dataCount = 0;
   for (int i = 0; i < DATASTORE_SIZE; i++) _data[i] = 0;
}

void DataStore::addData(int x) {
   _data[_dataCount] = x;
   if (_dataCount < 100) _dataCount++;
}

String DataStore::dataText() {
   float Avg = avg();
   float Stdev = stdev();
   String str = "";
   int Max = _dataCount;
   if (Max > 100) Max = 100;

   str += "n:" + String(_dataCount);
   str += " AVG:" + String(avg());
   str += " StDev:" + String(stdev()) + "  ";

   for (int i = 0; i < Max; i++) {
      str += String(_data[i]) + " ";
   }

   return str;
}
