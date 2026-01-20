// Libraries
#include <event.h>

// Local Files
#include "global.h"
#include "furnace.h"

int testModeADC = 0;
Event furnaceTestEvent;

int getTestModeADC() {
  return testModeADC;
}

void setTestModeADC(bool furnOn) {
  if (furnOn) {
    testModeADC = FURN_ON_COUNTS + 10;
  }
  else testModeADC = 0; 
}

void initFurnaceSim() {
  furnaceTestEvent.setSec(30);
}

void simulateFurnace() {
  int furnaceState = getFurnaceState();

  if (furnaceState == FURN_OFF) {
    // Look for Turn On Event
    if (furnaceTestEvent.check()) {
      setTestModeADC(true);
      int nextEvent = random(PREHEAT_TIME+25, PREHEAT_TIME+35);
      furnaceTestEvent.setSec(nextEvent);  // Set Cool Down Event
      Serial.println("Set Next Test Mode Furnace Cool Down Event: " + String(nextEvent) + " sec");
    }
  }
  
  else if (furnaceState == FURN_PRE_HEAT) {
    // Do nothing
  }

  else if (furnaceState == FURN_ON) {
    // Look for Turn Off Event
    if (furnaceTestEvent.check()) {
      setTestModeADC(false);
      int nextEvent = random(COOLDOWN_TIME+25,COOLDOWN_TIME+35);
      furnaceTestEvent.setSec(nextEvent);
      Serial.println("Set Next Test Mode Furnace PreHeat Event: " + String(nextEvent) + " sec");
    }
  }
  
  else if (furnaceState == FURN_COOL_DOWN) {
    // Do Nothing
  }

  else {
    Serial.println("Invalid Furnace State");
  }

}