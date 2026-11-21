
/*
  DESK-COMPANION — XIAO ESP32-C3
  Robonavigators

  OLED: SSD1306 128x64, hardware I2C
  TTP223: D1 / GPIO3

  Features:
  - 66 animated emotions
  - OLED clock, weather and forecast
  - Touch-controlled screens and deep sleep
  - Readable HTML/CSS/JavaScript dashboard
  - Wi-Fi scanning and two saved networks
  - First-boot captive portal
  - Recovery hotspot after 30 seconds offline
  - Automatic hotspot shutdown on reconnection
  - OTA password: admin123
  - mDNS: desk-companion.local

  Libraries: U8g2, ArduinoJson 7, NTPClient
  ESP32 Arduino Core 3.x
*/

#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <DNSServer.h>
#include <Preferences.h>
#include <ESPmDNS.h>
#include <ArduinoOTA.h>
#include <HTTPClient.h>
#include <WiFiUdp.h>
#include <NTPClient.h>
#include <ArduinoJson.h>
#include <U8g2lib.h>
#include <Wire.h>
#include <esp_mac.h>
#include <esp_sleep.h>
#include <time.h>

#define TOUCH_PIN 3
#define FRAME_MS 33UL
#define RETRY_MS 15000UL
#define RECOVERY_MS 30000UL
#define AP_CLOSE_MS 1800UL
#define WEATHER_MS 900000UL

WebServer server(80);
DNSServer dns;
Preferences prefs;
WiFiUDP ntpUDP;
NTPClient clockClient(ntpUDP, "time.google.com", 0, 60000);

U8G2_SSD1306_128X64_NONAME_F_HW_I2C oled(
  U8G2_R0, U8X8_PIN_NONE
);

String ssid1, pass1, ssid2, pass2;
String apName, city, stateName, country, apiKey;
float lat = 0, lon = 0;
long utcOffset = 0;

bool setupRequired = false;
bool apRunning = false;
bool otaReady = false;
bool mdnsReady = false;
bool weatherReady = false;
bool useBackupNext = false;

uint32_t disconnectedAt = 0;
uint32_t connectedAt = 0;
uint32_t lastAttempt = 0;
uint32_t lastFrame = 0;
uint32_t lastWeather = 0;

uint8_t screen = 0;
bool touching = false;
bool longPressDone = false;
uint32_t touchStart = 0;

bool sleepPending = false;
uint32_t sleepAt = 0;

const char *EMOTIONS[] = {
  "Neutral","Happy","Sad","Angry","Surprised","Sleepy",
  "Love","Wink","Confused","Scared","Bored","Excited",
  "Crying","Laughing","Suspicious","Dizzy","Sick","Cool",
  "Shocked","Annoyed","Curious","Thinking","Embarrassed",
  "Proud","Worried","Blink","Jealous","Grumpy",
  "Mischievous","Shy","Nervous","Ecstatic","Frustrated",
  "Hopeful","Grateful","Nostalgic","Determined","Playful",
  "Sarcastic","Smug","Guilty","Relieved","Anxious",
  "Content","Irritated","Amused","Skeptical","Horrified",
  "Peaceful","Starstruck","Drowsy","Puzzled","Devious",
  "Humble","Optimistic","Pessimistic","Focused",
  "Daydreaming","Miserable","Cheeky","Alert","Calm",
  "Furious","Tearful","Hysterical","Smitten"
};
constexpr int EMOTION_COUNT = 66;

int emotion = 0;
bool manualEmotion = false;
uint32_t lastEmotion = 0;
uint32_t emotionDuration = 5000;
int pupilX = 0, pupilY = 0;
int targetX = 0, targetY = 0;
uint32_t lastEye = 0;
uint32_t lastEyeTarget = 0;

String condition = "Loading...";
float temperature = 0, shownTemperature = 0;
float windSpeed = 0;
int humidity = 0;
int weatherId = 800;
float forecastTemps[3] = {0,0,0};
int forecastIds[3] = {800,800,800};

const char *DAYS[] = {
  "Sun","Mon","Tue","Wed","Thu","Fri","Sat"
};
const char *MONTHS[] = {
  "Jan","Feb","Mar","Apr","May","Jun",
  "Jul","Aug","Sep","Oct","Nov","Dec"
};

// ---------- OLED ----------

void centered(const String &s, int y) {
  oled.drawStr(
    max(0, (128 - (int)oled.getStrWidth(s.c_str())) / 2),
    y, s.c_str()
  );
}

void drawStartup() {
  oled.clearBuffer();
  oled.setFont(u8g2_font_logisoso16_tf);
  centered("DESK", 27);
  centered("COMPANION", 54);
  oled.sendBuffer();
}

void drawPortal(bool recovery) {
  oled.clearBuffer();
  oled.setFont(u8g2_font_6x10_tf);
  oled.drawStr(0,10,recovery ? "WI-FI RECOVERY" : "SETUP REQUIRED");
  oled.drawHLine(0,13,128);
  oled.drawStr(0,24,"Connect to:");
  oled.drawStr(0,36,"Desk-Companion-");
  oled.drawStr(92,36,apName.substring(apName.length()-4).c_str());
  oled.drawStr(0,49,"Open 192.168.4.1");
  oled.drawStr(0,61,recovery ? "Retrying Wi-Fi..." : "Waiting for Wi-Fi");
  oled.sendBuffer();
}

