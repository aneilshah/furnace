#include "FurnaceFBRepo.h"

// Project includes
#include "global.h"
#include "ntp.h"
#include "oledFunc.h"
#include "furnace.h"

// -----------------------------------------------------------------------------
// Paths / limits
// -----------------------------------------------------------------------------
#define FB_PATH "aneil"
#define FB_TEST_PATH "aneil/test_server"
#define MAX_DAILY_FB_WRITES 16

// -----------------------------------------------------------------------------
// Extern data from furnace project (from your old furnace firebase.ino)
// -----------------------------------------------------------------------------
extern Buffer CycleData;
extern Buffer OnData;
extern Buffer OffData;
extern DataStore Counts;

extern long LastFurnaceOnTimeLoops;
extern long LastFurnaceOffTimeLoops;

FurnaceFBRepo::FurnaceFBRepo(FirebaseClient& client) : fb(client) {}

int FurnaceFBRepo::firebaseOK() {
  return fb.ready();
}

void FurnaceFBRepo::clearDailyFBWriteCount() {
  writesToday = 0;
}

void FurnaceFBRepo::writeLog(const String& url, bool writeDailyData) {
  if (!firebaseOK()) {
    Serial.println("NOT UPDATING FIREBASE - NO CONNECTION");
    return;
  }
  if (writesToday >= MAX_DAILY_FB_WRITES) {
    Serial.println("ERROR: FB NOT WRITTEN - MAX WRITES EXCEEDED");
    return;
  }

  float avgMin = AVG_DELTA_MIN;
  if (avgMin < 0.1f) avgMin = 5.0f;

  const int cph = (int)(60.0f / avgMin);

  // Preserving your original behavior (25 hours). Change to 24 if desired.
  const int cpd = (int)((60.0f * 25.0f) / avgMin);

  FirebaseJson json;

  // Always written
  json.set("timestamp", getTimestamp());
  json.set("furnace_cycle_count", (int)FURNACE_EVENT_CNT);
  json.set("loop_count", (int)LOOP_COUNT);

  json.set("monitor_time_min", (float)(LOOP_COUNT / 60.0 / LOOPS_PER_SEC));
  json.set("monitor_time_day", (float)(LOOP_COUNT / 3600.0 / 24.0 / LOOPS_PER_SEC));

  json.set("last_furnace_on_time_min", (float)(LastFurnaceOnTimeLoops / 60.0 / LOOPS_PER_SEC));
  json.set("last_furnace_off_time_min", (float)(LastFurnaceOffTimeLoops / 60.0 / LOOPS_PER_SEC));

  json.set("cycle_time_min", (float)DELTA_MIN);
  json.set("avg_cycle_time_min", (float)AVG_DELTA_MIN);
  json.set("stdev_cycle_time_min", (float)STDEV_DELTA_MIN);

  json.set("cycles_per_hour", cph);
  json.set("cycles_per_day", cpd);

  json.set("on_percent", (int)ON_PCT);

  // Optional: reduce payload for daily write logs
  if (!writeDailyData) {
    json.set("cycle_data", CycleData.dataText());
    json.set("sensor_data", Counts.dataText());
    json.set("on_time_data", OnData.dataText());
    json.set("off_time_data", OffData.dataText());
  }

  json.set("loop_time", (int)LOOP_TIME);
  json.set("wifi_err", (int)WIFI_ERR);
  json.set("firebase_err", (int)FB_ERR);

  if (!fb.writeJSON(url, json)) {
    Serial.println("FB writeJSON failed: " + fb.lastError());
    return;
  }

  writesToday++;
}

void FurnaceFBRepo::writeFurnaceData(bool writeDailyData) {
  if (!firebaseOK()) {
    Serial.println("Firebase Not Connected");
    oledMain(MAIN_TIMEOUT_SEC);
    return;
  }

  displayPopupScreen("FIREBASE...", "Writing Data to Firebase");
  oledMain(MAIN_TIMEOUT_SEC);

  Serial.println();
  Serial.println("*** Writing Furnace Data to Firebase");
  const unsigned long sendDataPrevMillis = millis();

  String PATH = String(FB_PATH);
  const String timestamp = getTimelog();

  String year = "0";
  if (validClock()) {
    year = getYearStr();
    Serial.println("Firebase Year: " + year);
  }

  // match pump behavior
  const int yearInt = year.toInt();
  if (yearInt < 2026) {
    Serial.println("Skipping Firebase write: Year < 2026 (" + year + ")");
    oledMain(MAIN_TIMEOUT_SEC);
    return;
  }

  if (TEST_MODE) PATH = String(FB_TEST_PATH) + "/" + year;
  else PATH = PATH + "/" + year;

  const String URL1 = "/furnace/" + PATH + "/last_event/";
  const String URL2 = "/furnace/" + PATH + "/detail_logs/" + timestamp + "/";
  const String URL3 = "/furnace/" + PATH + "/daily_logs/" + timestamp + "/";

  writeLog(URL1, false);
  if (writeDailyData) writeLog(URL3, true);
  else                writeLog(URL2, false);

  Serial.printf("Time to Write: %lu ms\n",
                (unsigned long)(millis() - sendDataPrevMillis));

  oledMain(MAIN_TIMEOUT_SEC);
}

void FurnaceFBRepo::writeEventData(const String& eventText) {
  if (!firebaseOK()) {
    Serial.println("Firebase Not Connected (writeEvent)");
    return;
  }

  // Year gating consistent with writeFurnaceData()
  String year = "0";
  if (validClock()) {
    year = getYearStr();
  }

  const int yearInt = year.toInt();
  if (yearInt < 2026) {
    Serial.println("Skipping Firebase event write: Year < 2026 (" + year + ")");
    return;
  }

  String PATH = String(FB_PATH);
  if (TEST_MODE) PATH = String(FB_TEST_PATH) + "/" + year;
  else PATH = PATH + "/" + year;

  // Use the same timestamp key strategy as your logs
  const String timestampKey = getTimelog();

  const String URL = "/furnace/" + PATH + "/events/" + timestampKey + "/";

  FirebaseJson json;

  // Nested data object
  json.set("day", getDayStr());
  json.set("month", getMonthStr());
  json.set("year", getYearStr());
  json.set("hour", getHourStr());
  json.set("min", getMinStr());
  json.set("sec", getSecStr());
  json.set("event", eventText);
  json.set("timestamp", timestampKey);

  if (!fb.writeJSON(URL, json)) {
    Serial.println("FB writeEvent failed: " + fb.lastError());
    return;
  }
}
