
/*
  DESK-COMPANION — XIAO ESP32-S3
  Robonavigators

  TTP223: D10 / GPIO9
  OLED: SSD1306 128x64 I2C

  66 emotions, clock, weather, forecast,
  dual Wi-Fi, captive portal, 30-second
  automatic AP recovery, OTA and web UI.

  OTA password: admin123
  Arduino ESP32 Core 3.x
  Libraries: U8g2, ArduinoJson 7, NTPClient
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

#define TOUCH_PIN 9
#define LED_PIN 21
#define RETRY_MS 15000UL
#define RECOVERY_MS 30000UL
#define CLOSE_MS 1800UL
#define WEATHER_MS 900000UL
#define FRAME_MS 33UL

WebServer web(80);
DNSServer dns;
Preferences prefs;
WiFiUDP udp;
NTPClient ntp(udp,"time.google.com",0,60000);
U8G2_SSD1306_128X64_NONAME_F_HW_I2C oled(
  U8G2_R0,U8X8_PIN_NONE
);

String wifi1,password1,wifi2,password2;
String hotspot,city,stateName,country,weatherKey;
float latitude=0,longitude=0;
long utcOffset=0;

bool firstSetup=false;
bool apEnabled=false;
bool otaEnabled=false;
bool mdnsEnabled=false;
bool weatherEnabled=false;
bool backupNext=false;

unsigned long offlineAt=0;
unsigned long onlineAt=0;
unsigned long retryAt=0;
unsigned long weatherAt=0;
unsigned long frameAt=0;

bool sleeping=false;
unsigned long sleepAt=0;
bool pressed=false;
bool longDone=false;
unsigned long pressAt=0;

int screen=0;
int emotion=0;
bool manualEmotion=false;
unsigned long emotionAt=0;
unsigned long emotionHold=5000;
int pupilX=0,pupilY=0,targetX=0,targetY=0;
unsigned long eyeAt=0,targetAt=0;

String condition="Loading...";
float temperature=0,shownTemp=0,wind=0;
int humidity=0,weatherID=800;
float forecastTemp[3]={0,0,0};
int forecastID[3]={800,800,800};

const char* DAYS[]={"Sun","Mon","Tue","Wed","Thu","Fri","Sat"};
const char* MONTHS[]={
  "Jan","Feb","Mar","Apr","May","Jun",
  "Jul","Aug","Sep","Oct","Nov","Dec"
};

const char* EMOTIONS[]={
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

const int EMOTION_COUNT=66;

// ====================================================
// DISPLAY
// ====================================================

void centerText(const String &text,int y){
  int x=max(0,(128-(int)oled.getStrWidth(text.c_str()))/2);
  oled.drawStr(x,y,text.c_str());
}

void startupScreen(){
  oled.clearBuffer();
  oled.setFont(u8g2_font_logisoso16_tf);
  centerText("DESK",27);
  centerText("COMPANION",54);
  oled.sendBuffer();
}

void setupScreen(bool recovery){
  oled.clearBuffer();
  oled.setFont(u8g2_font_6x10_tf);
  oled.drawStr(0,10,recovery?"WI-FI RECOVERY":"SETUP REQUIRED");
  oled.drawHLine(0,13,128);
  oled.drawStr(0,24,"Connect to:");
  oled.drawStr(0,36,"Desk-Companion-");
  oled.drawStr(92,36,hotspot.substring(hotspot.length()-4).c_str());
  oled.drawStr(0,49,"Open 192.168.4.1");
  oled.drawStr(0,61,recovery?"Retrying Wi-Fi...":"Waiting for Wi-Fi");
  oled.sendBuffer();
}

void drawOneEye(int x,int y,int style){
  switch(style){
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
}

void drawMouth(int width,int curve){
  if(width<2)return;
  int px=64-width/2,py=50;
  for(int i=-width/2;i<=width/2;i+=2){
    float t=float(i)/float(width/2);
    int y=50+int(curve*t*t);
    oled.drawLine(px,py,64+i,y);
    px=64+i;py=y;
  }
}

void drawExtras(){
  if(emotion==6 || emotion==65){
    for(int x:{35,93}){
      oled.drawDisc(x-5,23,5);
      oled.drawDisc(x+5,23,5);
      oled.drawTriangle(x-10,25,x+10,25,x,38);
    }
  }
  if(emotion==12 || emotion==58 || emotion==63){
    oled.drawFilledEllipse(28,53,3,8);
    oled.drawFilledEllipse(100,53,3,8);
  }
  if(emotion==8 || emotion==21 || emotion==51){
    oled.setFont(u8g2_font_helvB10_tf);
    oled.drawStr(60,13,"?");
  }
  if(emotion==5 || emotion==50 || emotion==57){
    oled.setFont(u8g2_font_helvB10_tf);
    oled.drawStr(105,13,"Z");
  }
  if(emotion==22 || emotion==29){
    oled.drawFilledEllipse(25,47,6,3);
    oled.drawFilledEllipse(103,47,6,3);
  }
  if(emotion==11 || emotion==31 || emotion==49 ||
     emotion==64){
    oled.drawLine(8,7,8,20);
    oled.drawLine(2,13,14,13);
    oled.drawLine(120,7,120,20);
    oled.drawLine(114,13,126,13);
  }
}

void drawFace(){
  if(!manualEmotion && millis()-emotionAt>=emotionHold){
    int next;
    do{
      next=random(EMOTION_COUNT);
    }while(next==emotion);
    emotion=next;
    emotionAt=millis();
    emotionHold=random(3000,8000);
  }

  if(emotion==0){
    if(millis()-targetAt>2000){
      targetX=random(-7,8);
      targetY=random(-6,7);
      targetAt=millis();
    }
    if(millis()-eyeAt>=35){
      if(pupilX<targetX)pupilX++;
      if(pupilX>targetX)pupilX--;
      if(pupilY<targetY)pupilY++;
      if(pupilY>targetY)pupilY--;
      eyeAt=millis();
    }
  }else{
    pupilX=pupilY=0;
  }

  int style=(emotion*7+emotion/3)%8;
  int curve=(emotion*11)%27-10;
  int width=22+(emotion*13)%33;

  if(emotion==0){style=0;width=0;}
  if(emotion==1){style=3;curve=14;}
  if(emotion==2){style=1;curve=-12;}
  if(emotion==3){style=0;curve=-8;}
  if(emotion==5){style=2;curve=3;}
  if(emotion==7){style=4;curve=12;}
  if(emotion==13){style=2;curve=20;}
  if(emotion==15 || emotion==16)style=5;
  if(emotion==25 || emotion==48)style=2;

  drawOneEye(35,31,style);
  drawOneEye(93,31,emotion==7?2:style);

  if(emotion==3 || emotion==19 ||
     emotion==32 || emotion==62){
    oled.drawLine(22,13,46,21);
    oled.drawLine(82,21,106,13);
  }
  drawMouth(width,curve);
  drawExtras();
}

void drawClock(){
  time_t epoch=ntp.getEpochTime();
  struct tm *t=gmtime(&epoch);
  if(!t)return;

  String date=String(DAYS[t->tm_wday])+", "+
    String(t->tm_mday)+" "+MONTHS[t->tm_mon];

  oled.setFont(u8g2_font_helvR08_tf);
  centerText(date,12);

  int h=t->tm_hour;
  const char* ampm=h>=12?"PM":"AM";
  h=h%12;
  if(!h)h=12;

  char b[8];
  snprintf(b,sizeof(b),"%02d:%02d",h,t->tm_min);

  oled.setFont(u8g2_font_logisoso28_tn);
  oled.drawStr(4,49,b);
  oled.setFont(u8g2_font_helvB08_tf);
  oled.drawStr(108,49,ampm);
}

int iconFor(int id){
  if(id>=200 && id<300)return 66;
  if(id>=300 && id<600)return 67;
  if(id>=600 && id<700)return 70;
  if(id>=700 && id<800)return 255;
  if(id==800)return 69;
  return 65;
}

void weatherGlyph(int x,int y,int id,bool small){
  int symbol=iconFor(id);
  if(symbol==255){
    for(int i=0;i<3;i++)
      oled.drawHLine(x,y+i*(small?4:7),small?17:28);
  }else{
    oled.setFont(small?
      u8g2_font_open_iconic_weather_2x_t:
      u8g2_font_open_iconic_weather_4x_t);
    oled.drawGlyph(x,y,symbol);
  }
}

void drawWeather(){
  oled.setFont(u8g2_font_helvR08_tf);
  centerText(city.isEmpty()?"Weather":city,9);
  oled.drawHLine(0,12,128);

  char b[12];
  snprintf(b,sizeof(b),"%d",int(round(shownTemp)));
  oled.setFont(u8g2_font_logisoso22_tn);
  oled.drawStr(5,40,b);
  int w=oled.getStrWidth(b);
  oled.drawCircle(w+10,20,3);

  oled.setFont(u8g2_font_helvR08_tf);
  oled.drawStr(w+18,22,"C");
  oled.drawStr(5,49,condition.c_str());
  weatherGlyph(85,40,weatherID,false);

  oled.drawRFrame(0,52,128,12,4);
  char stats[32];
  snprintf(stats,sizeof(stats),"H:%d%%  W:%.0fm/s",
    humidity,wind);
  oled.drawStr(5,61,stats);
}

void drawForecast(){
  oled.setFont(u8g2_font_helvB08_tf);
  centerText("3-Day Forecast",10);
  oled.drawHLine(10,13,108);

  for(int i=0;i<3;i++){
    int y=28+i*16;
    oled.setFont(u8g2_font_helvR08_tf);
    oled.drawStr(12,y,DAYS[(ntp.getDay()+i+1)%7]);
    weatherGlyph(54,y+3,forecastID[i],true);

    char b[12];
    snprintf(b,sizeof(b),"%d C",
      int(round(forecastTemp[i])));
    oled.setFont(u8g2_font_helvR08_tf);
    oled.drawStr(85,y,b);
  }
}

// ====================================================
// OPENWEATHERMAP
// ====================================================

String weatherName(int id){
  if(id>=200 && id<300)return "Storm";
  if(id>=300 && id<500)return "Drizzle";
  if(id>=500 && id<600)return "Rain";
  if(id>=600 && id<700)return "Snow";
  if(id>=700 && id<800)return "Mist/Fog";
  if(id==800)return "Clear";
  if(id==801)return "Few Clouds";
  return "Cloudy";
}

void getCurrentWeather(){
  if(WiFi.status()!=WL_CONNECTED || !weatherEnabled)return;

  String url="http://api.openweathermap.org/data/2.5/weather?lat="+
    String(latitude,4)+"&lon="+String(longitude,4)+
    "&units=metric&appid="+weatherKey;

  HTTPClient http;
  http.setTimeout(8000);
  http.begin(url);
  if(http.GET()==200){
    JsonDocument d;
    if(!deserializeJson(d,http.getStream())){
      temperature=d["main"]["temp"].as<float>();
      humidity=d["main"]["humidity"].as<int>();
      wind=d["wind"]["speed"].as<float>();
      weatherID=d["weather"][0]["id"].as<int>();
      condition=weatherName(weatherID);

      if(!d["timezone"].isNull()){
        long offset=d["timezone"].as<long>();
        if(offset>=-43200 && offset<=50400){
          utcOffset=offset;
          ntp.setTimeOffset(offset);
          prefs.putLong("utcOffset",offset);
        }
      }
    }
  }
  http.end();
}

void getForecast(){
  if(WiFi.status()!=WL_CONNECTED || !weatherEnabled)return;

  String url="http://api.openweathermap.org/data/2.5/forecast?lat="+
    String(latitude,4)+"&lon="+String(longitude,4)+
    "&units=metric&appid="+weatherKey;

  HTTPClient http;
  http.setTimeout(8000);
  http.begin(url);
  if(http.GET()==200){
    JsonDocument d;
    if(!deserializeJson(d,http.getStream())){
      long today=ntp.getEpochTime()/86400;
      bool found[3]={false,false,false};

      for(JsonVariant item:d["list"].as<JsonArray>()){
        long ts=item["dt"].as<long>()+utcOffset;
        int slot=ts/86400-today-1;
        int hour=(ts%86400)/3600;

        if(slot>=0 && slot<3 &&
           hour>=11 && hour<=15 && !found[slot]){
          forecastTemp[slot]=item["main"]["temp"].as<float>();
          forecastID[slot]=item["weather"][0]["id"].as<int>();
          found[slot]=true;
        }
      }
    }
  }
  http.end();
}

bool locate(String c,String s,String co,String key,
            float &a,float &b){
  if(WiFi.status()!=WL_CONNECTED)return false;

  if(co.equalsIgnoreCase("India"))co="IN";
  if(co.equalsIgnoreCase("United States"))co="US";
  if(co.equalsIgnoreCase("United Kingdom"))co="GB";
  if(co.equalsIgnoreCase("United Arab Emirates"))co="AE";

  String q=c+","+s+","+co;
  q.replace(" ","%20");
  q.replace(",","%2C");

  HTTPClient http;
  http.setTimeout(8000);
  http.begin("http://api.openweathermap.org/geo/1.0/direct?q="+
    q+"&limit=5&appid="+key);
  if(http.GET()!=200){http.end();return false;}

  JsonDocument d;
  auto err=deserializeJson(d,http.getStream());
  http.end();
  if(err || !d.is<JsonArray>())return false;

  for(JsonVariant item:d.as<JsonArray>()){
    String rs=item["state"] | "";
    String rc=item["country"] | "";
    if(rs.equalsIgnoreCase(s) && rc.equalsIgnoreCase(co)){
      a=item["lat"].as<float>();
      b=item["lon"].as<float>();
      return true;
    }
  }
  return false;
}

// ====================================================
// WI-FI AND OTA
// ====================================================

void startHotspot(){
  if(apEnabled)return;
  WiFi.mode(WIFI_AP_STA);
  WiFi.softAPConfig(
    IPAddress(192,168,4,1),
    IPAddress(192,168,4,1),
    IPAddress(255,255,255,0)
  );

  if(WiFi.softAP(hotspot.c_str())){
    dns.start(53,"*",WiFi.softAPIP());
    apEnabled=true;
    Serial.println("AP ON: "+hotspot);
  }
}

void stopHotspot(){
  if(!apEnabled)return;
  dns.stop();
  WiFi.softAPdisconnect(true);
  apEnabled=false;
  WiFi.mode(WIFI_STA);
  Serial.println("AP OFF");
}

void connectWiFi(bool backup){
  String s=backup?wifi2:wifi1;
  String p=backup?password2:password1;
  if(s.isEmpty())return;
  retryAt=millis();
  WiFi.disconnect(false);
  WiFi.begin(s.c_str(),p.c_str());
  Serial.println("Connecting to "+s);
}

void manageConnection(){
  if(WiFi.status()!=WL_CONNECTED){
    onlineAt=0;
    if(!offlineAt)offlineAt=millis();

    if(millis()-retryAt>=RETRY_MS){
      bool backup=backupNext && !wifi2.isEmpty();
      if(wifi1.isEmpty())backup=true;
      connectWiFi(backup);
      backupNext=!backupNext;
    }

    if(!firstSetup && !apEnabled &&
       millis()-offlineAt>=RECOVERY_MS){
      startHotspot();
    }
    return;
  }

  offlineAt=0;

  if(!otaEnabled){
    ArduinoOTA.setHostname("desk-companion");
    ArduinoOTA.setPassword("admin123");
    ArduinoOTA.begin();
    otaEnabled=true;
  }

  if(!mdnsEnabled){
    if(MDNS.begin("desk-companion")){
      MDNS.addService("http","tcp",80);
      mdnsEnabled=true;
    }
  }

  if(apEnabled){
    if(!onlineAt)onlineAt=millis();

    if(millis()-onlineAt>=CLOSE_MS){
      if(firstSetup){
        prefs.putBool("provisioned",true);
        firstSetup=false;
        screen=0;
      }
      stopHotspot();
    }
  }
}

// ====================================================
// READABLE RESPONSIVE DASHBOARD
// ====================================================

const char DASHBOARD[] PROGMEM=R"HTML(
<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<meta name="theme-color" content="#f7f8f6">
<title>Desk-Companion | S3 Control Center</title>
<style>
:root{--bg:#f7f8f6;--card:#fff;--ink:#192923;
--sub:#7a8880;--line:#e2e9e3;
--accent:#176953;--soft:#eaf4ed}
*{box-sizing:border-box}
body{margin:0;background:var(--bg);
font:14px/1.5 system-ui,-apple-system,"Segoe UI",Arial;
color:var(--ink)}
button,input{font:inherit}
button{cursor:pointer;background:#fff;border:1px solid var(--line);
border-radius:9px;padding:11px 14px;color:#365448;font-weight:650}
button:hover{background:var(--soft)}
.primary{background:var(--accent);border-color:var(--accent);color:#fff}
.layout{display:flex;min-height:100vh}
aside{width:220px;flex-shrink:0;background:#fff;
border-right:1px solid var(--line);padding:26px 18px}
.brand{font-size:20px;font-weight:800;letter-spacing:-.7px}
.mini{color:var(--sub);font-size:10px;letter-spacing:1.6px}
nav{margin-top:35px}
nav a{display:block;padding:12px;color:#64766c;
text-decoration:none;border-radius:8px}
nav a:hover{background:var(--soft)}
main{width:100%;max-width:1200px;margin:auto;padding:32px}
h1{font-size:27px;margin:0}h2{font-size:17px;margin:0 0 12px}
p,small{color:var(--sub)}
.top{display:flex;align-items:center;justify-content:space-between;gap:15px}
.badge{background:#fff;border:1px solid var(--line);
color:var(--accent);border-radius:30px;padding:9px 13px}
.hero{background:#e9f0e9;border-radius:18px;padding:27px;
margin:24px 0;display:flex;justify-content:space-between;
align-items:center;gap:14px}
.hero h2{font-size:28px;margin:6px 0}
.face{width:110px;height:94px;background:#263f35;
border:5px solid #547163;border-radius:22px;
display:grid;place-content:center;gap:13px;transform:rotate(-5deg)}
.eyes{display:flex;gap:25px}
.eyes i{display:block;width:16px;height:23px;
border-radius:15px;background:#c7f7d6}
.smile{width:32px;height:11px;border-bottom:4px solid #c7f7d6;
border-radius:50%;margin:auto}
.metrics,.grid{display:grid;gap:15px;margin-bottom:16px;
grid-template-columns:repeat(2,minmax(0,1fr))}
.metrics{grid-template-columns:repeat(3,minmax(0,1fr))}
.card{background:#fff;border:1px solid var(--line);
border-radius:15px;padding:21px;min-width:0}
.card small{display:block;font-size:12px}
.value{font-size:18px;font-weight:750;margin-top:8px;overflow-wrap:anywhere}
label{display:block;font-size:12px;font-weight:700;margin:13px 0 6px}
input{display:block;width:100%;padding:11px;border:1px solid var(--line);
border-radius:8px;outline:none}
input:focus{border-color:var(--accent)}
.controls{display:grid;grid-template-columns:1fr 1fr;gap:8px}
.controls button.active{color:var(--accent);background:var(--soft);
border-color:#a8d2b8}
.actions{display:flex;gap:8px;flex-wrap:wrap;margin-top:16px}
.row{display:flex;justify-content:space-between;gap:15px;
padding:11px 0;border-bottom:1px solid var(--line)}
.row span{color:var(--sub)}
.row b{text-align:right;overflow-wrap:anywhere}
.options{display:none;max-height:180px;overflow:auto;
border:1px solid var(--line);border-radius:8px}
.options.show{display:block}
.options button{display:block;width:100%;text-align:left;border:0}
.scan{display:flex;flex-wrap:wrap;gap:6px;margin:10px 0}
.scan button{font-size:12px}
#notice{font-size:12px;color:var(--accent);min-height:18px;margin:15px 0}
footer{color:var(--sub);font-size:11px;margin-top:25px}
@media(max-width:850px){aside{display:none}main{padding:18px}}
@media(max-width:600px){.grid,.metrics{grid-template-columns:1fr}
.hero h2{font-size:22px}.face{width:75px;height:70px}
.eyes{gap:12px}.eyes i{width:10px;height:15px}}
</style>
</head>
<body>
<div class="layout">
<aside>
<div class="brand">Desk-Companion</div>
<div class="mini">CONTROL CENTER</div>
<nav>
<a href="#overview">Overview</a>
<a href="#display">Display</a>
<a href="#device">Device</a>
<a href="#network">Wi-Fi</a>
<a href="#weather">Weather &amp; Time</a>
</nav>
</aside>
<main id="overview">
<div class="top"><div>
<h1>Overview</h1><p>Manage your Desk-Companion.</p>
</div><div class="badge" id="connection">Connecting...</div></div>

<div class="hero"><div><small>MEET YOUR COMPANION</small>
<h2>Hello, human.</h2><p>Your pocket-sized desk companion.</p>
</div><div class="face"><div class="eyes"><i></i><i></i></div>
<div class="smile"></div></div></div>

<div class="metrics">
<div class="card"><small>Network</small><div id="net" class="value">—</div></div>
<div class="card"><small>Chip temperature</small><div id="temp" class="value">—</div></div>
<div class="card"><small>Uptime</small><div id="uptime" class="value">—</div></div>
</div>

<div class="grid">
<section class="card" id="display">
<h2>Display controls</h2><p>Choose what appears on the OLED.</p>
<div class="controls">
<button data-screen="0">Emotions</button>
<button data-screen="1">Clock</button>
<button data-screen="2">Weather</button>
<button data-screen="3">Forecast</button>
</div>
<label>Search 66 emotions</label>
<input id="emotionSearch" placeholder="Search an emotion..." autocomplete="off">
<div class="options" id="emotionOptions"></div>
<small id="emotionMode">Automatic emotions</small>
<div class="actions"><button id="next">Next screen</button>
<button id="sleep">Sleep</button></div>
</section>

<section class="card" id="device"><h2>Device information</h2>
<div class="row"><span>Board</span><b>XIAO ESP32-S3</b></div>
<div class="row"><span>Hotspot</span><b id="ap">—</b></div>
<div class="row"><span>Hotspot status</span><b id="apStatus">—</b></div>
<div class="row"><span>Local IP</span><b id="ip">—</b></div>
<div class="row"><span>Emotion</span><b id="emo">—</b></div>
<div class="actions"><button id="refresh">Refresh</button>
<button id="restart">Restart</button></div>
</section></div>

<div class="grid" id="network">
<section class="card"><h2>Primary Wi-Fi</h2>
<form id="wifiForm"><button id="scan1" type="button">Scan Wi-Fi</button>
<div class="scan" id="networks1"></div>
<label>SSID</label><input id="ssid1" maxlength="32" required>
<label>Password</label><input id="pass1" type="password"
placeholder="Leave blank to keep existing">
<button class="primary">Save primary network</button></form></section>

<section class="card"><h2>Secondary Wi-Fi</h2>
<form id="backupForm"><button id="scan2" type="button">Scan Wi-Fi</button>
<div class="scan" id="networks2"></div>
<label>SSID</label><input id="ssid2" maxlength="32" required>
<label>Password</label><input id="pass2" type="password"
placeholder="Leave blank to keep existing">
<button class="primary">Save backup network</button></form></section>
</div>

<section class="card" id="weather"><h2>Weather &amp; time</h2>
<p>Enter your city, state and country.</p>
<form id="weatherForm">
<label>City</label><input id="city" required>
<label>State / Province</label><input id="state" required>
<label>Country</label><input id="country" required>
<label>OpenWeatherMap API key</label>
<input id="key" type="password" placeholder="Leave blank to keep existing">
<button class="primary">Save weather settings</button>
</form></section>

<div id="notice"></div>
<footer>Desk-Companion · Robonavigators · Local control</footer>
</main></div>

<script>
const $=id=>document.getElementById(id);
const emotions="Neutral,Happy,Sad,Angry,Surprised,Sleepy,Love,Wink,Confused,Scared,Bored,Excited,Crying,Laughing,Suspicious,Dizzy,Sick,Cool,Shocked,Annoyed,Curious,Thinking,Embarrassed,Proud,Worried,Blink,Jealous,Grumpy,Mischievous,Shy,Nervous,Ecstatic,Frustrated,Hopeful,Grateful,Nostalgic,Determined,Playful,Sarcastic,Smug,Guilty,Relieved,Anxious,Content,Irritated,Amused,Skeptical,Horrified,Peaceful,Starstruck,Drowsy,Puzzled,Devious,Humble,Optimistic,Pessimistic,Focused,Daydreaming,Miserable,Cheeky,Alert,Calm,Furious,Tearful,Hysterical,Smitten".split(",");
let device={},initialized=false,busy=false;
function notice(t,error=false){
$("notice").textContent=t;
$("notice").style.color=error?"#b64f49":"#176953"}
async function post(path,data={}){
const r=await fetch(path,{method:"POST",
headers:{"Content-Type":"application/x-www-form-urlencoded"},
body:new URLSearchParams(data)});
const j=await r.json();
if(!r.ok)throw Error(j.error||"Request failed");
notice(j.message||"Saved");return j}
async function action(path,data={}){
try{await post(path,data);await status()}
catch(e){notice(e.message,true)}}
async function status(){
if(busy)return;busy=true;
try{
const r=await fetch("/api/status",{cache:"no-store"});
if(!r.ok)throw Error("Offline");
device=await r.json();
$("connection").textContent="Online";
$("net").textContent=device.wifiConnected?device.ssid:"Offline / AP";
$("temp").textContent=Number(device.cpuTempC).toFixed(1)+"°C";
$("uptime").textContent=Math.floor(device.uptime/3600)+"h "+
(Math.floor(device.uptime/60)%60)+"m";
$("ap").textContent=device.ap;
$("apStatus").textContent=device.apActive?"ON":"OFF";
$("ip").textContent=device.ip;
$("emo").textContent=emotions[device.emotion]||"—";
$("emotionMode").textContent=device.manualEmotion?
"Manual: "+emotions[device.emotion]:"Automatic — all 66 emotions";
document.querySelectorAll("[data-screen]").forEach(b=>
b.classList.toggle("active",Number(b.dataset.screen)===device.screen));
if(!initialized){
$("ssid1").value=device.savedSSID||"";
$("ssid2").value=device.savedSSID2||"";
$("city").value=device.city||"";
$("state").value=device.state||"";
$("country").value=device.country||"";
initialized=true}
}catch(e){$("connection").textContent="Offline"}
finally{busy=false}}
document.querySelectorAll("[data-screen]").forEach(b=>
b.onclick=()=>action("/api/screen",{screen:b.dataset.screen}));
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
function showEmotions(){
const q=$("emotionSearch").value.toLowerCase();
const box=$("emotionOptions");
box.replaceChildren();box.classList.add("show");
[{name:"Automatic — all 66 emotions",id:-1},
...emotions.map((name,id)=>({name,id}))].filter(x=>
x.name.toLowerCase().includes(q)).forEach(x=>{
const b=document.createElement("button");
b.textContent=x.name;
b.onclick=()=>{box.classList.remove("show");
$("emotionSearch").value="";
action("/api/emotion",{emotion:x.id})};
box.appendChild(b)})}
$("emotionSearch").oninput=showEmotions;
$("emotionSearch").onfocus=showEmotions;
document.addEventListener("click",e=>{
if(e.target!==$("emotionSearch")&&
!$("emotionOptions").contains(e.target))
$("emotionOptions").classList.remove("show")});
async function scan(target,input){
const box=$(target);box.textContent="Scanning...";
try{
const r=await fetch("/api/wifi/scan");
const j=await r.json();
box.replaceChildren();
(j.networks||[]).forEach(n=>{
const b=document.createElement("button");
b.type="button";b.textContent=n.ssid+" ("+n.rssi+" dBm)";
b.onclick=()=>{$(input).value=n.ssid;box.replaceChildren()};
box.appendChild(b)})}
catch(e){box.textContent="Scan failed"}}
$("scan1").onclick=()=>scan("networks1","ssid1");
$("scan2").onclick=()=>scan("networks2","ssid2");
status();setInterval(status,4000);
</script>
</body>
</html>
)HTML";

// ====================================================
// WEB API
// ====================================================

void respond(int code,String message,bool error=false){
  JsonDocument d;
  d[error?"error":"message"]=message;
  String output;serializeJson(d,output);
  web.send(code,"application/json",output);
}

void portalRedirect(){
  if(!apEnabled){
    web.send(404,"text/plain","Not found");
    return;
  }
  web.sendHeader("Location","http://192.168.4.1/",true);
  web.send(302,"text/plain","Desk-Companion setup");
}

void setupWeb(){
  web.on("/",HTTP_GET,[](){
    web.sendHeader("Cache-Control","no-store");
    web.send_P(200,"text/html; charset=utf-8",DASHBOARD);
  });

  web.on("/generate_204",HTTP_GET,portalRedirect);
  web.on("/gen_204",HTTP_GET,portalRedirect);
  web.on("/hotspot-detect.html",HTTP_GET,portalRedirect);
  web.on("/ncsi.txt",HTTP_GET,portalRedirect);
  web.on("/connecttest.txt",HTTP_GET,portalRedirect);

  web.on("/api/status",HTTP_GET,[](){
    JsonDocument d;
    bool connected=WiFi.status()==WL_CONNECTED;
    d["wifiConnected"]=connected;
    d["ssid"]=connected?WiFi.SSID():"";
    d["savedSSID"]=wifi1;
    d["savedSSID2"]=wifi2;
    d["ap"]=hotspot;
    d["apActive"]=apEnabled;
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
    String output;serializeJson(d,output);
    web.sendHeader("Cache-Control","no-store");
    web.send(200,"application/json",output);
  });

  web.on("/api/wifi/scan",HTTP_GET,[](){
    int count=WiFi.scanNetworks(false,true);
    if(count<0){respond(503,"Scan failed",true);return;}
    JsonDocument d;
    JsonArray arr=d["networks"].to<JsonArray>();
    for(int i=0;i<count && i<24;i++){
      String name=WiFi.SSID(i);
      if(name.isEmpty())continue;
      bool found=false;
      for(JsonObject n:arr)
        if(n["ssid"].as<String>()==name)found=true;
      if(found)continue;
      JsonObject n=arr.add<JsonObject>();
      n["ssid"]=name;n["rssi"]=WiFi.RSSI(i);
      n["secure"]=WiFi.encryptionType(i)!=WIFI_AUTH_OPEN;
    }
    WiFi.scanDelete();
    String output;serializeJson(d,output);
    web.send(200,"application/json",output);
  });

  web.on("/api/screen",HTTP_POST,[](){
    int value=web.arg("screen").toInt();
    if(value<0 || value>3){
      respond(400,"Invalid screen",true);return;
    }
    screen=value;
    respond(200,"Screen updated");
  });

  web.on("/api/emotion",HTTP_POST,[](){
    int value=web.arg("emotion").toInt();
    if(value< -1 || value>=EMOTION_COUNT){
      respond(400,"Invalid emotion",true);return;
    }
    manualEmotion=value>=0;
    if(manualEmotion)emotion=value;
    screen=0;emotionAt=millis();
    respond(200,manualEmotion?
      "Emotion changed":"Automatic emotions enabled");
  });

  web.on("/api/wifi",HTTP_POST,[](){
    String s=web.arg("ssid"),p=web.arg("password");
    s.trim();
    if(s.isEmpty() || s.length()>32 || p.length()>63 ||
       (p.length()>0 && p.length()<8)){
      respond(400,"Invalid Wi-Fi settings",true);return;
    }
    if(s!=wifi1 && p.isEmpty()){
      respond(400,"Enter new network password",true);return;
    }
    wifi1=s;
    if(p.length())password1=p;
    prefs.putString("wifiSSID",wifi1);
    prefs.putString("wifiPass",password1);
    respond(200,"Wi-Fi saved; connecting");
    onlineAt=0;backupNext=false;
    connectWiFi(false);
  });

  web.on("/api/wifi2",HTTP_POST,[](){
    String s=web.arg("ssid"),p=web.arg("password");
    s.trim();
    if(s.isEmpty() || s.length()>32 || p.length()>63 ||
       (p.length()>0 && p.length()<8)){
      respond(400,"Invalid backup Wi-Fi",true);return;
    }
    if(s!=wifi2 && p.isEmpty()){
      respond(400,"Enter backup password",true);return;
    }
    wifi2=s;
    if(p.length())password2=p;
    prefs.putString("wifiSSID2",wifi2);
    prefs.putString("wifiPass2",password2);
    respond(200,"Backup Wi-Fi saved");
    if(WiFi.status()!=WL_CONNECTED){
      retryAt=millis()-RETRY_MS;
      backupNext=true;
    }
  });

  web.on("/api/weather",HTTP_POST,[](){
    String c=web.arg("city");
    String s=web.arg("state");
    String co=web.arg("country");
    String key=web.arg("apiKey");
    c.trim();s.trim();co.trim();key.trim();

    if(c.isEmpty() || s.isEmpty() || co.isEmpty()){
      respond(400,"Enter city, state and country",true);
      return;
    }
    if(c.length()>64 || s.length()>64 ||
       co.length()>64 || key.length()>128){
      respond(400,"Field too long",true);return;
    }
    if(key.isEmpty())key=weatherKey;
    if(key.isEmpty()){
      respond(400,"API key required",true);return;
    }
    float a,b;
    if(!locate(c,s,co,key,a,b)){
      respond(422,"Location not found or internet unavailable",true);
      return;
    }
    city=c;stateName=s;country=co;weatherKey=key;
    latitude=a;longitude=b;weatherEnabled=true;
    prefs.putString("city",city);
    prefs.putString("state",stateName);
    prefs.putString("country",country);
    prefs.putString("weatherKey",weatherKey);
    prefs.putFloat("lat",latitude);
    prefs.putFloat("lon",longitude);
    prefs.putBool("located",true);
    weatherAt=0;
    respond(200,"Weather saved");
  });

  web.on("/api/sleep",HTTP_POST,[](){
    respond(200,"Going to sleep");
    sleeping=true;sleepAt=millis();
    oled.clearBuffer();
    oled.setFont(u8g2_font_helvB10_tf);
    centerText("Going to Sleep",35);
    oled.sendBuffer();
  });

  web.on("/api/restart",HTTP_POST,[](){
    respond(200,"Restarting");
    delay(300);
    ESP.restart();
  });

  web.onNotFound([](){
    if(web.uri().startsWith("/api/"))
      web.send(404,"application/json",
        "{\"error\":\"Unknown endpoint\"}");
    else portalRedirect();
  });
}

// ====================================================
// TOUCH AND DEEP SLEEP
// ====================================================

void handleTouch(){
  bool high=digitalRead(TOUCH_PIN)==HIGH;

  if(high && !pressed){
    pressed=true;
    pressAt=millis();
    longDone=false;
  }

  if(high && pressed && !longDone &&
     millis()-pressAt>1500){
    longDone=true;
    sleeping=true;
    sleepAt=millis();
    oled.clearBuffer();
    oled.setFont(u8g2_font_helvB10_tf);
    centerText("Going to Sleep",35);
    oled.sendBuffer();
  }

  if(!high && pressed){
    pressed=false;
    if(!longDone && millis()-pressAt>50){
      manualEmotion=false;
      screen=(screen+1)%4;
    }
  }
}

void enterDeepSleep(){
  oled.clearBuffer();
  oled.sendBuffer();
  oled.setPowerSave(1);
  WiFi.disconnect(true);
  WiFi.mode(WIFI_OFF);

  // ESP32-S3 uses RTC EXT0 wakeup on GPIO9.
  esp_sleep_enable_ext0_wakeup(
    GPIO_NUM_9,
    1
  );

  esp_deep_sleep_start();
}

// ====================================================
// SETUP
// ====================================================

void setup(){
  Serial.begin(115200);

  pinMode(TOUCH_PIN,INPUT_PULLDOWN);

  // Onboard XIAO ESP32-S3 user LED: active LOW.
  pinMode(LED_PIN,OUTPUT);
  digitalWrite(LED_PIN,LOW);

  Wire.begin();
  oled.begin();
  oled.setBusClock(400000);
  oled.setDrawColor(1);

  startupScreen();
  delay(1200);

  prefs.begin("deskbuddy",false);

  wifi1=prefs.getString("wifiSSID","");
  password1=prefs.getString("wifiPass","");
  wifi2=prefs.getString("wifiSSID2","");
  password2=prefs.getString("wifiPass2","");

  firstSetup=!prefs.getBool("provisioned",false) ||
    (wifi1.isEmpty() && wifi2.isEmpty());

  weatherKey=prefs.getString("weatherKey","");
  city=prefs.getString("city","");
  stateName=prefs.getString("state","");
  country=prefs.getString("country","");
  latitude=prefs.getFloat("lat",0);
  longitude=prefs.getFloat("lon",0);
  utcOffset=prefs.getLong("utcOffset",0);

  weatherEnabled=!weatherKey.isEmpty() &&
    !city.isEmpty() && !stateName.isEmpty() &&
    !country.isEmpty() && prefs.getBool("located",false);

  ntp.setTimeOffset(utcOffset);

  WiFi.mode(firstSetup?WIFI_AP_STA:WIFI_STA);

  uint8_t mac[6]={0};
  esp_read_mac(mac,ESP_MAC_WIFI_STA);
  char suffix[5];
  snprintf(suffix,sizeof(suffix),"%02X%02X",mac[4],mac[5]);
  hotspot="Desk-Companion-"+String(suffix);

  if(firstSetup){
    startHotspot();
    setupScreen(false);
  }

  setupWeb();
  web.begin();

  if(!wifi1.isEmpty())connectWiFi(false);
  else if(!wifi2.isEmpty())connectWiFi(true);

  offlineAt=millis();
  ntp.begin();
  randomSeed(esp_random());
  emotionAt=millis();

  Serial.println("Desk-Companion XIAO ESP32-S3");
  Serial.println("OTA password: admin123");
  Serial.println("Hotspot: "+hotspot);
}

// ====================================================
// MAIN LOOP
// ====================================================

void loop(){
  if(apEnabled)dns.processNextRequest();
  web.handleClient();

  if(sleeping){
    if(millis()-sleepAt>=4000)enterDeepSleep();
    return;
  }

  manageConnection();

  if(WiFi.status()==WL_CONNECTED){
    if(otaEnabled)ArduinoOTA.handle();
    ntp.update();
  }

  if(firstSetup){
    if(millis()-frameAt>=1000){
      frameAt=millis();
      setupScreen(false);
    }
    return;
  }

  if(apEnabled && WiFi.status()!=WL_CONNECTED){
    if(millis()-frameAt>=1000){
      frameAt=millis();
      setupScreen(true);
    }
    return;
  }

  handleTouch();
  if(sleeping)return;

  if(WiFi.status()==WL_CONNECTED && weatherEnabled &&
     (!weatherAt || millis()-weatherAt>=WEATHER_MS)){
    getCurrentWeather();
    getForecast();
    weatherAt=millis();
  }

  shownTemp+=(temperature-shownTemp)*0.08f;

  if(millis()-frameAt<FRAME_MS)return;
  frameAt=millis();

  oled.clearBuffer();

  switch(screen){
    case 0:drawFace();break;
    case 1:drawClock();break;
    case 2:drawWeather();break;
    case 3:drawForecast();break;
  }

  oled.sendBuffer();
}