void eye(int x, int y, int style, bool left) {
  switch (style) {
    case 0:
      oled.drawDisc(x,y,17);
      oled.setDrawColor(0);
      oled.drawDisc(x+pupilX,y+pupilY,6);
      oled.setDrawColor(1);
      break;
    case 1:
      oled.drawCircle(x,y,17);
      oled.drawDisc(x,y,5);
      break;
    case 2:
      oled.drawBox(x-16,y-2,32,4);
      break;
    case 3:
      oled.drawFilledEllipse(x,y,17,10);
      break;
    case 4:
      oled.drawBox(x-16,y-2,32,4);
      oled.drawCircle(x,y+4,12);
      break;
    case 5:
      oled.drawLine(x-9,y-9,x+9,y+9);
      oled.drawLine(x-9,y+9,x+9,y-9);
      break;
    case 6:
      oled.drawBox(x-18,y-4,36,8);
      break;
    default:
      oled.drawCircle(x,y,14);
      oled.drawDisc(x,y,3);
      break;
  }
  if (emotion == 7 && left) {
    oled.drawBox(x-17,y-3,34,6);
  }
}

void mouth(int width, int amplitude) {
  if (width < 2) return;
  int px = 64 - width/2;
  int py = 50;
  for (int i = -width/2; i <= width/2; i += 2) {
    float t = float(i) / float(width/2);
    int yy = 50 + int(amplitude*t*t);
    oled.drawLine(px,py,64+i,yy);
    px = 64+i;
    py = yy;
  }
}

void faceExtra(int e) {
  if (e == 6 || e == 65) {
    for (int x : {35,93}) {
      oled.drawDisc(x-5,23,5);
      oled.drawDisc(x+5,23,5);
      oled.drawTriangle(x-10,25,x+10,25,x,38);
    }
  }
  if (e == 12 || e == 58 || e == 63) {
    oled.drawFilledEllipse(28,53,3,8);
    oled.drawFilledEllipse(100,53,3,8);
  }
  if (e == 8 || e == 21 || e == 51) {
    oled.setFont(u8g2_font_helvB10_tf);
    oled.drawStr(60,13,"?");
  }
  if (e == 5 || e == 50 || e == 57) {
    oled.setFont(u8g2_font_helvB10_tf);
    oled.drawStr(105,13,"Z");
  }
  if (e == 22 || e == 29) {
    oled.drawFilledEllipse(25,47,6,3);
    oled.drawFilledEllipse(103,47,6,3);
  }
  if (e == 11 || e == 31 || e == 49 || e == 64) {
    oled.drawLine(8,7,8,20);
    oled.drawLine(2,13,14,13);
    oled.drawLine(120,7,120,20);
    oled.drawLine(114,13,126,13);
  }
}

void drawEmotion() {
  if (!manualEmotion && millis()-lastEmotion >= emotionDuration) {
    int next;
    do {
      next = random(0, EMOTION_COUNT);
    } while (next == emotion);
    emotion = next;
    lastEmotion = millis();
    emotionDuration = random(3000,8000);
  }

  if (emotion == 0) {
    if (millis()-lastEyeTarget > 2000) {
      targetX = random(-7,8);
      targetY = random(-6,7);
      lastEyeTarget = millis();
    }
    if (millis()-lastEye >= 35) {
      if (pupilX<targetX) pupilX++;
      if (pupilX>targetX) pupilX--;
      if (pupilY<targetY) pupilY++;
      if (pupilY>targetY) pupilY--;
      lastEye = millis();
    }
  } else {
    pupilX = pupilY = 0;
  }

  int style = (emotion * 7 + emotion/3) % 8;
  int amplitude = ((emotion * 11) % 27) - 10;
  int width = 22 + ((emotion * 13) % 33);

  if (emotion == 0) { style=0; width=0; }
  if (emotion == 1) { style=3; amplitude=14; }
  if (emotion == 2) { style=1; amplitude=-12; }
  if (emotion == 3) { style=0; amplitude=-8; }
  if (emotion == 5) { style=2; amplitude=3; }
  if (emotion == 7) { style=4; amplitude=12; }
  if (emotion == 13) { style=2; amplitude=20; }
  if (emotion == 15 || emotion == 16) style=5;
  if (emotion == 25) style=2;
  if (emotion == 48) style=2;

  eye(35,31,style,true);
  eye(93,31,style,false);

  if (emotion == 3 || emotion == 19 ||
      emotion == 32 || emotion == 62) {
    oled.drawLine(22,13,46,21);
    oled.drawLine(82,21,106,13);
  }

  mouth(width,amplitude);
  faceExtra(emotion);
}

void drawClock() {
  time_t epoch = clockClient.getEpochTime();
  struct tm *t = gmtime(&epoch);
  if (!t) return;

  String date = String(DAYS[t->tm_wday]) + ", " +
                String(t->tm_mday) + " " +
                String(MONTHS[t->tm_mon]);

  oled.setFont(u8g2_font_helvR08_tf);
  centered(date,12);

  int h=t->tm_hour;
  String ampm=(h>=12)?"PM":"AM";
  h=h%12;
  if (h==0) h=12;

  char buffer[8];
  snprintf(buffer,sizeof(buffer),"%02d:%02d",h,t->tm_min);

  oled.setFont(u8g2_font_logisoso28_tn);
  oled.drawStr(4,49,buffer);
  oled.setFont(u8g2_font_helvB08_tf);
  oled.drawStr(108,49,ampm.c_str());
}

int symbolFor(int id) {
  if (id>=200 && id<300) return 66;
  if (id>=300 && id<600) return 67;
  if (id>=600 && id<700) return 70;
  if (id>=700 && id<800) return 255;
  if (id==800) return 69;
  return 65;
}

