#pragma once
void scanWifi();
void connectFurnaceWifi();
bool ensureServerStarted();
bool waitForWifiStable(uint32_t stableMs = 1500, uint32_t timeoutMs = 20000);
bool wifiOK();
bool dnsOK();
bool internetOK();
bool internetOK443();
void resetFirebase();

#define EDGE_HOST_ID 185
#define TEST_SERVER_HOST_ID 191
// #define SUBNET_ID 50
#define USE_STATIC_IP 0
