#include <ESP8266WiFi.h>
#include <ESPAsyncWebServer.h>
#include <TimeLib.h>
#include <Timezone.h>

TimeChangeRule usPDT = {"PDT", Second, Sun, Mar, 2, -420};
TimeChangeRule usPST = {"PST", First, Sun, Nov, 2, -480};
Timezone usPT(usPDT, usPST);

/* Put your SSID & Password */
const char* ssid = "WindowBlinds";  // Enter SSID here
const char* password = "susamogus";  //Enter Password here

/* Put IP Address details */
IPAddress local_ip(192,168,1,1);
IPAddress gateway(192,168,1,1);
IPAddress subnet(255,255,255,0);


String getLocalTimeString();
time_t compileTime();
time_t getLocalTime();

String blind_open_string = "00:00";
int blind_open_hour = 0;
int blind_open_minute = 0;

// ============================================== Web Stuf ==============================================

const char* param_blind_open = "blind_open";
const char* param_unix_time = "unix_time";

const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
  <meta name="viewport" content="width=device-width, initial-scale=1.0, user-scalable=no">
  <title>Blind Control</title>
  <style>
    b { font-size: larger; }
    p { font-size: large; }
    input { margin-left: 10px;}
    @media (prefers-color-scheme: dark) {
    
    }
  </style>
  <script>
    function submitTime() {
      window.location = "set_unix_time?unix_time=" + Math.round((new Date()).getTime() / 1000);
    }
  </script>
  <body style="background-color: rgb(37, 37, 37); color: white;">
    <div style="text-align:center;">
      %TIMEPLACEHOLDER%
  
    </div>
    <br>
    <hr>
    <br>
    <div style="text-align: center;">
      <form action="/set_blind_time">
        <label for="blind_open">Open Blind Time:</label>
        %BLINDOPENPLACEHOLDER%
        <input type="submit"></input>
      </form>
    </div>
    <br>
    <div style="text-align: center;">
      <form onsubmit="submitTime();return false">
        <label for="unix_time">Sync Time:</label>
        <input type="Sync"></input>
      </form>
    </div>
  </body>
</html>
)rawliteral";

String processor(const String& var){
  //Serial.println(var);
  if(var == "TIMEPLACEHOLDER"){
    String time_text = "";
    time_text += "<p><b>Unix Time: </b>" + String(now()) + "</p>";
    time_text += "<p><b>Local Time: </b>" + getLocalTimeString() + "</p>";

    return time_text;
  }
  if (var == "BLINDOPENPLACEHOLDER"){
    String blind_open_text = "<input type=\"time\" name=\"blind_open\" value=\"" + blind_open_string + "\">" ;
    return blind_open_text;
  }
  return String();
}

AsyncWebServer server(80);

void setup() {
  Serial.begin(115200);

  WiFi.mode(WIFI_AP);
  boolean result = WiFi.softAP(ssid, password);
  Serial.println(result ? "Ready" : "Failed!");
  WiFi.softAPConfig(local_ip, gateway, subnet);
  
  delay(100);

  // ============ time shit =============
  setTime(compileTime() + 60 * 60 * 7); // this is only for PDT(since compile time is in PDT as of rn)

  // ============ server shit ============
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send_P(200, "text/html", index_html, processor);
  });
  
  server.begin();
  Serial.println("HTTP server started");
  server.on("/set_blind_time", HTTP_GET, [] (AsyncWebServerRequest *request) {
    if (request->hasParam(param_blind_open)) {
      blind_open_string = request->getParam(param_blind_open)->value();
      String open_hour = blind_open_string.substring(0, 2);
      String open_minute = blind_open_string.substring(3, 5);

      blind_open_hour = open_hour.toInt();
      blind_open_minute = open_minute.toInt();
    }
    Serial.println("open hour: " + String(blind_open_hour) + "  open minute: " + String(blind_open_minute));
    request->redirect("/");
  });
  server.on("/set_unix_time", HTTP_GET, [] (AsyncWebServerRequest *request) {
    if (request->hasParam(param_unix_time)) {
      String blind_open_string = request->getParam(param_unix_time)->value();
      Serial.println(blind_open_string);
      setTimeUnix(blind_open_string);
    }
    request->redirect("/");

  });
}

void loop() {
 
}

// =================================== time shit =======================================

void setTimeUnix(String unixTimeString) {
  time_t pctime = 0;
  for (int i = 0; i < unixTimeString.length(); i++) {
    char c = unixTimeString[i];
    if ( c >= '0' && c <= '9') {
     pctime = (10 * pctime) + (c - '0') ; // convert digits to a number
    }
  }
  setTime(pctime);
}

time_t compileTime()
{
    const time_t FUDGE(10);     // fudge factor to allow for compile time (seconds, YMMV)
    const char *compDate = __DATE__, *compTime = __TIME__, *months = "JanFebMarAprMayJunJulAugSepOctNovDec";
    char chMon[4], *m;
    tmElements_t tm;

    strncpy(chMon, compDate, 3);
    chMon[3] = '\0';
    m = strstr(months, chMon);
    tm.Month = ((m - months) / 3 + 1);

    tm.Day = atoi(compDate + 4);
    tm.Year = atoi(compDate + 7) - 1970;
    tm.Hour = atoi(compTime);
    tm.Minute = atoi(compTime + 3);
    tm.Second = atoi(compTime + 6);
    time_t t = makeTime(tm);
    return t + FUDGE;           // add fudge factor to allow for compile time
}

time_t getLocalTime() {
  time_t temp = now();
  return usPT.toLocal(temp);
}

String getLocalTimeString() {
 // digital clock display of the time
 time_t current_time = getLocalTime();
 TimeChangeRule *tcr;
 time_t temp = now();
 usPT.toLocal(temp, &tcr);
 
 String start_string = String(hour(current_time));
 start_string = printDigits(minute(current_time), start_string);
 start_string = printDigits(second(current_time), start_string);
 start_string += " " + String(month(current_time)) + "/" + String(day(current_time)) + "/" + String(year(current_time));
 return start_string + " (" + tcr -> abbrev + ")";
}

// technically a monad(fancy vocab hehe)
String printDigits(int digits, String start_string) {
 // utility function for digital clock display: prints preceding colon and leading 0
 start_string += ":";
 if (digits < 10)
 start_string += '0';
 start_string += digits;
 return start_string;
}