void drawWeatherSymbol(int x,int y,int id,bool small) {
  int sym=symbolFor(id);
  if (sym==255) {
    for (int i=0;i<3;i++)
      oled.drawHLine(x,y+i*(small?4:7),small?17:28);
  } else {
    oled.setFont(small ?
      u8g2_font_open_iconic_weather_2x_t :
      u8g2_font_open_iconic_weather_4x_t);
    oled.drawGlyph(x,y,sym);
  }
}

void drawWeather() {
  oled.setFont(u8g2_font_helvR08_tf);
  centered(city.isEmpty()?"Weather":city,9);
  oled.drawHLine(0,12,128);

  char b[12];
  snprintf(b,sizeof(b),"%d",int(round(shownTemperature)));
  oled.setFont(u8g2_font_logisoso22_tn);
  oled.drawStr(5,40,b);
  int w=oled.getStrWidth(b);
  oled.drawCircle(10+w,20,3);

  oled.setFont(u8g2_font_helvR08_tf);
  oled.drawStr(18+w,22,"C");
  oled.drawStr(5,49,condition.c_str());
  drawWeatherSymbol(85,40,weatherId,false);

  oled.drawRFrame(0,52,128,12,4);
  char stats[34];
  snprintf(stats,sizeof(stats),"H:%d%%  W:%.0fm/s",
           humidity,windSpeed);
  oled.drawStr(5,61,stats);
}

void drawForecast() {
  oled.setFont(u8g2_font_helvB08_tf);
  centered("3-Day Forecast",10);
  oled.drawHLine(10,13,108);

  for (int i=0;i<3;i++) {
    int y=28+i*16;
    oled.setFont(u8g2_font_helvR08_tf);
    oled.drawStr(12,y,DAYS[(clockClient.getDay()+i+1)%7]);
    drawWeatherSymbol(54,y+3,forecastIds[i],true);

    char b[12];
    snprintf(b,sizeof(b),"%d C",
             int(round(forecastTemps[i])));
    oled.setFont(u8g2_font_helvR08_tf);
    oled.drawStr(85,y,b);
  }
}

// ---------- WEATHER API ----------

String conditionName(int id) {
  if (id>=200 && id<300) return "Storm";
  if (id>=300 && id<500) return "Drizzle";
  if (id>=500 && id<600) return "Rain";
  if (id>=600 && id<700) return "Snow";
  if (id>=700 && id<800) return "Mist/Fog";
  if (id==800) return "Clear";
  if (id==801) return "Few Clouds";
  return "Cloudy";
}

void fetchWeather() {
  if (WiFi.status()!=WL_CONNECTED || !weatherReady) return;

  String url="http://api.openweathermap.org/data/2.5/weather?lat="+
    String(lat,4)+"&lon="+String(lon,4)+
    "&units=metric&appid="+apiKey;

  HTTPClient http;
  http.setTimeout(8000);
  http.begin(url);

  if (http.GET()==200) {
    JsonDocument d;
    if (!deserializeJson(d,http.getStream())) {
      temperature=d["main"]["temp"].as<float>();
      humidity=d["main"]["humidity"].as<int>();
      windSpeed=d["wind"]["speed"].as<float>();
      weatherId=d["weather"][0]["id"].as<int>();
      condition=conditionName(weatherId);

      if (!d["timezone"].isNull()) {
        long offset=d["timezone"].as<long>();
        if (offset>=-43200 && offset<=50400) {
          utcOffset=offset;
          clockClient.setTimeOffset(offset);
          prefs.putLong("utcOffset",offset);
        }
      }
    }
  }
  http.end();
}

void fetchForecast() {
  if (WiFi.status()!=WL_CONNECTED || !weatherReady) return;

  String url="http://api.openweathermap.org/data/2.5/forecast?lat="+
    String(lat,4)+"&lon="+String(lon,4)+
    "&units=metric&appid="+apiKey;

  HTTPClient http;
  http.setTimeout(8000);
  http.begin(url);

  if (http.GET()==200) {
    JsonDocument d;
    if (!deserializeJson(d,http.getStream())) {
      // Select approximately midday from each upcoming local day.
      time_t now=clockClient.getEpochTime();
      long today=now/86400;
      bool found[3]={false,false,false};

      for (JsonVariant item : d["list"].as<JsonArray>()) {
        long dt=item["dt"].as<long>()+utcOffset;
        long day=dt/86400;
        int slot=day-today-1;
        int hour=(dt%86400)/3600;

        if (slot>=0 && slot<3 &&
            hour>=11 && hour<=15 &&
            !found[slot]) {
          forecastTemps[slot]=item["main"]["temp"].as<float>();
          forecastIds[slot]=item["weather"][0]["id"].as<int>();
          found[slot]=true;
        }
      }
    }
  }
  http.end();
}

bool geocode(String c,String s,String co,String key,
             float &outLat,float &outLon) {
  if (WiFi.status()!=WL_CONNECTED) return false;

  co.trim();
  if (co.equalsIgnoreCase("India")) co="IN";
  if (co.equalsIgnoreCase("United States")) co="US";
  if (co.equalsIgnoreCase("United Kingdom")) co="GB";
  if (co.equalsIgnoreCase("United Arab Emirates")) co="AE";

  String q=c+","+s+","+co;
  q.replace(" ","%20");
  q.replace(",","%2C");

  HTTPClient http;
  http.setTimeout(8000);
  http.begin("http://api.openweathermap.org/geo/1.0/direct?q="+
             q+"&limit=5&appid="+key);
  if (http.GET()!=200) {
    http.end();
    return false;
  }

  JsonDocument d;
  auto err=deserializeJson(d,http.getStream());
  http.end();

  if (err || !d.is<JsonArray>()) return false;

  for (JsonVariant result : d.as<JsonArray>()) {
    String rs=result["state"] | "";
    String rc=result["country"] | "";
    if (rs.equalsIgnoreCase(s) && rc.equalsIgnoreCase(co)) {
      outLat=result["lat"].as<float>();
      outLon=result["lon"].as<float>();
      return true;
    }
  }
  return false;
}

