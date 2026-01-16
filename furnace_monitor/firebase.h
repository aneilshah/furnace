#pragma once
#include <Arduino.h>

void setupFirebase(bool showDisplay = true);
void writeFurnaceData();
void writeEventData(String eventText);
int firebaseOK();
