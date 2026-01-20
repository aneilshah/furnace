#pragma once

// external furnace variables
extern float AVG_DELTA_MIN;
extern float STDEV_DELTA_MIN;
extern long DELTA_SEC;
extern float DELTA_MIN;
extern int FURNACE_EVENT_CNT;
extern long FURNACE_ON_TIME_LOOPS;
extern long FURNACE_OFF_TIME_LOOPS;
extern int holdCount;
extern long LastFurnaceOnTimeLoops;
extern long LastFurnaceOffTimeLoops;
extern int ON_PCT;
extern int OFF_PCT;


// Furnace Defines
#define HOLD_TIME 100;        // 10 sec (100ms per count)

// Furnace States
#define FURN_OFF 0
#define FURN_PRE_HEAT 1
#define FURN_ON 2
#define FURN_COOL_DOWN 3

// Furnace Functions
void processFurnaceEvent();
void initFurnace();
void processFBWriteEvent();
String FurnStateTxt();
String StateClock();
int getFurnaceState();
int getFurnaceStateDeltaSec();

// Buffer Class

#define BUF_SIZE 10
#define DATASTORE_SIZE 150

class Buffer {
  public:
    Buffer();
    float avg();
    float stdev();
    void reset();
    void addData(long x);
    String dataText();
    int count();
    
  private:
    long _data[BUF_SIZE] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
    int _dataCount;
    long _counter;
};

class DataStore {
  public:
    DataStore();
    float avg();
    float stdev();
    void reset();
    void addData(int x);
    String dataText();
    
  private:
    int _data[DATASTORE_SIZE+1];
    int _dataCount;
};
