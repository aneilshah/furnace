#pragma once
#include <Arduino.h>
#include "FirebaseClient.h"

class FurnaceFBRepo {
public:
  FurnaceFBRepo(FirebaseClient& c);

  void writeFurnaceData(bool writeDailyData);
  void clearDailyFBWriteCount();
  int firebaseOK();

private:
  void writeLog(const String& url, bool writeDailyData);

  FirebaseClient& fb;
  int writesToday = 0;
};