// ---------- WI-FI MANAGEMENT ----------

void startAP() {
  if (apRunning) return;

  WiFi.mode(WIFI_AP_STA);
  WiFi.softAPConfig(
    IPAddress(192,168,4,1),
    IPAddress(192,168,4,1),
    IPAddress(255,255,255,0)
  );

  if (WiFi.softAP(apName.c_str())) {
    dns.start(53,"*",WiFi.softAPIP());
    apRunning=true;
    Serial.println("AP ON: "+apName);
  }
}

void stopAP() {
  if (!apRunning) return;
  dns.stop();
  WiFi.softAPdisconnect(true);
  apRunning=false;
  WiFi.mode(WIFI_STA);
  Serial.println("AP OFF");
}

void connectNetwork(bool backup) {
  String s=backup?ssid2:ssid1;
  String p=backup?pass2:pass1;
  if (s.isEmpty()) return;

  lastAttempt=millis();
  WiFi.disconnect(false);
  WiFi.begin(s.c_str(),p.c_str());
  Serial.println("Connecting: "+s);
}

void manageWiFi() {
  if (WiFi.status()!=WL_CONNECTED) {
    connectedAt=0;
    if (!disconnectedAt) disconnectedAt=millis();

    if (millis()-lastAttempt>=RETRY_MS) {
      bool backup=useBackupNext && !ssid2.isEmpty();
      if (ssid1.isEmpty()) backup=true;
      connectNetwork(backup);
      useBackupNext=!useBackupNext;
    }

    if (!setupRequired && !apRunning &&
        millis()-disconnectedAt>=RECOVERY_MS) {
      startAP();
    }
    return;
  }

  disconnectedAt=0;

  if (!otaReady) {
    ArduinoOTA.setHostname("desk-companion");
    ArduinoOTA.setPassword("admin123");
    ArduinoOTA.begin();
    otaReady=true;
  }

  if (!mdnsReady) {
    if (MDNS.begin("desk-companion")) {
      MDNS.addService("http","tcp",80);
      mdnsReady=true;
    }
  }

  if (apRunning) {
    if (!connectedAt) connectedAt=millis();
    if (millis()-connectedAt>=AP_CLOSE_MS) {
      if (setupRequired) {
        prefs.putBool("provisioned",true);
        setupRequired=false;
        screen=0;
      }
      stopAP();
    }
  }
}

// ---------- READABLE DASHBOARD ----------

