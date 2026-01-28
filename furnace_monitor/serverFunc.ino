#include "global.h"

extern void VLog(String);

// Create Wifi Server
WiFiServer server(80);  // Set web server port number to 80

// Put constant HTML fragments in flash (saves RAM)
static const char HTML_HEAD[] PROGMEM =
"<!DOCTYPE html><html>"
"<head><meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">"
"<link rel=\"icon\" href=\"data:,\">"
"<script>"
"setInterval(loadDoc,30000);"
"loadDoc();"
"function loadDoc(){"
"var xhttp=new XMLHttpRequest();"
"xhttp.onreadystatechange=function(){"
"if(this.readyState==4&&this.status==200){"
"document.getElementById(\"webpage\").innerHTML=this.responseText}"
"};"
"xhttp.open(\"GET\",\"/data\",true);"
"xhttp.send();"
"}"
"</script>"
"<style>"
":root{"
"--theme-color:#2F6F73;"
"--theme-hover:#D9EEF0;"
"--theme-font-size:18px;"            /* fallback */
"--theme-row-alt:#f5f7f7;"
"--theme-measurement-color:#2a2a2a;"
"}"
"body{"
"text-align:center;"
"font-family:\"Arial\",Arial;"
"font-size:var(--theme-font-size);"
"}"
"table{"
"border-collapse:collapse;"
"width:80%;"                         /* default (landscape fallback) */
"margin-left:auto;margin-right:auto;"
"border-spacing:2px;"
"background-color:white;"
"border:4px solid var(--theme-color);"
"}"
"th{"
"padding:20px;"
"background-color:var(--theme-color);"
"color:white;"
"font-size:1.1em;"
"}"
"tr{"
"border:5px solid var(--theme-color);"
"}"
"tr:nth-child(even){background-color:var(--theme-row-alt);}"
"tr:hover{background-color:var(--theme-hover);}"
"td{border:4px;padding:12px;}"
"td:first-child{font-weight:700;color:var(--theme-measurement-color);}"
".sensor{"
"display:inline-block;"
"padding:6px 14px;"
"border-radius:999px;"
"background-color:var(--theme-color);"
"color:white;"
"font-weight:700;"
"letter-spacing:0.3px;"
"min-width:140px;"
"text-align:center;"
"box-shadow:0 1px 2px rgba(0,0,0,0.15);"
"}"
".small{font-size:0.85em;}"
".large{font-size:1.25em;}"
".huge{font-size:1.5em;font-weight:bold;}"

/* -------- Orientation-based scaling -------- */
"@media (orientation: portrait){"
"table{width:95%;}"
":root{--theme-font-size:16px;}"
"td{padding:8px;}"
".sensor{padding:4px 10px;min-width:110px;}"
"}"
"@media (orientation: landscape){"
"table{width:80%;}"
":root{--theme-font-size:20px;}"
"td{padding:12px;}"
".sensor{padding:6px 14px;min-width:140px;}"
"}"

"</style>"
"</head><body>";

static const char HTML_TOP[] PROGMEM =
"<h2>Shahman Furnace Monitor V0.02</h2>"
"<div id=\"webpage\">";   // AJAX replaces this div with /data response

static const char HTML_TABLE_OPEN[] PROGMEM =
"<table><tr><th>MEASUREMENT</th><th>VALUE</th></tr>";

static const char HTML_TABLE_CLOSE[] PROGMEM =
"</table>";

static const char HTML_BOTTOM[] PROGMEM =
"</div></body></html>";

static inline void httpOk(WiFiClient &client) {
  client.println("HTTP/1.1 200 OK");
  client.println("Content-type:text/html");
  client.println("Connection: close");
  client.println("Cache-Control: no-store"); // avoid caching, helpful for /data
  client.println();
}

static inline void httpNotFound(WiFiClient &client) {
  client.println("HTTP/1.1 404 Not Found");
  client.println("Connection: close");
  client.println();
}

static inline void printRowOpen(WiFiClient &client, const __FlashStringHelper *label) {
  client.print(F("<tr><td>"));
  client.print(label);
  client.print(F("</td><td><span class=\"sensor\">"));
}

static inline void printRowClose(WiFiClient &client) {
  client.println(F("</span></td></tr>"));
}

static inline void printRow(WiFiClient &client, const __FlashStringHelper *label, const String &value) {
  printRowOpen(client, label);
  client.print(value);
  printRowClose(client);
}

static inline void printRowCstr(WiFiClient &client, const __FlashStringHelper *label, const char *value) {
  printRowOpen(client, label);
  client.print(value);
  printRowClose(client);
}

// Reads first request line without allocating big Strings; returns true if got a line.
static bool readRequestLine(WiFiClient &client, char *buf, size_t buflen, uint32_t timeoutMs = 1500) {
  uint32_t start = millis();
  size_t idx = 0;

  while (millis() - start < timeoutMs) {
    while (client.available()) {
      char c = (char)client.read();
      if (c == '\n') {        // end of line
        buf[idx] = '\0';
        return true;
      }
      if (c != '\r') {
        if (idx + 1 < buflen) buf[idx++] = c;
      }
    }
    delay(1);
  }
  buf[idx] = '\0';
  return idx > 0;
}

