#include "global.h"

extern void VLog(String);

// Create Wifi Server
WiFiServer server(80);  // Set web server port number to 80


void webServer() {
    WiFiClient client = server.available(); // Listen for incoming clients
    String str = "";
    
    if (client) 
    { // If a new client connects,
    
        String request = client.readStringUntil('\r'); 
        client.println("HTTP/1.1 200 OK");
        client.println("Content-type:text/html");
        client.println("Connection: close");
        client.println();
        
        // Display the HTML web page
        client.println("<!DOCTYPE html><html>");
        client.println("<head><meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">");
        client.println("<link rel=\"icon\" href=\"data:,\">");
        client.println("<meta http-equiv=\"refresh\" content=\"5\">");
        client.println("<script>\n");
        client.println("setInterval(loadDoc,5000);\n");
        client.println("function loadDoc() {\n");
        client.println("var xhttp = new XMLHttpRequest();\n");
        client.println("xhttp.onreadystatechange = function() {\n");
        client.println("if (this.readyState == 4 && this.status == 200) {\n");
        client.println("document.getElementById(\"webpage\").innerHTML =this.responseText}\n");
        client.println("};\n");
        client.println("xhttp.open(\"GET\", \"/\", true);\n");
        client.println("xhttp.send();\n");
        client.println("}\n");
        client.println("</script>\n"); 
        // CSS to style the table
        
        client.println("<style>body { text-align: center; font-family: \"Arial\", Arial;}");
        client.println("table { border-collapse: collapse; width:60%; margin-left:auto; margin-right:auto;border-spacing: 2px;background-color: white;border: 4px solid green; }");
        client.println("th { padding: 20px; background-color: #008000; color: white; }");
        client.println("tr { border: 5px solid green; padding: 2px; }");
        client.println("tr:hover { background-color:lightskyblue; }");
        client.println("td { border:4px; padding: 12px; }");
        client.println(".sensor { color:white; font-weight: bold; background-color: #bcbcbc; padding: 1px; }"); 
        
        // Web Page Heading
        client.println("</style></head><body><h1>Aneil's ESP32 Project</h1>");
        client.println("<h2>Furnace Monitor V1.0");
        client.println("<table><tr><th>MEASUREMENT</th><th>VALUE</th></tr>");
        
        client.println("<tr><td>Furnace Cycles</td><td><span class=\"sensor\">");
        client.println(String(FURNACE_EVENT_CNT));
        client.println("</span></td></tr>"); 
        
        client.println("<tr><td>Loop Count (100ms)</td><td><span class=\"sensor\">");
        client.println(String(LOOP_COUNT));
        client.println("</span></td></tr>"); 

        client.println("<tr><td>Furnace State</td><td><span class=\"sensor\">");
        client.println(FurnStateTxt() + " ["  + StateClock() + "]");
        client.println("</span></td></tr>"); 

        float monMin = float(LOOP_COUNT / 60.0 / LOOPS_PER_SEC);
        float monHr = float(LOOP_COUNT / 3600.0 / LOOPS_PER_SEC);
        float monDay = float(LOOP_COUNT / 3600.0 / 24.0 / LOOPS_PER_SEC);

        if (monHr < 1) {
          client.println("<tr><td>Total Monitor Time</td><td><span class=\"sensor\">");
          client.println(String(monMin) + String("m"));
          client.println("</span></td></tr>"); 
        } 
        else if (monDay < 1) {
          client.println("<tr><td>Total Monitor Time</td><td><span class=\"sensor\">");
          client.println(String(monHr) + String("hr"));
          client.println("</span></td></tr>"); 
        } 
        else {
          client.println("<tr><td>Total Monitor Time</td><td><span class=\"sensor\">");
          client.println(String(monDay) + String("d"));
          client.println("</span></td></tr>"); 
        }

        float onMin = float(FURNACE_ON_TIME_LOOPS / 60.0 / LOOPS_PER_SEC);
        float onHour = float(FURNACE_ON_TIME_LOOPS / 3600.0 / LOOPS_PER_SEC);
        float lastOnMin = float(LastFurnaceOnTimeLoops) / 60 / LOOPS_PER_SEC;
        float lastOffMin = float(LastFurnaceOffTimeLoops) / 60 / LOOPS_PER_SEC;
        
        if (onMin < 60) { 
          client.println("<tr><td>TOTAL FURNACE ON Time</td><td><span class=\"sensor\">");
          client.println(String(onMin) + String("m"));
          client.println("</span></td></tr>"); 
        } 
        else {
          client.println("<tr><td>TOTAL FURNACE ON Time</td><td><span class=\"sensor\">");
          client.println(String(onHour) + String("hr"));
          client.println("</span></td></tr>");
        }

        client.println("<tr><td>LAST FURNACE ON Time</td><td><span class=\"sensor\">");
        client.println(String(lastOnMin) + String("m"));
        client.println("</span></td></tr>"); 

        client.println("<tr><td>LAST FURNACE OFF Time</td><td><span class=\"sensor\">");
        client.println(String(lastOffMin) + String("m"));
        client.println("</span></td></tr>"); 
        
        str = String(DELTA_MIN) + String("m");
        if (FURNACE_EVENT_CNT < 2) str = "***";
        client.println("<tr><td>Last Cycle Time</td><td><span class=\"sensor\">");
        client.println(str);
        client.println("</span></td></tr>"); 

        str = String(AVG_DELTA_MIN) + String("m [") + String(STDEV_DELTA_MIN) + String("]");       
        if (FURNACE_EVENT_CNT < 2) str = "***";
        client.println("<tr><td>Avg Cycle Time [StDev]</td><td><span class=\"sensor\">");
        client.print(str);
        client.println("</span></td></tr>"); 

        float deltaMin = AVG_DELTA_MIN;
        if (deltaMin < 0.1) deltaMin = 5.0;   // default to 5 mins if there is no data
        float cph = 60.0 / deltaMin;
        int cpd = 60 * 25 / deltaMin;

        str = String(cph) + String(" [") + String(cpd) + String("]");
        if (FURNACE_EVENT_CNT < 2) str = "***"; 
         
        client.println("<tr><td>Cycles per HR [Day]</td><td><span class=\"sensor\">");
        client.print(str);
        client.println("</span></td></tr>"); 

        float offTime = (LOOP_COUNT - LAST_EVENT) / 60.0 / LOOPS_PER_SEC;

        str = String(offTime) + String("m");
        if (FURNACE_EVENT_CNT < 1) str = "***";
        client.println("<tr><td>Time Since Last Cycle</td><td><span class=\"sensor\">");
        client.println(str);
        client.println("</span></td></tr>"); 

        str = String(ON_PCT) + String("%");
        if (FURNACE_EVENT_CNT < 2) str = "***";
        client.println("<tr><td>ON Time Percent</td><td><span class=\"sensor\">");
        client.println(str);
        client.println("</span></td></tr>"); 
        
        client.println("<tr><td>Timestamp</td><td><span class=\"sensor\">");
        client.println(String(getTimestamp()));
        client.println("</span></td></tr>"); 
        
        client.println("</body></html>");
        
        client.println();
        delay(10);
        client.stop();
        // VLog("Client disconnected.");
        // VLog("");
    } 
}