const char DASHBOARD[] PROGMEM = R"HTML(
<!doctype html>
<html lang="en">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<meta name="theme-color" content="#f7f8f6">
<title>Desk-Companion | Control Center</title>
<style>
:root{--bg:#f7f8f6;--card:#fff;--ink:#192923;--sub:#7a8880;
--line:#e2e9e3;--green:#176953;--light:#eaf4ed}
*{box-sizing:border-box}
body{margin:0;background:var(--bg);font:14px/1.5 system-ui,
-apple-system,"Segoe UI",Arial;color:var(--ink)}
button,input{font:inherit}
button{cursor:pointer;border:1px solid var(--line);border-radius:9px;
padding:11px 14px;background:#fff;color:#365448;font-weight:650}
button:hover{background:var(--light)}
.primary{background:var(--green);color:#fff;border-color:var(--green)}
.layout{display:flex;min-height:100vh}
aside{width:220px;background:#fff;padding:26px 18px;
border-right:1px solid var(--line)}
.brand{font-size:20px;font-weight:800;letter-spacing:-.7px}
.mini{color:var(--sub);font-size:10px;letter-spacing:1.7px}
nav{margin-top:36px}
nav a{display:block;padding:12px;color:#64766c;text-decoration:none;
border-radius:8px}
nav a:hover{background:var(--light)}
main{padding:32px;max-width:1200px;width:100%;margin:auto}
h1{font-size:27px;margin:0}h2{font-size:17px;margin:0 0 12px}
p{color:var(--sub)}.top{display:flex;justify-content:space-between;
align-items:center;gap:12px}
.pill{border:1px solid var(--line);background:#fff;border-radius:24px;
padding:8px 13px;font-size:12px;color:var(--green)}
.hero{margin:23px 0;background:#e9f0e9;border:1px solid #e0e9e0;
border-radius:18px;padding:26px;display:flex;align-items:center;
justify-content:space-between;gap:15px}
.hero h2{font-size:28px}
.face{background:#263f35;border:5px solid #547163;
width:110px;height:94px;border-radius:22px;display:grid;
place-content:center;gap:13px;transform:rotate(-5deg)}
.eyes{display:flex;gap:25px}
.eyes i{width:16px;height:23px;border-radius:15px;background:#c7f7d6}
.mouth{height:11px;width:32px;border-bottom:4px solid #c7f7d6;
border-radius:50%;margin:auto}
.metrics,.grid{display:grid;grid-template-columns:repeat(2,minmax(0,1fr));
gap:15px;margin-bottom:16px}
.metrics{grid-template-columns:repeat(3,minmax(0,1fr))}
.card{background:var(--card);border:1px solid var(--line);
border-radius:15px;padding:21px;min-width:0}
small{display:block;color:var(--sub);font-size:12px}
.value{font-size:18px;font-weight:750;margin-top:7px;overflow-wrap:anywhere}
.controls{display:grid;grid-template-columns:1fr 1fr;gap:8px}
.controls button.active{color:var(--green);background:var(--light);
border-color:#a8d2b8}
label{display:block;font-size:12px;font-weight:700;margin:13px 0 6px}
input{display:block;width:100%;padding:11px;border:1px solid var(--line);
border-radius:8px;outline:none}
input:focus{border-color:var(--green)}
.actions{display:flex;gap:8px;flex-wrap:wrap;margin-top:16px}
.row{display:flex;justify-content:space-between;gap:15px;
padding:11px 0;border-bottom:1px solid var(--line)}
.row span{color:var(--sub)}
.row b{text-align:right;overflow-wrap:anywhere}
.options{max-height:180px;overflow:auto;border:1px solid var(--line);
border-radius:8px;display:none}
.options.show{display:block}
.options button{display:block;width:100%;text-align:left;
border:0;border-radius:0}
.scan{display:flex;flex-wrap:wrap;gap:6px;margin:10px 0}
.scan button{font-size:12px}
#notice{font-size:12px;color:var(--green);min-height:18px;margin:15px 0}
footer{color:var(--sub);font-size:11px;margin-top:25px}
@media(max-width:850px){aside{display:none}main{padding:18px}}
@media(max-width:600px){.grid,.metrics{grid-template-columns:1fr}
.hero h2{font-size:22px}.face{width:75px;height:70px}
.eyes{gap:12px}.eyes i{width:10px;height:15px}}
</style>
</head>
<body>
<div class="layout">
<aside><div class="brand">Desk-Companion</div>
<div class="mini">CONTROL CENTER</div>
<nav><a href="#overview">Overview</a><a href="#display">Display</a>
<a href="#device">Device</a><a href="#network">Wi-Fi</a>
<a href="#weather">Weather &amp; Time</a></nav></aside>
<main id="overview">
<div class="top"><div><h1>Overview</h1>
<p>Manage your Desk-Companion.</p></div>
<div class="pill" id="connection">Connecting...</div></div>
<div class="hero"><div><small>MEET YOUR COMPANION</small>
<h2>Hello, human.</h2><p>Your pocket-sized desk companion.</p></div>
<div class="face"><div class="eyes"><i></i><i></i></div>
<div class="mouth"></div></div></div>
<div class="metrics">
<div class="card"><small>Network</small><div id="net" class="value">—</div></div>
<div class="card"><small>Chip temperature</small><div id="temp" class="value">—</div></div>
<div class="card"><small>Uptime</small><div id="uptime" class="value">—</div></div>
</div>
<div class="grid">
<section class="card" id="display"><h2>Display controls</h2>
<p>Choose what appears on your OLED.</p>
<div class="controls" id="screens">
<button data-screen="0">Emotions</button><button data-screen="1">Clock</button>
<button data-screen="2">Weather</button><button data-screen="3">Forecast</button>
</div>
<label for="emotionSearch">Search 66 emotions</label>
<input id="emotionSearch" placeholder="Search an emotion..." autocomplete="off">
<div class="options" id="emotionOptions"></div>
<small id="emotionMode">Automatic emotions</small>
<div class="actions"><button id="next">Next screen</button>
<button id="sleep">Sleep</button></div></section>
<section class="card" id="device"><h2>Device information</h2>
<div class="row"><span>Board</span><b>XIAO ESP32-C3</b></div>
<div class="row"><span>Hotspot</span><b id="ap">—</b></div>
<div class="row"><span>Hotspot status</span><b id="apStatus">—</b></div>
<div class="row"><span>Local IP</span><b id="ip">—</b></div>
<div class="row"><span>Emotion</span><b id="emo">—</b></div>
<div class="actions"><button id="refresh">Refresh</button>
<button id="restart">Restart</button></div></section>
</div>
<div class="grid" id="network">
<section class="card"><h2>Primary Wi-Fi</h2>
<form id="wifiForm"><button type="button" id="scan1">Scan Wi-Fi</button>
<div id="networks1" class="scan"></div>
<label>SSID</label><input id="ssid1" maxlength="32" required>
<label>Password</label><input id="pass1" type="password"
placeholder="Leave blank to keep existing">
<button class="primary">Save primary network</button></form></section>
<section class="card"><h2>Secondary Wi-Fi</h2>
<form id="backupForm"><button type="button" id="scan2">Scan Wi-Fi</button>
<div id="networks2" class="scan"></div>
<label>SSID</label><input id="ssid2" maxlength="32" required>
<label>Password</label><input id="pass2" type="password"
placeholder="Leave blank to keep existing">
<button class="primary">Save backup network</button></form></section>
</div>
<section class="card" id="weather"><h2>Weather &amp; time</h2>
<p>Enter the city, state and country for weather and local time.</p>
<form id="weatherForm"><label>City</label><input id="city" required>
<label>State / Province</label><input id="state" required>
<label>Country</label><input id="country" required>
<label>OpenWeatherMap API key</label><input id="key" type="password"
placeholder="Leave blank to keep existing">
<button class="primary">Save weather settings</button></form></section>
<div id="notice"></div>
<footer>Desk-Companion · Robonavigators · Local control</footer>
</main></div>
<script>
const $=id=>document.getElementById(id);
let device={},initialized=false,busy=false;
const names="Neutral,Happy,Sad,Angry,Surprised,Sleepy,Love,Wink,Confused,Scared,Bored,Excited,Crying,Laughing,Suspicious,Dizzy,Sick,Cool,Shocked,Annoyed,Curious,Thinking,Embarrassed,Proud,Worried,Blink,Jealous,Grumpy,Mischievous,Shy,Nervous,Ecstatic,Frustrated,Hopeful,Grateful,Nostalgic,Determined,Playful,Sarcastic,Smug,Guilty,Relieved,Anxious,Content,Irritated,Amused,Skeptical,Horrified,Peaceful,Starstruck,Drowsy,Puzzled,Devious,Humble,Optimistic,Pessimistic,Focused,Daydreaming,Miserable,Cheeky,Alert,Calm,Furious,Tearful,Hysterical,Smitten".split(",");
function notice(t,error=false){$("notice").textContent=t;
$("notice").style.color=error?"#b64f49":"#176953"}
async function post(url,data={}){
const r=await fetch(url,{method:"POST",headers:{"Content-Type":
"application/x-www-form-urlencoded"},body:new URLSearchParams(data)});
const j=await r.json();
if(!r.ok)throw Error(j.error||"Request failed");
notice(j.message||"Saved");return j}
async function action(url,data={}){try{await post(url,data);await status()}
catch(e){notice(e.message,true)}}
async function status(){
if(busy)return;busy=true;
try{const r=await fetch("/api/status",{cache:"no-store"});
if(!r.ok)throw Error("Offline");device=await r.json();
$("connection").textContent="Online";
$("net").textContent=device.wifiConnected?device.ssid:"Offline / AP";
$("temp").textContent=Number(device.cpuTempC).toFixed(1)+"°C";
$("uptime").textContent=Math.floor(device.uptime/3600)+"h "+
(Math.floor(device.uptime/60)%60)+"m";
$("ap").textContent=device.ap;$("apStatus").textContent=device.apActive?"ON":"OFF";
$("ip").textContent=device.ip;$("emo").textContent=names[device.emotion]||"—";
$("emotionMode").textContent=device.manualEmotion?
"Manual: "+names[device.emotion]:"Automatic — all 66 emotions";
document.querySelectorAll("[data-screen]").forEach(b=>b.classList.toggle(
"active",Number(b.dataset.screen)===device.screen));
if(!initialized){$("ssid1").value=device.savedSSID||"";
$("ssid2").value=device.savedSSID2||"";
$("city").value=device.city||"";
$("state").value=device.state||"";
$("country").value=device.country||"";
initialized=true}
}catch(e){$("connection").textContent="Offline"}finally{busy=false}}
document.querySelectorAll("[data-screen]").forEach(b=>b.onclick=()=>
action("/api/screen",{screen:b.dataset.screen}));
$("next").onclick=()=>action("/api/screen",{screen:(device.screen+1)%4});
$("refresh").onclick=status;
$("sleep").onclick=()=>{if(confirm("Put Desk-Companion to sleep?"))
action("/api/sleep")};
$("restart").onclick=()=>{if(confirm("Restart Desk-Companion?"))
action("/api/restart")};
$("wifiForm").onsubmit=e=>{e.preventDefault();
action("/api/wifi",{ssid:$("ssid1").value,password:$("pass1").value})};
$("backupForm").onsubmit=e=>{e.preventDefault();
action("/api/wifi2",{ssid:$("ssid2").value,password:$("pass2").value})};
$("weatherForm").onsubmit=e=>{e.preventDefault();
action("/api/weather",{city:$("city").value,state:$("state").value,
country:$("country").value,apiKey:$("key").value})};
function emotionList(){
let q=$("emotionSearch").value.toLowerCase(),box=$("emotionOptions");
box.replaceChildren();box.classList.add("show");
[{name:"Automatic — all 66 emotions",id:-1},
...names.map((name,id)=>({name,id}))].filter(x=>
x.name.toLowerCase().includes(q)).forEach(x=>{
const b=document.createElement("button");b.textContent=x.name;
b.onclick=()=>{box.classList.remove("show");
$("emotionSearch").value="";action("/api/emotion",{emotion:x.id})};
box.appendChild(b)})}
$("emotionSearch").oninput=emotionList;
$("emotionSearch").onfocus=emotionList;
document.addEventListener("click",e=>{if(e.target!==$("emotionSearch")&&
!$("emotionOptions").contains(e.target))
$("emotionOptions").classList.remove("show")});
async function scan(target,input){
const box=$(target);box.textContent="Scanning...";
try{const r=await fetch("/api/wifi/scan");const j=await r.json();
box.replaceChildren();
(j.networks||[]).forEach(n=>{const b=document.createElement("button");
b.type="button";b.textContent=n.ssid+" ("+n.rssi+" dBm)";
b.onclick=()=>{$(input).value=n.ssid;box.replaceChildren()};
box.appendChild(b)})}catch(e){box.textContent="Scan failed"}}
$("scan1").onclick=()=>scan("networks1","ssid1");
$("scan2").onclick=()=>scan("networks2","ssid2");
status();setInterval(status,4000);
</script>
</body>
</html>
)HTML";

// ---------- WEB ROUTES ----------

void reply(int code,const String &message,bool error=false) {
  JsonDocument d;
  d[error?"error":"message"]=message;
  String out;
  serializeJson(d,out);
  server.send(code,"application/json",out);
}

void portalRedirect() {
  if (!apRunning) {
    server.send(404,"text/plain","Not found");
    return;
  }
  server.sendHeader("Location","http://192.168.4.1/",true);
  server.send(302,"text/plain","Desk-Companion setup");
}

void setupWeb() {
  server.on("/",HTTP_GET,[](){
    server.sendHeader("Cache-Control","no-store");
    server.send_P(200,"text/html; charset=utf-8",DASHBOARD);
  });

  server.on("/generate_204",HTTP_GET,portalRedirect);
  server.on("/gen_204",HTTP_GET,portalRedirect);
  server.on("/hotspot-detect.html",HTTP_GET,portalRedirect);
  server.on("/ncsi.txt",HTTP_GET,portalRedirect);
  server.on("/connecttest.txt",HTTP_GET,portalRedirect);

  server.on("/api/status",HTTP_GET,[](){
    JsonDocument d;
    bool connected=WiFi.status()==WL_CONNECTED;
    d["wifiConnected"]=connected;
    d["ssid"]=connected?WiFi.SSID():"";
    d["savedSSID"]=ssid1;
    d["savedSSID2"]=ssid2;
    d["ap"]=apName;
    d["apActive"]=apRunning;
    d["ip"]=connected?WiFi.localIP().toString():
      WiFi.softAPIP().toString();
    d["uptime"]=millis()/1000;
    d["screen"]=screen;
    d["emotion"]=emotion;
    d["manualEmotion"]=manualEmotion;
    d["cpuTempC"]=temperatureRead();
    d["city"]=city;
    d["state"]=stateName;
    d["country"]=country;
    String out;
    serializeJson(d,out);
    server.sendHeader("Cache-Control","no-store");
    server.send(200,"application/json",out);
  });

  server.on("/api/wifi/scan",HTTP_GET,[](){
    int count=WiFi.scanNetworks(false,true);
    if (count<0) {reply(503,"Scan unavailable",true);return;}
    JsonDocument d;
    JsonArray arr=d["networks"].to<JsonArray>();
    for (int i=0;i<count && i<24;i++) {
      String name=WiFi.SSID(i);
      if (name.isEmpty()) continue;
      bool found=false;
      for (JsonObject n:arr)
        if (n["ssid"].as<String>()==name) found=true;
      if (found) continue;
      JsonObject n=arr.add<JsonObject>();
      n["ssid"]=name;
      n["rssi"]=WiFi.RSSI(i);
      n["secure"]=WiFi.encryptionType(i)!=WIFI_AUTH_OPEN;
    }
    WiFi.scanDelete();
    String out;
    serializeJson(d,out);
    server.send(200,"application/json",out);
  });

  server.on("/api/screen",HTTP_POST,[](){
    int value=server.arg("screen").toInt();
    if (value<0 || value>3) {reply(400,"Invalid screen",true);return;}
    screen=value;
    reply(200,"Screen updated");
  });

  server.on("/api/emotion",HTTP_POST,[](){
    int value=server.arg("emotion").toInt();
    if (value< -1 || value>=EMOTION_COUNT) {
      reply(400,"Invalid emotion",true);return;
    }
    manualEmotion=value>=0;
    if (manualEmotion) emotion=value;
    screen=0;
    lastEmotion=millis();
    reply(200,manualEmotion?"Emotion changed":"Automatic emotions enabled");
  });

  server.on("/api/wifi",HTTP_POST,[](){
    String s=server.arg("ssid"),p=server.arg("password");
    s.trim();
    if (s.isEmpty() || s.length()>32 || p.length()>63 ||
        (p.length()>0 && p.length()<8)) {
      reply(400,"Invalid Wi-Fi settings",true);return;
    }
    if (s!=ssid1 && p.isEmpty()) {
      reply(400,"Enter password for new network",true);return;
    }
    ssid1=s;
    if (p.length()) pass1=p;
    prefs.putString("wifiSSID",ssid1);
    prefs.putString("wifiPass",pass1);
    reply(200,"Wi-Fi saved; connecting");
    connectedAt=0;
    useBackupNext=false;
    connectNetwork(false);
  });

  server.on("/api/wifi2",HTTP_POST,[](){
    String s=server.arg("ssid"),p=server.arg("password");
    s.trim();
    if (s.isEmpty() || s.length()>32 || p.length()>63 ||
        (p.length()>0 && p.length()<8)) {
      reply(400,"Invalid backup Wi-Fi",true);return;
    }
    if (s!=ssid2 && p.isEmpty()) {
      reply(400,"Enter backup network password",true);return;
    }
    ssid2=s;
    if (p.length()) pass2=p;
    prefs.putString("wifiSSID2",ssid2);
    prefs.putString("wifiPass2",pass2);
    reply(200,"Backup Wi-Fi saved");
    if (WiFi.status()!=WL_CONNECTED) {
      lastAttempt=millis()-RETRY_MS;
      useBackupNext=true;
    }
  });

  server.on("/api/weather",HTTP_POST,[](){
    String c=server.arg("city");
    String s=server.arg("state");
    String co=server.arg("country");
    String key=server.arg("apiKey");
    c.trim();s.trim();co.trim();key.trim();
    if (c.isEmpty() || s.isEmpty() || co.isEmpty()) {
      reply(400,"Enter city, state and country",true);return;
    }
    if (c.length()>64 || s.length()>64 || co.length()>64 ||
        key.length()>128) {
      reply(400,"Field too long",true);return;
    }
    if (key.isEmpty()) key=apiKey;
    if (key.isEmpty()) {
      reply(400,"Weather API key required",true);return;
    }
    float a,b;
    if (!geocode(c,s,co,key,a,b)) {
      reply(422,"Location not found or internet unavailable",true);
      return;
    }
    city=c;stateName=s;country=co;apiKey=key;
    lat=a;lon=b;weatherReady=true;
    prefs.putString("city",city);
    prefs.putString("state",stateName);
    prefs.putString("country",country);
    prefs.putString("weatherKey",apiKey);
    prefs.putFloat("lat",lat);
    prefs.putFloat("lon",lon);
    prefs.putBool("located",true);
    lastWeather=0;
    reply(200,"Weather saved");
  });

  server.on("/api/sleep",HTTP_POST,[](){
    reply(200,"Going to sleep");
    sleepPending=true;
    sleepAt=millis();
    oled.clearBuffer();
    oled.setFont(u8g2_font_helvB10_tf);
    centered("Going to Sleep",35);
    oled.sendBuffer();
  });

  server.on("/api/restart",HTTP_POST,[](){
    reply(200,"Restarting");
    delay(300);
    ESP.restart();
  });

  server.onNotFound([](){
    if (server.uri().startsWith("/api/"))
      server.send(404,"application/json","{\"error\":\"Unknown endpoint\"}");
    else portalRedirect();
  });
}

// ---------- TOUCH AND SLEEP ----------

void handleTouch() {
  bool high=digitalRead(TOUCH_PIN)==HIGH;

  if (high && !touching) {
    touching=true;
    touchStart=millis();
    longPressDone=false;
  }

  if (high && touching && !longPressDone &&
      millis()-touchStart>1500) {
    longPressDone=true;
    sleepPending=true;
    sleepAt=millis();
    oled.clearBuffer();
    oled.setFont(u8g2_font_helvB10_tf);
    centered("Going to Sleep",35);
    oled.sendBuffer();
  }

  if (!high && touching) {
    touching=false;
    if (!longPressDone && millis()-touchStart>50) {
      manualEmotion=false;
      screen=(screen+1)%4;
    }
  }
}

void enterSleep() {
  oled.clearBuffer();
  oled.sendBuffer();
  oled.setPowerSave(1);
  WiFi.disconnect(true);
  WiFi.mode(WIFI_OFF);
  esp_deep_sleep_enable_gpio_wakeup(
    1ULL<<TOUCH_PIN,
    ESP_GPIO_WAKEUP_GPIO_HIGH
  );
  esp_deep_sleep_start();
}

// ---------- SETUP ----------

void setup() {
  Serial.begin(115200);
  pinMode(TOUCH_PIN,INPUT_PULLDOWN);

  Wire.begin();
  oled.begin();
  oled.setBusClock(400000);
  oled.setDrawColor(1);
  drawStartup();
  delay(1200);

  prefs.begin("deskbuddy",false);
  ssid1=prefs.getString("wifiSSID","");
  pass1=prefs.getString("wifiPass","");
  ssid2=prefs.getString("wifiSSID2","");
  pass2=prefs.getString("wifiPass2","");
  setupRequired=!prefs.getBool("provisioned",false) ||
                (ssid1.isEmpty() && ssid2.isEmpty());

  apiKey=prefs.getString("weatherKey","");
  city=prefs.getString("city","");
  stateName=prefs.getString("state","");
  country=prefs.getString("country","");
  lat=prefs.getFloat("lat",0);
  lon=prefs.getFloat("lon",0);
  utcOffset=prefs.getLong("utcOffset",0);
  weatherReady=!apiKey.isEmpty() && !city.isEmpty() &&
    !stateName.isEmpty() && !country.isEmpty() &&
    prefs.getBool("located",false);

  clockClient.setTimeOffset(utcOffset);

  WiFi.mode(setupRequired?WIFI_AP_STA:WIFI_STA);
  uint8_t mac[6]={0};
  esp_read_mac(mac,ESP_MAC_WIFI_STA);
  char suffix[5];
  snprintf(suffix,sizeof(suffix),"%02X%02X",mac[4],mac[5]);
  apName="Desk-Companion-"+String(suffix);

  if (setupRequired) {
    startAP();
    drawPortal(false);
  }

  setupWeb();
  server.begin();

  if (!ssid1.isEmpty()) connectNetwork(false);
  else if (!ssid2.isEmpty()) connectNetwork(true);

  disconnectedAt=millis();
  clockClient.begin();
  randomSeed(esp_random());
  lastEmotion=millis();

  Serial.println("Desk-Companion XIAO ESP32-C3");
  Serial.println("OTA password: admin123");
  Serial.println("Setup AP: "+apName);
}

// ---------- MAIN LOOP ----------

void loop() {
  if (apRunning) dns.processNextRequest();
  server.handleClient();

  if (sleepPending) {
    if (millis()-sleepAt>=4000) enterSleep();
    return;
  }

  manageWiFi();

  if (WiFi.status()==WL_CONNECTED) {
    if (otaReady) ArduinoOTA.handle();
    clockClient.update();
  }

  if (setupRequired) {
    if (millis()-lastFrame>=1000) {
      lastFrame=millis();
      drawPortal(false);
    }
    return;
  }

  if (apRunning && WiFi.status()!=WL_CONNECTED) {
    if (millis()-lastFrame>=1000) {
      lastFrame=millis();
      drawPortal(true);
    }
    return;
  }

  handleTouch();
  if (sleepPending) return;

  if (WiFi.status()==WL_CONNECTED && weatherReady &&
      (!lastWeather || millis()-lastWeather>=WEATHER_MS)) {
    fetchWeather();
    fetchForecast();
    lastWeather=millis();
  }

  shownTemperature+=(temperature-shownTemperature)*0.08f;

  if (millis()-lastFrame<FRAME_MS) return;
  lastFrame=millis();
  oled.clearBuffer();

  switch (screen) {
    case 0: drawEmotion();break;
    case 1: drawClock();break;
    case 2: drawWeather();break;
    case 3: drawForecast();break;
  }

  oled.sendBuffer();
}