static void renderTable(WiFiClient &client) {
  client.print((const __FlashStringHelper*)HTML_TABLE_OPEN);

  // Values
  printRow(client, F("Furnace Cycles"), String(FURNACE_EVENT_CNT));
  printRow(client, F("Loop Count (100ms)"), String(LOOP_COUNT));

  // Furnace State: build once
  {
    String s;
    s.reserve(64);
    s = FurnStateTxt();
    s += " [";
    s += StateClock();
    s += "]";
    printRow(client, F("Furnace State"), s);
  }

  const float loopsPerSec = (float)LOOPS_PER_SEC;
  const float monMin = (float)LOOP_COUNT / (60.0f * loopsPerSec);
  const float monHr  = (float)LOOP_COUNT / (3600.0f * loopsPerSec);
  const float monDay = (float)LOOP_COUNT / (86400.0f * loopsPerSec);

  if (monHr < 1.0f)      printRow(client, F("Total Monitor Time"), String(monMin) + "m");
  else if (monDay < 1.0f)printRow(client, F("Total Monitor Time"), String(monHr) + "hr");
  else                   printRow(client, F("Total Monitor Time"), String(monDay) + "d");

  const float onMin   = (float)FURNACE_ON_TIME_LOOPS / (60.0f * loopsPerSec);
  const float onHour  = (float)FURNACE_ON_TIME_LOOPS / (3600.0f * loopsPerSec);
  const float lastOnMin  = (float)LastFurnaceOnTimeLoops  / (60.0f * loopsPerSec);
  const float lastOffMin = (float)LastFurnaceOffTimeLoops / (60.0f * loopsPerSec);

  if (onMin < 60.0f) printRow(client, F("TOTAL FURNACE ON Time"), String(onMin) + "m");
  else               printRow(client, F("TOTAL FURNACE ON Time"), String(onHour) + "hr");

  printRow(client, F("LAST FURNACE ON Time"),  String(lastOnMin) + "m");
  printRow(client, F("LAST FURNACE OFF Time"), String(lastOffMin) + "m");

  if (FURNACE_EVENT_CNT < 2) printRowCstr(client, F("Last Cycle Time"), "***");
  else                       printRow(client, F("Last Cycle Time"), String(DELTA_MIN) + "m");

  if (FURNACE_EVENT_CNT < 2) {
    printRowCstr(client, F("Avg Cycle Time [StDev]"), "***");
  } else {
    String s;
    s.reserve(32);
    s = String(AVG_DELTA_MIN);
    s += "m [";
    s += String(STDEV_DELTA_MIN);
    s += "]";
    printRow(client, F("Avg Cycle Time [StDev]"), s);
  }

  if (FURNACE_EVENT_CNT < 2) {
    printRowCstr(client, F("Cycles per HR [Day]"), "***");
  } else {
    float deltaMin = AVG_DELTA_MIN;
    if (deltaMin < 0.1f) deltaMin = 5.0f;
    const float cph = 60.0f / deltaMin;
    const int   cpd = (int)(60.0f * 25.0f / deltaMin);

    String s;
    s.reserve(24);
    s = String(cph);
    s += " [";
    s += String(cpd);
    s += "]";
    printRow(client, F("Cycles per HR [Day]"), s);
  }

  if (FURNACE_EVENT_CNT < 1) {
    printRowCstr(client, F("Time Since Last Cycle"), "***");
  } else {
    const float offTimeMin = (float)(LOOP_COUNT - LAST_EVENT) / (60.0f * loopsPerSec);
    printRow(client, F("Time Since Last Cycle"), String(offTimeMin) + "m");
  }

  if (FURNACE_EVENT_CNT < 2) printRowCstr(client, F("ON Time Percent"), "***");
  else                       printRow(client, F("ON Time Percent"), String(ON_PCT) + "%");

  printRow(client, F("Timestamp"), String(getTimestamp()));

  client.print((const __FlashStringHelper*)HTML_TABLE_CLOSE);
}

static void renderPage(WiFiClient &client) {
  client.print((const __FlashStringHelper*)HTML_HEAD);
  client.print((const __FlashStringHelper*)HTML_TOP);

  renderTable(client);

  client.print((const __FlashStringHelper*)HTML_BOTTOM);
}

void webServer() {
  WiFiClient client = server.available();
  if (!client) return;

  // Read the request line (e.g. "GET / HTTP/1.1")
  char line[128];
  if (!readRequestLine(client, line, sizeof(line))) {
    client.stop();
    return;
  }

  // Route parsing: handle GET / and GET /data
  const bool isGet = (strncmp(line, "GET ", 4) == 0);
  const char *path = isGet ? (line + 4) : nullptr;

  bool isRoot = false;
  bool isData = false;

  if (path && path[0] == '/') {
    const char *sp = strchr(path, ' ');
    size_t len = sp ? (size_t)(sp - path) : strlen(path);

    isRoot = (len == 1); // "/"
    isData = (len == 5 && strncmp(path, "/data", 5) == 0);
  }

  if (!isGet || (!isRoot && !isData)) {
    httpNotFound(client);
    client.stop();
    return;
  }

  httpOk(client);

  if (isData) {
    renderTable(client);   // only dynamic content
  } else {
    renderPage(client);    // full page once
  }

  client.flush();
  client.stop();
}
