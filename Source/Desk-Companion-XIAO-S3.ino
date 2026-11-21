/*
  Desk-Companion - XIAO ESP32-S3 / WebServer edition
  Open-source friendly: no personal data, default network credentials, or API keys.
  Required libraries: U8g2, ArduinoJson 7, NTPClient; ESP32 Arduino core.
  Built-in: WiFi, WebServer, Preferences, ESPmDNS, ArduinoOTA, HTTPClient.
  Touch TTP223 OUT -> D10 / GPIO9. OLED I2C -> board SDA/SCL.
  First boot: connect to open device hotspot shown on OLED (or Serial).
  Captive portal: DNSServer wildcard DNS + OS detection redirects to the dashboard.
  Connect to hotspot, visit http://192.168.4.1, configure WiFi/weather.
  Dashboard and device hotspot have no password. Use only in trusted environments.
  For local/trusted networks only: HTTP is not encrypted. Avoid untrusted WiFi.
  Wi-Fi/API secrets saved in ESP32 NVS (not encrypted against physical extraction).
*/
// Desk-Companion for Seeed XIAO ESP32-S3 (no speaker)
// TTP223 OUT -> D10 / GPIO9. Select XIAO ESP32S3 in Arduino IDE.
// Requires Arduino-ESP32 core with ESP32-S3 EXT0 deep-sleep wake support.
#include <WiFi.h>
#include <WebServer.h>
#include <DNSServer.h>
#include <Preferences.h>
#include <ESPmDNS.h>
#include <HTTPClient.h>
#include <WiFiUdp.h>
#include <NTPClient.h>
#include <ArduinoJson.h>
#include <U8g2lib.h>
#include <Wire.h>
#include <ArduinoOTA.h>

// --- Generic device configuration; no personal credentials in firmware ---
#define TTP_PIN 9  // XIAO ESP32-S3 D10 / GPIO9 (RTC-capable wake pin)
WebServer server(80);
DNSServer captiveDns;
const uint16_t CAPTIVE_DNS_PORT = 53;
Preferences prefs;
String wifiSSID, wifiPassword, wifiSSID2, wifiPassword2, weatherKey, cityLabel = "", locationState = "", locationCountry = "";
String apSSID, adminSecret; // adminSecret retained only for OTA authentication
float weatherLat = 0.0f, weatherLon = 0.0f;
int utcOffsetSeconds = 0;
bool weatherConfigured = false, otaStarted = false, portalReady = false;
bool setupRequired = false; // True only until at least one Wi-Fi profile is saved in NVS
unsigned long lastWiFiAttempt = 0;
bool trySecondaryNext = false;
const unsigned long WIFI_RETRY_MS = 15000;
const char* SCREEN_NAMES[] = {"Emotions", "Clock", "Weather", "Forecast"};
bool manualEmotion = false;
void setupWebServer();

// Hardware Objects
U8G2_SSD1306_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0, /* reset=*/ U8X8_PIN_NONE);

// Time Settings
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "time.google.com", 0, 60000); 
String days[] = {"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"};
String months[] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};

// Weather settings are stored locally in NVS and never included in firmware.
String currentCondition = "Loading...";
float currentTemp = 0.0;
float displayedTemp = 0.0;      // smoothed value shown on screen
uint16_t weatherIcon = 69; 
int currentHumidity = 0;
float currentWindSpeed = 0.0;
String currentWindDir = "N";

// Forecast Data
float fc_temp[3] = {0, 0, 0};
uint16_t fc_icon[3] = {69, 69, 69};
unsigned long lastWeatherUpdate = 0;

// State Management
int displayState = 0; 
unsigned long bootTime = 0;

// Button & Deep Sleep Variables
bool isPressing = false;
unsigned long btnPressTime = 0;
bool longPressHandled = false;
bool isGoingToSleep = false;
unsigned long sleepTimer = 0;

// Frame Rate Limiter
unsigned long lastFrameTime = 0;
const int FRAME_DELAY = 33; 

// Eye Animation Variables (used only in NEUTRAL emotion for idle look-around)
int currentPupilX = 0, currentPupilY = 0;
int targetPupilX = 0, targetPupilY = 0;
unsigned long lastEyeUpdate = 0;
unsigned long lastEyePause = 0;
int eyePauseDuration = 2000;

// ==================================================================
//  EMOTION SYSTEM — 66 emotions (26 original + 40 new)
// ==================================================================
enum Emotion {
  NEUTRAL, HAPPY, SAD, ANGRY, SURPRISED, SLEEPY, LOVE, WINK,
  CONFUSED, SCARED, BORED, EXCITED, CRYING, LAUGHING, SUSPICIOUS,
  DIZZY, SICK, COOL, SHOCKED, ANNOYED, CURIOUS, THINKING,
  EMBARRASSED, PROUD, WORRIED, BLINK,
  JEALOUS, GRUMPY, MISCHIEVOUS, SHY, NERVOUS, ECSTATIC, FRUSTRATED,
  HOPEFUL, GRATEFUL, NOSTALGIC, DETERMINED, PLAYFUL, SARCASTIC, SMUG,
  GUILTY, RELIEVED, ANXIOUS, CONTENT, IRRITATED, AMUSED, SKEPTICAL,
  HORRIFIED, PEACEFUL, STARSTRUCK, DROWSY, PUZZLED, DEVIOUS, HUMBLE,
  OPTIMISTIC, PESSIMISTIC, FOCUSED, DAYDREAMING, MISERABLE, CHEEKY,
  ALERT, CALM, FURIOUS, TEARFUL, HYSTERICAL, SMITTEN
};
const int EMOTION_COUNT = 66;
Emotion currentEmotion = NEUTRAL;
unsigned long lastEmotionChange = 0;
unsigned long emotionHoldTime = 5000;

enum EyeShape { EYE_DISC, EYE_RING, EYE_CLOSED_LINE, EYE_ELLIPSE, EYE_HALF, EYE_X, EYE_DOT_RING, EYE_SQUINT };
enum Extra { EX_NONE, EX_TEARS, EX_SPARKLE, EX_QMARK, EX_SWEAT, EX_HEARTS, EX_SPIRAL, EX_ZZZ, EX_BLUSH, EX_EXCLAIM };

struct EmotionParams {
  EyeShape eye;
  int browTiltL;
  int browTiltR;
  bool showBrow;
  int mouthAmp;
  int mouthWidth;
  Extra extra;
};

EmotionParams EMO[EMOTION_COUNT] = {
  /*NEUTRAL*/       {EYE_DISC, 0, 0, false, 0, 0, EX_NONE},
  /*HAPPY*/         {EYE_ELLIPSE, 0, 0, false, 14, 50, EX_NONE},
  /*SAD*/           {EYE_RING, -8, 8, true, -14, 46, EX_NONE},
  /*ANGRY*/         {EYE_DISC, 10, -10, true, -8, 40, EX_NONE},
  /*SURPRISED*/     {EYE_RING, 0, 0, false, 0, 12, EX_EXCLAIM},
  /*SLEEPY*/        {EYE_CLOSED_LINE, 0, 0, false, 3, 20, EX_NONE},
  /*LOVE*/          {EYE_DISC, 0, 0, false, 12, 40, EX_HEARTS},
  /*WINK*/          {EYE_HALF, 0, 0, false, 12, 40, EX_NONE},
  /*CONFUSED*/      {EYE_RING, 6, 0, true, -4, 26, EX_QMARK},
  /*SCARED*/        {EYE_RING, -10, 10, true, -10, 20, EX_SWEAT},
  /*BORED*/         {EYE_CLOSED_LINE, 0, 0, false, 0, 28, EX_NONE},
  /*EXCITED*/       {EYE_ELLIPSE, 0, 0, false, 18, 56, EX_SPARKLE},
  /*CRYING*/        {EYE_RING, 0, 0, false, -10, 30, EX_TEARS},
  /*LAUGHING*/      {EYE_CLOSED_LINE, 0, 0, false, 20, 44, EX_NONE},
  /*SUSPICIOUS*/    {EYE_HALF, 6, 0, true, -3, 30, EX_NONE},
  /*DIZZY*/         {EYE_X, 0, 0, false, -4, 24, EX_SPIRAL},
  /*SICK*/          {EYE_X, 0, 0, false, -6, 22, EX_NONE},
  /*COOL*/          {EYE_SQUINT, 0, 0, false, 6, 34, EX_NONE},
  /*SHOCKED*/       {EYE_RING, 0, 0, false, 0, 8, EX_NONE},
  /*ANNOYED*/       {EYE_HALF, -6, 6, true, 0, 28, EX_NONE},
  /*CURIOUS*/       {EYE_DISC, 0, 10, true, 4, 24, EX_NONE},
  /*THINKING*/      {EYE_HALF, 0, 0, false, 0, 20, EX_QMARK},
  /*EMBARRASSED*/   {EYE_DISC, 0, 0, false, 4, 20, EX_BLUSH},
  /*PROUD*/         {EYE_HALF, 4, -4, true, 12, 46, EX_NONE},
  /*WORRIED*/       {EYE_RING, -8, 8, true, -6, 24, EX_SWEAT},
  /*BLINK*/         {EYE_CLOSED_LINE, 0, 0, false, 0, 24, EX_NONE},
  /*JEALOUS*/       {EYE_HALF, -4, -4, true, -6, 24, EX_NONE},
  /*GRUMPY*/        {EYE_HALF, -8, 8, true, -10, 30, EX_NONE},
  /*MISCHIEVOUS*/   {EYE_HALF, 8, 4, true, 10, 34, EX_NONE},
  /*SHY*/           {EYE_DISC, 0, 0, false, 4, 18, EX_BLUSH},
  /*NERVOUS*/       {EYE_RING, -6, 6, true, -3, 20, EX_SWEAT},
  /*ECSTATIC*/      {EYE_ELLIPSE, 0, 0, false, 22, 58, EX_SPARKLE},
  /*FRUSTRATED*/    {EYE_HALF, -10, 10, true, -8, 32, EX_NONE},
  /*HOPEFUL*/       {EYE_DISC, 6, 6, true, 8, 30, EX_NONE},
  /*GRATEFUL*/      {EYE_HALF, 2, 2, true, 10, 36, EX_NONE},
  /*NOSTALGIC*/     {EYE_HALF, 0, 0, false, 4, 28, EX_NONE},
  /*DETERMINED*/    {EYE_DISC, 8, -8, true, 2, 30, EX_NONE},
  /*PLAYFUL*/       {EYE_DISC, 0, 6, true, 14, 42, EX_NONE},
  /*SARCASTIC*/     {EYE_HALF, 0, 8, true, -2, 26, EX_NONE},
  /*SMUG*/          {EYE_HALF, 0, 0, false, 8, 30, EX_NONE},
  /*GUILTY*/        {EYE_DISC, 0, 0, false, -4, 18, EX_SWEAT},
  /*RELIEVED*/      {EYE_HALF, 0, 0, false, 8, 32, EX_NONE},
  /*ANXIOUS*/       {EYE_RING, -6, 6, true, -6, 22, EX_SWEAT},
  /*CONTENT*/       {EYE_HALF, 0, 0, false, 6, 30, EX_NONE},
  /*IRRITATED*/     {EYE_DISC, -8, 8, true, -4, 26, EX_NONE},
  /*AMUSED*/        {EYE_HALF, 0, 0, false, 10, 36, EX_NONE},
  /*SKEPTICAL*/     {EYE_HALF, 8, 0, true, -2, 24, EX_NONE},
  /*HORRIFIED*/     {EYE_RING, -10, 10, true, -12, 18, EX_SWEAT},
  /*PEACEFUL*/      {EYE_CLOSED_LINE, 0, 0, false, 6, 26, EX_NONE},
  /*STARSTRUCK*/    {EYE_DISC, 0, 0, false, 10, 36, EX_SPARKLE},
  /*DROWSY*/        {EYE_HALF, 0, 0, false, 0, 20, EX_ZZZ},
  /*PUZZLED*/       {EYE_RING, 6, -4, true, -2, 22, EX_QMARK},
  /*DEVIOUS*/       {EYE_HALF, 10, 4, true, -2, 32, EX_NONE},
  /*HUMBLE*/        {EYE_HALF, -2, -2, true, 4, 24, EX_NONE},
  /*OPTIMISTIC*/    {EYE_DISC, 6, 6, true, 10, 36, EX_NONE},
  /*PESSIMISTIC*/   {EYE_HALF, -6, -6, true, -8, 26, EX_NONE},
  /*FOCUSED*/       {EYE_SQUINT, 0, 0, false, -2, 24, EX_NONE},
  /*DAYDREAMING*/   {EYE_HALF, 0, 0, false, 2, 22, EX_ZZZ},
  /*MISERABLE*/     {EYE_RING, -10, 10, true, -14, 30, EX_TEARS},
  /*CHEEKY*/        {EYE_HALF, 4, 4, true, 12, 38, EX_NONE},
  /*ALERT*/         {EYE_RING, 4, 4, true, 0, 20, EX_NONE},
  /*CALM*/          {EYE_HALF, 0, 0, false, 4, 28, EX_NONE},
  /*FURIOUS*/       {EYE_DISC, 14, -14, true, -10, 38, EX_NONE},
  /*TEARFUL*/       {EYE_RING, -4, 4, true, -8, 26, EX_TEARS},
  /*HYSTERICAL*/    {EYE_CLOSED_LINE, 0, 0, false, 22, 50, EX_SPARKLE},
  /*SMITTEN*/       {EYE_DISC, 0, 0, false, 14, 40, EX_HEARTS},
};

// ---------- helper drawing primitives ----------
void drawMouthCurve(int cx, int cy, int w, int amp) {
  if (w <= 0) return;
  int prevX = cx - w / 2, prevY = cy;
  for (int i = -w / 2; i <= w / 2; i += 2) {
    float t = (float)i / (w / 2.0);
    int y = cy + (int)(amp * (t * t));
    u8g2.drawLine(prevX, prevY, cx + i, y);
    prevX = cx + i; prevY = y;
  }
}

void drawEyebrow(int x, int y, int len, int angleTilt) {
  u8g2.drawLine(x - len / 2, y, x + len / 2, y - angleTilt);
}

void drawStarburst(int cx, int cy, int r) {
  for (int a = 0; a < 360; a += 45) {
    float rad = a * PI / 180.0;
    u8g2.drawLine(cx, cy, cx + cos(rad) * r, cy + sin(rad) * r);
  }
}

void drawOneEye(int cx, int cy, EyeShape shape) {
  switch (shape) {
    case EYE_DISC:
      u8g2.drawDisc(cx, cy, 18);
      u8g2.setDrawColor(0);
      u8g2.drawDisc(cx + currentPupilX, cy + currentPupilY, 6);
      u8g2.setDrawColor(1);
      break;
    case EYE_RING:
      u8g2.drawCircle(cx, cy, 18);
      u8g2.drawDisc(cx, cy, 5);
      break;
    case EYE_CLOSED_LINE:
      u8g2.drawBox(cx - 16, cy - 2, 32, 4);
      break;
    case EYE_ELLIPSE:
      u8g2.drawFilledEllipse(cx, cy, 18, 10);
      break;
    case EYE_HALF:
      u8g2.drawBox(cx - 16, cy - 2, 32, 4);
      u8g2.drawCircle(cx, cy + 4, 12);
      break;
    case EYE_X:
      u8g2.drawLine(cx - 8, cy - 8, cx + 8, cy + 8);
      u8g2.drawLine(cx - 8, cy + 8, cx + 8, cy - 8);
      break;
    case EYE_DOT_RING:
      u8g2.drawCircle(cx, cy, 14);
      u8g2.drawDisc(cx, cy, 3);
      break;
    case EYE_SQUINT:
      u8g2.drawBox(cx - 18, cy - 4, 36, 8);
      break;
  }
}

void drawExtra(Extra ex, int lx, int rx, int ey) {
  switch (ex) {
    case EX_TEARS:
      u8g2.drawFilledEllipse(lx - 6, ey + 22, 3, 8);
      u8g2.drawFilledEllipse(rx + 6, ey + 22, 3, 8);
      break;
    case EX_SPARKLE:
      drawStarburst(15, 15, 6);
      drawStarburst(113, 15, 6);
      break;
    case EX_QMARK:
      u8g2.setFont(u8g2_font_helvB10_tf);
      u8g2.drawStr(60, 14, "?");
      break;
    case EX_EXCLAIM:
      u8g2.setFont(u8g2_font_helvB10_tf);
      u8g2.drawStr(60, 14, "!");
      break;
    case EX_SWEAT:
      u8g2.drawFilledEllipse(rx + 20, ey - 10, 3, 6);
      break;
    case EX_HEARTS:
      for (int i = 0; i < 2; i++) {
        int cx = (i == 0) ? lx : rx;
        u8g2.drawDisc(cx - 6, ey - 4, 5);
        u8g2.drawDisc(cx + 6, ey - 4, 5);
        u8g2.drawTriangle(cx - 10, ey - 2, cx + 10, ey - 2, cx, ey + 10);
      }
      break;
    case EX_SPIRAL:
      u8g2.drawCircle(lx, ey, 6);
      u8g2.drawCircle(rx, ey, 6);
      break;
    case EX_ZZZ:
      u8g2.setFont(u8g2_font_helvB10_tf);
      u8g2.drawStr(95, 14, "Z");
      break;
    case EX_BLUSH:
      u8g2.drawFilledEllipse(lx - 2, ey + 16, 7, 3);
      u8g2.drawFilledEllipse(rx + 2, ey + 16, 7, 3);
      break;
    case EX_NONE: default: break;
  }
}

void drawFace(Emotion e) {
  int lx = 35, rx = 93, ey = 32;
  EmotionParams p = EMO[e];

  drawOneEye(lx, ey, p.eye);
  drawOneEye(rx, ey, p.eye);

  if (p.showBrow) {
    drawEyebrow(lx, ey - 20, 22, p.browTiltL);
    drawEyebrow(rx, ey - 20, 22, p.browTiltR);
  }

  if (p.mouthWidth > 0) drawMouthCurve(64, 52, p.mouthWidth, p.mouthAmp);

  drawExtra(p.extra, lx, rx, ey);
}

void updateEmotion() {
  if (!manualEmotion && millis() - lastEmotionChange > emotionHoldTime) {
    Emotion next;
    do {
      next = (Emotion)random(0, EMOTION_COUNT);
    } while (next == currentEmotion);
    currentEmotion = next;
    lastEmotionChange = millis();
    emotionHoldTime = random(3000, 8000);
  }
}

// ---------- Startup Screen ----------
void drawStartupScreen() {
  // Same Logisoso16 font used by the original Desk Buddy startup animation.
  // Two lines prevent the longer new project name from overflowing the OLED.
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_logisoso16_tf);
  const char* title1 = "DESK";
  const char* title2 = "COMPANION";
  u8g2.drawStr((128 - u8g2.getStrWidth(title1)) / 2, 27, title1);
  u8g2.drawStr((128 - u8g2.getStrWidth(title2)) / 2, 54, title2);
  u8g2.sendBuffer();
}

void drawFirstTimeSetupScreen() {
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_6x10_tf);
  u8g2.drawStr(0, 10, "SETUP REQUIRED");
  u8g2.drawHLine(0, 13, 128);
  u8g2.drawStr(0, 24, "Connect to Wi-Fi:");
  u8g2.drawStr(0, 36, apSSID.c_str());
  u8g2.drawStr(0, 49, "Open 192.168.4.1");
  u8g2.drawStr(0, 61, "Save Wi-Fi to start");
  u8g2.sendBuffer();
}

void setup() {
  Serial.begin(115200);
  pinMode(TTP_PIN, INPUT_PULLDOWN);
  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, LOW); // XIAO ESP32-S3 user LED GPIO21: active LOW
  u8g2.begin();
  u8g2.setBusClock(400000);
  u8g2.setDrawColor(1);
  drawStartupScreen();
  delay(1200);

  prefs.begin("deskbuddy", false); // Legacy NVS namespace retained to preserve existing saved settings
  wifiSSID = prefs.getString("wifiSSID", "");
  wifiPassword = prefs.getString("wifiPass", "");
  wifiSSID2 = prefs.getString("wifiSSID2", "");
  wifiPassword2 = prefs.getString("wifiPass2", "");
  setupRequired = wifiSSID.isEmpty() && wifiSSID2.isEmpty(); // Do not force weather setup: it is optional
  weatherKey = prefs.getString("weatherKey", "");
  cityLabel = prefs.getString("city", "");
  locationState = prefs.getString("state", "");
  locationCountry = prefs.getString("country", "");
  weatherLat = prefs.getFloat("lat", 0.0f);
  weatherLon = prefs.getFloat("lon", 0.0f);
  utcOffsetSeconds = prefs.getInt("utcOffset", 0);
  weatherConfigured = weatherKey.length() > 0 && cityLabel.length() > 0 && locationState.length() > 0 && locationCountry.length() > 0 && prefs.getBool("located", false);
  timeClient.setTimeOffset(utcOffsetSeconds);

  // Open setup access point. Use the LAST FOUR hexadecimal characters
  // of the STA Wi-Fi MAC address (the last two bytes as displayed).
  // For example, AA:BB:CC:DD:57:B9 -> Desk-Companion-57B9.
  // WiFi.mode() must initialize the Wi-Fi interface before reading its MAC.
  WiFi.mode(WIFI_AP_STA);
  String staMac = WiFi.macAddress();
  staMac.replace(":", "");
  staMac.toUpperCase();
  String suffix = staMac.substring(staMac.length() - 4);
  apSSID = "Desk-Companion-" + suffix;

  // Device-specific OTA password, not required to access the open dashboard.
  uint32_t unique = (uint32_t)(ESP.getEfuseMac() & 0xFFFFFFFFULL);
  char id[9]; snprintf(id, sizeof(id), "%08X", unique);
  adminSecret = "DB-" + String(id);
  Serial.println("Station MAC: " + WiFi.macAddress());
  WiFi.softAPConfig(IPAddress(192,168,4,1), IPAddress(192,168,4,1), IPAddress(255,255,255,0));
  bool apStarted = WiFi.softAP(apSSID.c_str());
  if (apStarted) captiveDns.start(CAPTIVE_DNS_PORT, "*", WiFi.softAPIP());
  else Serial.println("WARNING: Desk-Companion access point failed to start");
  if (wifiSSID.length()) WiFi.begin(wifiSSID.c_str(), wifiPassword.c_str());
  else if (wifiSSID2.length()) WiFi.begin(wifiSSID2.c_str(), wifiPassword2.c_str());
  setupWebServer();
  server.begin();
  portalReady = true;
  Serial.println("Desk-Companion setup network: " + apSSID);
  Serial.println("Setup hotspot: OPEN (no password)");
  Serial.println("Hotspot dashboard: http://192.168.4.1/");
  Serial.println("On home/classroom Wi-Fi: http://desk-companion.local/ (if mDNS is supported)");
  // Display the setup instructions only for a device with no saved Wi-Fi.
  // On configured devices, the startup logo transitions directly to emotions.
  if (setupRequired) drawFirstTimeSetupScreen();

  timeClient.begin();
  bootTime = millis();
  randomSeed(esp_random());
}

void loop() {
  captiveDns.processNextRequest(); // DNS wildcard: captive portal clients resolve to the ESP32 AP
  server.handleClient();
  if (isGoingToSleep) {
    if (millis() - sleepTimer >= 4000) {
      u8g2.clearBuffer(); u8g2.sendBuffer(); u8g2.setPowerSave(1);
      esp_sleep_enable_ext0_wakeup(static_cast<gpio_num_t>(TTP_PIN), 1); // ESP32-S3 RTC GPIO9, HIGH wakes
      esp_deep_sleep_start();
    }
    return;
  }

  // Try the preferred network, then the backup every 15 seconds when disconnected.
  // The local setup hotspot stays available regardless of Wi-Fi connectivity.
  if (WiFi.status() != WL_CONNECTED && (wifiSSID.length() || wifiSSID2.length()) &&
      millis() - lastWiFiAttempt >= WIFI_RETRY_MS) {
    lastWiFiAttempt = millis();
    const bool useBackup = (trySecondaryNext && wifiSSID2.length()) || !wifiSSID.length();
    WiFi.disconnect(false);
    if (useBackup) WiFi.begin(wifiSSID2.c_str(), wifiPassword2.c_str());
    else WiFi.begin(wifiSSID.c_str(), wifiPassword.c_str());
    trySecondaryNext = !trySecondaryNext;
  }
  if (WiFi.status() == WL_CONNECTED) {
    if (!otaStarted) {
      ArduinoOTA.setHostname("desk-companion");
      ArduinoOTA.setPassword(adminSecret.c_str());
      ArduinoOTA.begin();
      // mDNS allows http://desk-companion.local/ on the same local Wi-Fi.
      // Captive-portal hotspot users can always use http://192.168.4.1/.
      if (MDNS.begin("desk-companion")) {
        MDNS.addService("http", "tcp", 80);
        Serial.println("Local dashboard: http://desk-companion.local/");
      } else {
        Serial.println("mDNS startup failed; use the device IP address");
      }
      otaStarted = true;
    }
    ArduinoOTA.handle();
    timeClient.update();
  }

  // Keep the captive portal, web server and Wi-Fi reconnect logic running
  // while showing first-time setup. As soon as Wi-Fi is saved, return to UI.
  if (setupRequired) {
    if (millis() - lastFrameTime >= 1000) {
      lastFrameTime = millis();
      drawFirstTimeSetupScreen();
    }
    return;
  }

  handleTouch();
  if (isGoingToSleep) return;
  if (WiFi.status() == WL_CONNECTED && weatherConfigured &&
      (millis() - lastWeatherUpdate > 900000 || lastWeatherUpdate == 0)) {
    fetchWeather(); fetchForecast(); lastWeatherUpdate = millis();
  }
  displayedTemp += (currentTemp - displayedTemp) * 0.08;
  if (fabs(currentTemp - displayedTemp) < 0.05) displayedTemp = currentTemp;

  if (millis() - lastFrameTime >= FRAME_DELAY) {
    lastFrameTime = millis();
    u8g2.clearBuffer();
    time_t epochTime = timeClient.getEpochTime();
    struct tm *ptm = gmtime(&epochTime);
    if (displayState == 0) {
      updateEmotion();
      if (currentEmotion == NEUTRAL && millis() - lastEyeUpdate >= 15) {
        if (currentPupilX < targetPupilX) currentPupilX++;
        else if (currentPupilX > targetPupilX) currentPupilX--;
        if (currentPupilY < targetPupilY) currentPupilY++;
        else if (currentPupilY > targetPupilY) currentPupilY--;
        lastEyeUpdate = millis();
        if (currentPupilX == targetPupilX && currentPupilY == targetPupilY) {
          if (millis() - lastEyePause > eyePauseDuration) {
            int r = random(0, 6);
            if (r == 0) { targetPupilX = 0; targetPupilY = 0; }
            else if (r == 1) { targetPupilX = -9; targetPupilY = 0; }
            else if (r == 2) { targetPupilX = 9; targetPupilY = 0; }
            else if (r == 3) { targetPupilX = 0; targetPupilY = -9; }
            else if (r == 4) { targetPupilX = 0; targetPupilY = 9; }
            else { targetPupilX = random(-6, 7); targetPupilY = random(-6, 7); }
            eyePauseDuration = random(1000, 3500); lastEyePause = millis();
          }
        } else lastEyePause = millis();
      }
      drawFace(currentEmotion);
    } else if (displayState == 1) drawTimeScreen(ptm);
    else if (displayState == 2) drawCurrentWeather();
    else if (displayState == 3) drawForecastScreen();
    u8g2.sendBuffer();
  }
}

// --- Touch Input & Deep Sleep Trigger ---
void handleTouch() {
  bool btnState = digitalRead(TTP_PIN);

  if (btnState == HIGH && !isPressing) {
    isPressing = true;
    btnPressTime = millis();
    longPressHandled = false;
  }

  if (btnState == HIGH && isPressing) {
    if (!longPressHandled && millis() - btnPressTime > 1500) {
      longPressHandled = true;
      triggerDeepSleep();
    }
  }

  if (btnState == LOW && isPressing) {
    isPressing = false;
    if (!longPressHandled && millis() - btnPressTime > 50) { 
      manualEmotion = false;
      displayState++;
      if (displayState > 3) displayState = 0; 
    }
  }
}

void triggerDeepSleep() {
  isGoingToSleep = true; 
  sleepTimer = millis();
  
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_helvB10_tf);
  int w = u8g2.getStrWidth("Going to Sleep Zzz");
  u8g2.drawStr((128 - w) / 2, 35, "Going to Sleep Zzz");
  u8g2.sendBuffer();

}

// --- Time Screen ---
void drawTimeScreen(struct tm *ptm) {
  String dateStr = days[timeClient.getDay()] + ", " + String(ptm->tm_mday) + " " + months[ptm->tm_mon];
  u8g2.setFont(u8g2_font_helvR08_tf);
  int dateWidth = u8g2.getStrWidth(dateStr.c_str());
  u8g2.drawStr((128 - dateWidth) / 2, 12, dateStr.c_str());

  int currentHour = ptm->tm_hour;
  int currentMin = ptm->tm_min;
  String amPmStr = (currentHour >= 12) ? "PM" : "AM";
  if (currentHour == 0) currentHour = 12;
  else if (currentHour > 12) currentHour -= 12;
  
  String timeStr = (currentHour < 10 ? "0" : "") + String(currentHour) + ":" + (currentMin < 10 ? "0" : "") + String(currentMin);
  
  u8g2.setFont(u8g2_font_logisoso28_tn); 
  int timeWidth = u8g2.getStrWidth(timeStr.c_str());
  
  u8g2.setFont(u8g2_font_helvB08_tf); 
  int amPmWidth = u8g2.getStrWidth(amPmStr.c_str());
  
  int startX = (128 - (timeWidth + 2 + amPmWidth)) / 2;
  
  u8g2.setFont(u8g2_font_logisoso28_tn); 
  u8g2.drawStr(startX, 50, timeStr.c_str());
  u8g2.setFont(u8g2_font_helvB08_tf); 
  u8g2.drawStr(startX + timeWidth + 2, 50, amPmStr.c_str());
}

// Custom mist symbol: three fog lines, independent of font glyph availability.
const uint16_t WEATHER_ICON_MIST = 255;
void drawMistIcon(int x, int y, bool small) {
  int width = small ? 17 : 30;
  int gap = small ? 4 : 7;
  for (int i = 0; i < 3; i++) {
    int offset = (i == 1) ? 4 : 0;
    u8g2.drawHLine(x + offset, y + i * gap, width - offset);
    if (!small) u8g2.drawHLine(x + offset, y + i * gap + 1, width - offset);
  }
}

// --- Weather Screen (u8g2 port of the reference layout, smoothed temp) ---
void drawCurrentWeather() {
  // 1. HEADER (Location)
  u8g2.setFont(u8g2_font_helvR08_tf);
  int locW = u8g2.getStrWidth(cityLabel.c_str());
  u8g2.drawStr((128 - locW) / 2, 9, cityLabel.c_str());
  u8g2.drawHLine(0, 12, 128);

  // 2. MAIN BODY (Temp & Icon)
  u8g2.setFont(u8g2_font_logisoso22_tn);
  char tempBuf[4];
  int roundedTemp = (int)round(displayedTemp);
  snprintf(tempBuf, sizeof(tempBuf), "%d", roundedTemp);
  u8g2.drawStr(5, 40, tempBuf);
  int tempW = u8g2.getStrWidth(tempBuf);

  int degX = 5 + tempW + 6;
  u8g2.drawCircle(degX, 20, 3);
  u8g2.drawCircle(degX, 20, 2);

  u8g2.setFont(u8g2_font_helvR08_tf);
  u8g2.drawStr(degX + 7, 22, "C");

  u8g2.setFont(u8g2_font_helvR08_tf);
  u8g2.drawStr(5, 50, currentCondition.c_str());

  // Weather icon — variety depends on condition (see mapOWMCondition)
  if (weatherIcon == WEATHER_ICON_MIST) {
    drawMistIcon(86, 20, false);
  } else {
    u8g2.setFont(u8g2_font_open_iconic_weather_4x_t);
    u8g2.drawGlyph(85, 42, weatherIcon);
  }

  // 3. BOTTOM "PILL" CONTAINER (Stats)
  u8g2.drawRFrame(0, 52, 128, 12, 4);

  u8g2.setFont(u8g2_font_helvR08_tf);
  char humBuf[10];
  snprintf(humBuf, sizeof(humBuf), "H: %d%%", currentHumidity);
  u8g2.drawStr(6, 61, humBuf);

  u8g2.drawDisc(50, 58, 1);

  char windBuf[16];
  snprintf(windBuf, sizeof(windBuf), "W: %.0fm/s %s", currentWindSpeed, currentWindDir.c_str());
  u8g2.drawStr(58, 61, windBuf);
}

// --- Forecast Screen (rounded temps, no decimal point) ---
void drawForecastScreen() {
  u8g2.setFont(u8g2_font_helvB08_tf);
  u8g2.drawStr(30, 10, "3-Day Forecast");
  u8g2.drawLine(10, 13, 118, 13);

  int todayIndex = timeClient.getDay();
  for(int i = 0; i < 3; i++) {
    int yPos = 28 + (i * 16);
    u8g2.setFont(u8g2_font_helvR08_tf);
    String fDay = days[(todayIndex + i + 1) % 7];
    u8g2.drawStr(15, yPos, fDay.c_str());

    if (fc_icon[i] == WEATHER_ICON_MIST) {
      drawMistIcon(55, yPos - 10, true);
    } else {
      u8g2.setFont(u8g2_font_open_iconic_weather_2x_t);
      u8g2.drawGlyph(55, yPos + 4, fc_icon[i]);
    }

    u8g2.setFont(u8g2_font_helvR08_tf);
    int roundedFc = (int)round(fc_temp[i]);
    char fTempBuf[8];
    snprintf(fTempBuf, sizeof(fTempBuf), "%d C", roundedFc); // no decimal point
    u8g2.drawStr(85, yPos, fTempBuf);
  }
}

// --- OpenWeatherMap condition mapping (weather.id ranges) ---
// Uses u8g2_font_open_iconic_weather glyph codes for more variety.
// Reference glyphs in that font: 65=sun,66=moon,67=rain,68=snow,69=clear day/sun-cloud,
// 70=fog/mist,71=wind,72=storm/lightning. (Exact indices per u8g2 open_iconic_weather set.)
void mapOWMCondition(int id, String &cond, uint16_t &icon) {
  if (id == 800) {
    cond = "Clear"; icon = 69; // sun
  } else if (id == 801) {
    cond = "Few Clouds"; icon = 65; // sun peeking
  } else if (id == 802 || id == 803) {
    cond = "P. Cloudy"; icon = 64; // cloud
  } else if (id == 804) {
    cond = "Overcast"; icon = 65; // full cloud
  } else if (id >= 200 && id <= 232) {
    cond = "Thunderstorm"; icon = 66; // lightning bolt icon
  } else if (id >= 300 && id <= 321) {
    cond = "Drizzle"; icon = 67; // light rain
  } else if (id >= 500 && id <= 504) {
    cond = "Rain"; icon = 67; // rain
  } else if (id >= 511 && id <= 531) {
    cond = "Heavy Rain"; icon = 67; // rain
  } else if (id >= 600 && id <= 622) {
    cond = "Snow"; icon = 70; // snow icon
  } else if (id == 701 || id == 711 || id == 721 || id == 741) {
    cond = (id == 741) ? "Fog" : "Mist";
    icon = WEATHER_ICON_MIST; // custom three-line fog symbol
  } else if (id >= 731 && id <= 781) {
    cond = "Windy/Dust"; icon = 72; // wind icon
  } else {
    cond = "Cloudy"; icon = 65;
  }
}

// --- Current weather via OWM /weather ---
void fetchWeather() {
  if (WiFi.status() != WL_CONNECTED) return;

  String url = "http://api.openweathermap.org/data/2.5/weather?lat=" + String(weatherLat, 4) +
               "&lon=" + String(weatherLon, 4) + "&units=metric&appid=" + weatherKey;

  HTTPClient http;
  http.setTimeout(8000);
  http.begin(url);
  int httpCode = http.GET();

  if (httpCode == 200) {
    String payload = http.getString();
    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, payload);
    if (!error) {
      currentTemp = doc["main"]["temp"];
      currentHumidity = doc["main"]["humidity"];
      currentWindSpeed = doc["wind"]["speed"];
      float windDeg = doc["wind"]["deg"];
      currentWindDir = degToCompass(windDeg);

      // OpenWeatherMap returns the offset for the coordinates, including DST where applicable.
      // Retain the last known offset in NVS for offline clock display.
      if (!doc["timezone"].isNull()) {
        int locationOffset = doc["timezone"].as<int>();
        if (locationOffset >= -43200 && locationOffset <= 50400 && locationOffset != utcOffsetSeconds) {
          utcOffsetSeconds = locationOffset;
          timeClient.setTimeOffset(utcOffsetSeconds);
          prefs.putInt("utcOffset", utcOffsetSeconds);
        }
      }
      int wid = doc["weather"][0]["id"];
      mapOWMCondition(wid, currentCondition, weatherIcon);
    }
  }
  http.end();
}

String degToCompass(float deg) {
  const char* dirs[] = {"N","NE","E","SE","S","SW","W","NW"};
  int idx = (int)((deg + 22.5) / 45.0) % 8;
  return String(dirs[idx]);
}

// --- 3-day forecast via OWM /forecast (3-hour steps, pick ~midday each day) ---
void fetchForecast() {
  if (WiFi.status() != WL_CONNECTED) return;

  String url = "http://api.openweathermap.org/data/2.5/forecast?lat=" + String(weatherLat, 4) +
               "&lon=" + String(weatherLon, 4) + "&units=metric&appid=" + weatherKey;

  HTTPClient http;
  http.setTimeout(8000);
  http.begin(url);
  int httpCode = http.GET();

  if (httpCode == 200) {
    String payload = http.getString();
    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, payload);
    if (!error) {
      int idxs[3] = {7, 15, 23};
      for (int i = 0; i < 3; i++) {
        int idx = idxs[i];
        fc_temp[i] = doc["list"][idx]["main"]["temp"];
        int fid = doc["list"][idx]["weather"][0]["id"];
        String dummy;
        mapOWMCondition(fid, dummy, fc_icon[i]);
      }
    }
  }
  http.end();
}


// ---------------- Device-local WebServer dashboard ----------------
const char DASHBOARD_HTML[] PROGMEM = R"DBHTML(<!DOCTYPE html>
<html lang="en"><head><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1"><meta name="theme-color" content="#f7f8f6"><meta name="description" content="Private local control center for Desk-Companion"><title>Desk-Companion — Control Center</title>
<style>
:root{--bg:#f7f8f6;--card:#fff;--text:#1b2422;--sub:#78817e;--line:#e7eae7;--accent:#176953;--accentLight:#edf6f1;--ink:#26352e;--shadow:0 5px 26px rgba(21,36,29,.035)}
*{box-sizing:border-box}html{scroll-behavior:smooth}body{margin:0;background:var(--bg);font:14px/1.5 Inter,-apple-system,BlinkMacSystemFont,"Segoe UI",Arial,sans-serif;color:var(--text);-webkit-font-smoothing:antialiased}button,input,select{font:inherit}button{cursor:pointer}.icon{display:inline-block;vertical-align:middle;flex:none}.layout{display:flex;min-height:100vh}.sidebar{width:246px;background:#fff;border-right:1px solid var(--line);padding:27px 18px;display:flex;flex-direction:column;gap:34px;position:sticky;top:0;height:100vh}.brand{display:flex;align-items:center;gap:12px;padding:0 9px}.mark{height:40px;width:40px;border-radius:12px;background:#e6f0e9;color:var(--accent);display:grid;place-items:center}.brand-name{font-weight:750;font-size:16px;letter-spacing:-.5px;line-height:1.2}.brand-sub{font-size:10px;letter-spacing:1.15px;color:var(--sub);font-weight:700;margin-top:3px}.navgroup{font-size:10px;font-weight:750;color:#9baba2;letter-spacing:1.4px;margin:0 12px 12px;text-transform:uppercase}.navitem{display:flex;align-items:center;gap:12px;text-decoration:none;color:#68736d;padding:12px 13px;border-radius:10px;margin:3px 0;font-size:13px;font-weight:580}.navitem.active{background:var(--accentLight);color:var(--accent);font-weight:750}.navitem:hover{background:#f5f7f4;color:var(--ink)}.sidebar-bottom{margin-top:auto;padding:14px;border-radius:12px;background:#f7f9f6;border:1px solid var(--line);display:flex;gap:10px;color:var(--sub);font-size:11px;line-height:1.6}.main{flex:1;min-width:0;max-width:1450px;padding:32px clamp(20px,4vw,62px) 55px}.top{display:flex;justify-content:space-between;align-items:center;gap:12px;margin-bottom:29px}.page-title{font-size:26px;font-weight:760;letter-spacing:-1.1px;margin:0}.lead{font-size:13px;color:var(--sub);margin:5px 0 0}.badge{display:inline-flex;align-items:center;gap:8px;background:#fff;border:1px solid var(--line);border-radius:100px;padding:9px 13px;color:#376f59;font-size:12px;font-weight:700;white-space:nowrap}.pulse{width:7px;height:7px;border-radius:50%;background:#29a26e;box-shadow:0 0 0 3px #dff5e9}.badge.offline{color:#aa5a44}.badge.offline .pulse{background:#bd6b56;box-shadow:0 0 0 3px #f8e8e2}.hero{background:#e9f0e9;min-height:228px;border-radius:18px;display:flex;align-items:center;justify-content:space-between;padding:28px 39px;gap:25px;border:1px solid #e0e9e0;overflow:hidden}.eyebrow{font-size:10px;letter-spacing:1.8px;color:#438068;font-weight:800}.hero h2{font-size:30px;letter-spacing:-1.1px;line-height:1.15;margin:9px 0 8px;font-weight:800}.hero p{color:#6d8075;margin:0;max-width:340px;font-size:13px}.hero-face{width:170px;height:137px;background:#263f35;border:6px solid #547163;border-radius:27px;position:relative;flex:none;box-shadow:0 13px 30px #254a3326;display:flex;align-items:center;justify-content:center;flex-direction:column;gap:22px;transform:rotate(-5deg)}.hero-face:before,.hero-face:after{content:"";height:15px;width:4px;background:#547163;position:absolute;top:-19px;border-radius:4px}.hero-face:before{left:51px;transform:rotate(-24deg)}.hero-face:after{right:50px;transform:rotate(24deg)}.face-eyes{display:flex;gap:36px}.face-eyes span{height:32px;width:23px;border-radius:20px;background:#c7f7d6;box-shadow:0 0 13px #bdfad44a}.face-mouth{height:15px;width:39px;border:0;border-bottom:4px solid #c7f7d6;border-radius:0 0 50% 50%}.metrics{display:grid;grid-template-columns:repeat(3,minmax(0,1fr));gap:15px;margin:18px 0 25px}.metric{background:#fff;border:1px solid var(--line);border-radius:14px;padding:17px 20px;display:flex;gap:15px;align-items:center}.metric-icon{width:43px;height:43px;border-radius:12px;background:#f1f5f1;color:var(--accent);display:grid;place-items:center}.metric-label{font-size:11px;color:var(--sub);margin-bottom:2px}.metric-value{font-size:17px;font-weight:770;letter-spacing:-.4px;overflow-wrap:anywhere}.two{display:grid;grid-template-columns:1.12fr 1fr;gap:18px;align-items:start}.card{background:var(--card);border:1px solid var(--line);border-radius:16px;padding:23px 24px;box-shadow:var(--shadow);min-width:0}.card h3{margin:0;font-weight:760;font-size:16px;letter-spacing:-.3px}.cardhead{display:flex;align-items:center;justify-content:space-between;margin-bottom:18px;gap:10px}.cardhead .icon{color:#6b8175}.card-sub{font-size:12px;color:var(--sub);margin:5px 0 20px}.screenBtns{display:grid;grid-template-columns:repeat(2,1fr);gap:10px}.screenBtns button{display:flex;gap:11px;align-items:center;text-align:left;border:1px solid var(--line);border-radius:11px;background:#fff;padding:17px 14px;color:#54605a;font-size:12px;font-weight:650;min-height:61px}.screenBtns button.active{background:var(--accentLight);color:var(--accent);border-color:#a8d5ba}.screenBtns button:hover{border-color:#86b69d}.control-row{display:flex;justify-content:space-between;align-items:center;gap:10px;padding:13px 0;border-bottom:1px solid var(--line)}.control-row span:first-child{color:var(--sub);font-size:12px}.control-row strong{font-size:12px;text-align:right;overflow-wrap:anywhere}label{display:block;color:#5a6960;font-size:12px;font-weight:680;margin-bottom:7px}input,select{display:block;width:100%;border:1px solid #dfe5df;border-radius:9px;background:#fff;color:#27372e;padding:12px 13px;outline:none;min-width:0}input:focus,select:focus{border-color:#5d9c7f;box-shadow:0 0 0 3px #e7f5eb}input::placeholder{color:#a2aaa4}.field{margin:0 0 15px}.form-grid{display:grid;grid-template-columns:1fr 1fr;gap:12px}.btn{display:inline-flex;align-items:center;justify-content:center;gap:8px;border:1px solid #dce4dc;background:#fff;color:#40554a;border-radius:9px;padding:11px 15px;font-size:12px;font-weight:740}.btn:hover{background:#f5f8f5}.btn.primary{background:var(--accent);border-color:var(--accent);color:white}.btn.primary:hover{background:#10523f}.btn.danger{color:#ad554f;border-color:#f0ddda}.btn.danger:hover{background:#fff7f6}.actions{display:flex;flex-wrap:wrap;gap:9px;margin-top:19px}.hint{font-size:11px;color:#8b968e;line-height:1.65;margin:14px 0 0}.section-title{font-size:17px;font-weight:780;letter-spacing:-.4px;margin:28px 0 13px}.config{display:grid;grid-template-columns:1fr 1fr;gap:18px}.config .card{min-width:0}.notice{min-height:20px;font-size:12px;color:var(--accent);margin:18px 0}footer{padding-top:16px;border-top:1px solid var(--line);color:#99a39b;font-size:11px;display:flex;justify-content:space-between;gap:10px}@media(max-width:1080px){.sidebar{width:210px}.main{padding:28px 24px 48px}.hero{padding:26px}.metrics{gap:9px}.metric{padding:15px 12px;gap:10px}}@media(max-width:790px){.sidebar{display:none}.main{padding:24px 18px 44px}.two,.config{grid-template-columns:1fr}.hero{padding:25px}.metrics{grid-template-columns:repeat(3,minmax(0,1fr))}}@media(max-width:530px){.top{align-items:flex-start}.page-title{font-size:23px}.hero{min-height:180px;padding:20px}.hero h2{font-size:25px}.hero p{font-size:12px}.hero-face{width:96px;height:92px;border-width:4px;border-radius:18px;gap:13px}.face-eyes{gap:20px}.face-eyes span{width:13px;height:20px}.face-mouth{width:23px;height:10px;border-bottom-width:3px}.hero-face:before,.hero-face:after{display:none}.metrics{grid-template-columns:1fr;gap:9px}.metric{padding:13px 15px}.card{padding:20px 17px}.form-grid{grid-template-columns:1fr}footer{display:block}}
.scanhead{display:flex;align-items:center;justify-content:space-between;gap:12px}.scanhead label{margin:0}.scanbtn{padding:9px 13px!important;white-space:nowrap}.scanresults{display:none;margin:12px 0 2px;max-height:245px;overflow:auto;border:1px solid var(--line);background:#fbfcfb;border-radius:12px}.scanresults.visible{display:block}.scanitem{width:100%;border:0;border-bottom:1px solid var(--line);background:transparent;display:flex;align-items:center;justify-content:space-between;gap:14px;text-align:left;padding:12px 14px;color:var(--text);font-size:13px}.scanitem:last-child{border-bottom:0}.scanitem:hover,.scanitem:focus-visible{background:var(--accentLight);outline:0}.scanmeta{font-size:11px;color:var(--sub);white-space:nowrap}.scanhint{padding:13px;color:var(--sub);font-size:12px}

/* Native-feeling, accessible in-app dialogs and notifications. */
.dialog-backdrop{position:fixed;inset:0;z-index:1000;display:flex;justify-content:center;align-items:center;padding:18px;background:rgba(20,37,31,.42);backdrop-filter:blur(7px);opacity:0;visibility:hidden;transition:opacity .18s ease,visibility .18s ease}
.dialog-backdrop.open{opacity:1;visibility:visible}.dialog-panel{width:min(100%,430px);background:#fff;border:1px solid #e7ebe7;border-radius:20px;padding:27px;box-shadow:0 25px 90px rgba(23,38,30,.20);transform:translateY(12px) scale(.98);transition:transform .2s ease}.dialog-backdrop.open .dialog-panel{transform:translateY(0) scale(1)}
.dialog-icon{display:grid;place-items:center;width:46px;height:46px;border-radius:14px;color:#2a6b54;background:#eaf3ed;margin-bottom:18px}.dialog-icon.danger{color:#a55234;background:#fcf0e9}
.dialog-panel h2{font-size:21px;letter-spacing:-.6px;margin:0 0 9px;color:#182c23}.dialog-panel p{font-size:13px;line-height:1.7;color:#69796f;margin:0 0 24px}.dialog-actions{display:flex;justify-content:flex-end;gap:10px;flex-wrap:wrap}.dialog-actions .btn{min-width:96px;justify-content:center}.btn.destructive{background:#a64e38;border-color:#a64e38;color:white}.btn.destructive:hover{background:#8b3d2a}
.toast-stack{position:fixed;top:20px;right:20px;z-index:1100;display:flex;flex-direction:column;gap:10px;width:min(375px,calc(100vw - 36px));pointer-events:none}.app-toast{display:flex;align-items:start;gap:12px;padding:14px 15px;background:#fff;border:1px solid #e0e8e1;border-radius:15px;box-shadow:0 10px 45px rgba(24,47,36,.13);pointer-events:auto;animation:toast-in .22s ease}.app-toast.error{border-color:#f0d3d0}.app-toast .toast-symbol{flex:none;color:#297252;margin-top:2px}.app-toast.error .toast-symbol{color:#bb4a42}.app-toast .toast-body{flex:1;min-width:0}.app-toast strong{display:block;font-size:13px;color:#22372c;margin:0 0 3px}.app-toast p{margin:0;font-size:12px;line-height:1.55;color:#6a7a70;overflow-wrap:anywhere}.app-toast button{padding:2px;border:0;background:none;color:#87948b;cursor:pointer}.app-toast svg{width:18px;height:18px;display:block}
@keyframes toast-in{from{opacity:0;transform:translateY(-8px)}to{opacity:1;transform:translateY(0)}}@media(max-width:600px){.toast-stack{top:auto;bottom:18px;right:18px}.dialog-panel{padding:23px}}
@media(prefers-reduced-motion:reduce){.dialog-backdrop,.dialog-panel,.app-toast{transition:none;animation:none}}

/* Searchable, fully custom emotion selector (no native browser dropdown). */
.emotion-picker{position:relative}.emotion-search-box{display:flex;align-items:center;gap:9px;background:#fff;border:1px solid #dfe5df;border-radius:10px;padding:0 10px;color:#74847a;transition:box-shadow .15s,border-color .15s}.emotion-search-box:focus-within{border-color:#5d9c7f;box-shadow:0 0 0 3px #e7f5eb}.emotion-search-box input{padding:12px 0;flex:1;min-width:0;border:0;box-shadow:none!important;background:transparent}.emotion-clear{display:grid;place-items:center;border:0;background:transparent;color:#829087;padding:5px;border-radius:6px}.emotion-clear:hover{color:#245f48;background:#eef4ef}.emotion-options{display:none;position:absolute;left:0;right:0;top:calc(100% - 23px);z-index:25;border:1px solid #dce7de;border-radius:13px;padding:6px;background:#fff;box-shadow:0 16px 44px rgba(21,36,29,.14);max-height:244px;overflow:auto}.emotion-picker.open .emotion-options{display:block}.emotion-option{width:100%;display:flex;align-items:center;justify-content:space-between;text-align:left;padding:10px 12px;background:transparent;border:0;border-radius:8px;color:#34483a;font-size:13px}.emotion-option:hover,.emotion-option.focused{background:#edf6f1;color:#176953}.emotion-option.selected{font-weight:750}.emotion-option svg{color:#176953}.emotion-empty{padding:18px 12px;color:#87948a;font-size:12px;text-align:center}.emotion-current{color:#78887d;font-size:11px;padding-top:8px}
</style></head><body><div class="layout">
<aside class="sidebar"><div class="brand"><div class="mark"><svg width="22" height="22" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="1.8" stroke-linecap="round" stroke-linejoin="round" aria-hidden="true"><path d="M8 3h8l4 4v10l-4 4H8l-4-4V7z"/><path d="M9 10h.01M15 10h.01" stroke-width="3"/><path d="M8.5 15c2.2 2 4.8 2 7 0"/></svg></div><div><div class="brand-name">Desk-Companion</div><div class="brand-sub">CONTROL CENTER</div></div></div><nav aria-label="Main navigation"><div class="navgroup">Workspace</div><a class="navitem active" href="#overview"><svg class="icon" width="18" height="18" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="1.75" stroke-linecap="round" stroke-linejoin="round" aria-hidden="true"><path d="M3 12h4l3-7 4 14 3-7h4"/></svg> Overview</a><a class="navitem" href="#display"><svg class="icon" width="18" height="18" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="1.75" stroke-linecap="round" stroke-linejoin="round" aria-hidden="true"><rect x="3" y="4" width="18" height="14" rx="2"/><path d="M8 22h8M12 18v4"/></svg> Display controls</a><a class="navitem" href="#device"><svg class="icon" width="18" height="18" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="1.75" stroke-linecap="round" stroke-linejoin="round" aria-hidden="true"><rect x="5" y="5" width="14" height="14" rx="2"/><path d="M9 9h6v6H9zM9 1v4M15 1v4M9 19v4M15 19v4M1 9h4M1 15h4M19 9h4M19 15h4"/></svg> Device status</a><div class="navgroup" style="margin-top:29px">Configuration</div><a class="navitem" href="#network"><svg class="icon" width="18" height="18" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="1.75" stroke-linecap="round" stroke-linejoin="round" aria-hidden="true"><path d="M2 8.8a16 16 0 0 1 20 0M5 12.5a11 11 0 0 1 14 0M8.5 16a6 6 0 0 1 7 0"/><circle cx="12" cy="20" r="1" fill="currentColor" stroke="none"/></svg> Wi-Fi network</a><a class="navitem" href="#weather"><svg class="icon" width="18" height="18" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="1.75" stroke-linecap="round" stroke-linejoin="round" aria-hidden="true"><path d="M6 19a4 4 0 0 1-1-7.9A7 7 0 0 1 18 10a4.5 4.5 0 0 1 0 9H6z"/></svg> Weather & time</a></nav><div class="sidebar-bottom"><svg class="icon" width="17" height="17" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="1.75" stroke-linecap="round" stroke-linejoin="round" aria-hidden="true"><rect x="5" y="10" width="14" height="11" rx="2"/><path d="M8 10V7a4 4 0 0 1 8 0v3"/></svg><span>Local-first control.<br>No cloud login required.</span></div></aside>
<main class="main" id="overview"><div class="top"><div><h1 class="page-title">Overview</h1><p class="lead">Manage your Desk-Companion, all in one place.</p></div><div class="badge offline" id="connectionBadge"><span class="pulse"></span><span id="conn">Connecting</span></div></div>
<div class="hero"><div><div class="eyebrow">MEET YOUR COMPANION</div><h2>Hello, human.</h2><p>Your pocket-sized desk companion is ready whenever you are.</p></div><div class="hero-face" role="img" aria-label="Smiling Desk-Companion illustration"><div class="face-eyes"><span></span><span></span></div><div class="face-mouth"></div></div></div>
<div class="metrics"><div class="metric"><div class="metric-icon"><svg class="icon" width="22" height="22" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="1.75" stroke-linecap="round" stroke-linejoin="round" aria-hidden="true"><path d="M2 8.8a16 16 0 0 1 20 0M5 12.5a11 11 0 0 1 14 0M8.5 16a6 6 0 0 1 7 0"/><circle cx="12" cy="20" r="1" fill="currentColor" stroke="none"/></svg></div><div><div class="metric-label">Network</div><div class="metric-value" id="wifilabel">—</div></div></div><div class="metric"><div class="metric-icon"><svg class="icon" width="22" height="22" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="1.75" stroke-linecap="round" stroke-linejoin="round" aria-hidden="true"><path d="M10 3a2 2 0 0 1 4 0v10.5a4 4 0 1 1-4 0V3z"/><path d="M12 9v9"/></svg></div><div><div class="metric-label">ESP32-S3 chip temperature</div><div class="metric-value" id="temperature">—</div></div></div><div class="metric"><div class="metric-icon"><svg class="icon" width="22" height="22" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="1.75" stroke-linecap="round" stroke-linejoin="round" aria-hidden="true"><circle cx="12" cy="12" r="9"/><path d="M12 7v5l3 2"/></svg></div><div><div class="metric-label">Uptime</div><div class="metric-value" id="uptime">—</div></div></div></div>
<div class="two"><section class="card" id="display"><div class="cardhead"><h3>Display controls</h3><svg class="icon" width="18" height="18" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="1.75" stroke-linecap="round" stroke-linejoin="round" aria-hidden="true"><rect x="3" y="4" width="18" height="14" rx="2"/><path d="M8 22h8M12 18v4"/></svg></div><p class="card-sub">Choose what appears on the OLED display.</p><div class="screenBtns" id="screens"><button data-screen="0"><svg class="icon" width="18" height="18" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="1.75" stroke-linecap="round" stroke-linejoin="round" aria-hidden="true"><circle cx="12" cy="12" r="9"/><path d="M8 14c2 3 6 3 8 0M9 9h.01M15 9h.01" stroke-width="2.2"/></svg> Emotions</button><button data-screen="1"><svg class="icon" width="18" height="18" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="1.75" stroke-linecap="round" stroke-linejoin="round" aria-hidden="true"><circle cx="12" cy="12" r="9"/><path d="M12 7v5l3 2"/></svg> Clock</button><button data-screen="2"><svg class="icon" width="18" height="18" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="1.75" stroke-linecap="round" stroke-linejoin="round" aria-hidden="true"><path d="M6 19a4 4 0 0 1-1-7.9A7 7 0 0 1 18 10a4.5 4.5 0 0 1 0 9H6z"/></svg> Weather</button><button data-screen="3"><svg class="icon" width="18" height="18" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="1.75" stroke-linecap="round" stroke-linejoin="round" aria-hidden="true"><rect x="3" y="5" width="18" height="16" rx="2"/><path d="M7 3v4M17 3v4M3 10h18"/></svg> Forecast</button></div><div class="field" style="margin-top:22px"><label for="emotion">Emotion mode</label><div class="emotion-picker" id="emotionPicker">
  <div class="emotion-search-box"><svg class="icon" width="18" height="18" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="1.8" stroke-linecap="round" stroke-linejoin="round" aria-hidden="true"><circle cx="11" cy="11" r="7"/><path d="m16 16 4 4"/></svg><input id="emotionSearch" type="text" role="combobox" aria-controls="emotionOptions" aria-expanded="false" aria-autocomplete="list" autocomplete="off" placeholder="Search 66 emotions…"/><button class="emotion-clear" id="emotionClear" type="button" aria-label="Clear emotion search"><svg width="15" height="15" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round"><path d="M18 6 6 18M6 6l12 12"/></svg></button></div>
  <div class="emotion-options" id="emotionOptions" role="listbox" aria-label="Choose an emotion"></div>
  <div class="emotion-current" id="emotionCurrent">Mode: Automatic — all 66 emotions</div>
</div></div><div class="actions"><button class="btn" id="next">Next screen <svg class="icon" width="16" height="16" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="1.75" stroke-linecap="round" stroke-linejoin="round" aria-hidden="true"><path d="M5 12h14m-6-6 6 6-6 6"/></svg></button><button class="btn danger" id="sleep"><svg class="icon" width="16" height="16" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="1.75" stroke-linecap="round" stroke-linejoin="round" aria-hidden="true"><path d="M20 15.4A8.5 8.5 0 0 1 8.6 4 8.5 8.5 0 1 0 20 15.4Z"/></svg> Sleep</button></div><p class="hint">A long touch puts Desk-Companion to sleep. Touch again to wake.</p></section>
<section class="card" id="device"><div class="cardhead"><h3>Device information</h3><svg class="icon" width="18" height="18" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="1.75" stroke-linecap="round" stroke-linejoin="round" aria-hidden="true"><rect x="5" y="5" width="14" height="14" rx="2"/><path d="M9 9h6v6H9zM9 1v4M15 1v4M9 19v4M15 19v4M1 9h4M1 15h4M19 9h4M19 15h4"/></svg></div><p class="card-sub">Current status reported by your ESP32-S3.</p><div class="control-row"><span>Hotspot name</span><strong id="ap">—</strong></div><div class="control-row"><span>Local IP address</span><strong id="ip">—</strong></div><div class="control-row"><span>Active emotion</span><strong id="emo">—</strong></div><div class="actions"><button class="btn" id="refresh"><svg class="icon" width="16" height="16" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="1.75" stroke-linecap="round" stroke-linejoin="round" aria-hidden="true"><path d="M20 11a8 8 0 0 0-14-5L4 8M4 3v5h5M4 13a8 8 0 0 0 14 5l2-2M20 21v-5h-5"/></svg> Refresh</button><button class="btn danger" id="reboot"><svg class="icon" width="16" height="16" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="1.75" stroke-linecap="round" stroke-linejoin="round" aria-hidden="true"><path d="M12 2v10M6 5a9 9 0 1 0 12 0"/></svg> Restart</button></div><p class="hint">The local hotspot remains available for direct access.</p></section></div>
<h2 class="section-title">Configuration</h2><div class="config"><section class="card" id="network"><div class="cardhead"><h3>Wi-Fi network</h3><svg class="icon" width="18" height="18" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="1.75" stroke-linecap="round" stroke-linejoin="round" aria-hidden="true"><path d="M2 8.8a16 16 0 0 1 20 0M5 12.5a11 11 0 0 1 14 0M8.5 16a6 6 0 0 1 7 0"/><circle cx="12" cy="20" r="1" fill="currentColor" stroke="none"/></svg></div><p class="card-sub">Connect Desk-Companion to your 2.4 GHz network.</p><form id="wifiForm"><div class="field"><div class="scanhead"><label for="ssid">Network name (SSID)</label><button class="btn scanbtn" id="scanWifi" type="button"><svg class="icon" width="16" height="16" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="1.75" stroke-linecap="round" stroke-linejoin="round" aria-hidden="true"><path d="M2 8.8a16 16 0 0 1 20 0M5 12.5a11 11 0 0 1 14 0M8.5 16a6 6 0 0 1 7 0"/><circle cx="12" cy="20" r="1" fill="currentColor" stroke="none"/></svg> Scan Wi-Fi</button></div><div id="scanResults" class="scanresults" aria-live="polite"></div></div><div class="field"><label for="ssid">Network name (SSID)</label><input id="ssid" name="ssid" maxlength="32" autocomplete="off" placeholder="Enter network name" required></div><div class="field"><label for="password">Wi-Fi password</label><input id="password" name="password" type="password" maxlength="63" autocomplete="new-password" placeholder="Leave blank to keep existing"></div><button class="btn primary" type="submit"><svg class="icon" width="16" height="16" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="1.75" stroke-linecap="round" stroke-linejoin="round" aria-hidden="true"><path d="M4 3h13l3 3v15H4zM8 3v6h8V3M8 21v-8h8v8"/></svg> Save network</button></form><p class="hint">Credentials are stored on your device, never inside the shared source code.</p></section>
<section class="card" id="network2"><div class="cardhead"><h3>Secondary Wi-Fi network</h3><svg class="icon" width="18" height="18" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="1.75" stroke-linecap="round" stroke-linejoin="round" aria-hidden="true"><path d="M2 8.8a16 16 0 0 1 20 0M5 12.5a11 11 0 0 1 14 0M8.5 16a6 6 0 0 1 7 0"/><circle cx="12" cy="20" r="1" fill="currentColor" stroke="none"/></svg></div><p class="card-sub">Backup 2.4 GHz network. Desk-Companion switches to it if the first network is unavailable.</p><form id="wifiForm2"><div class="field"><div class="scanhead"><label for="ssid2">Network name (SSID)</label><button class="btn scanbtn" id="scanWifi2" type="button"><svg class="icon" width="16" height="16" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="1.75" stroke-linecap="round" stroke-linejoin="round" aria-hidden="true"><path d="M2 8.8a16 16 0 0 1 20 0M5 12.5a11 11 0 0 1 14 0M8.5 16a6 6 0 0 1 7 0"/><circle cx="12" cy="20" r="1" fill="currentColor" stroke="none"/></svg> Scan Wi-Fi</button></div><div id="scanResults2" class="scanresults" aria-live="polite"></div></div><div class="field"><label for="ssid2">Network name (SSID)</label><input id="ssid2" name="ssid2" maxlength="32" autocomplete="off" placeholder="Enter network name" required></div><div class="field"><label for="password2">Wi-Fi password</label><input id="password2" name="password2" type="password" maxlength="63" autocomplete="new-password" placeholder="Leave blank to keep existing"></div><button class="btn primary" type="submit"><svg class="icon" width="16" height="16" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="1.75" stroke-linecap="round" stroke-linejoin="round" aria-hidden="true"><path d="M4 3h13l3 3v15H4zM8 3v6h8V3M8 21v-8h8v8"/></svg> Save backup network</button></form><p class="hint">Backup credentials are stored locally, not hardcoded in the firmware.</p></section>
<section class="card" id="weather"><div class="cardhead"><h3>Weather & time</h3><svg class="icon" width="18" height="18" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="1.75" stroke-linecap="round" stroke-linejoin="round" aria-hidden="true"><path d="M6 19a4 4 0 0 1-1-7.9A7 7 0 0 1 18 10a4.5 4.5 0 0 1 0 9H6z"/></svg></div><p class="card-sub">Enter your city, state and country. Desk-Companion finds the location and synchronizes its local time automatically after connecting to the internet.</p><form id="weatherForm"><div class="field"><label for="city">City</label><input id="city" name="city" maxlength="32" placeholder="e.g. Kochi" required></div><div class="form-grid"><div class="field"><label for="state">State / Province</label><input id="state" name="state" maxlength="64" placeholder="e.g. Kerala" required></div><div class="field"><label for="country">Country</label><input id="country" name="country" maxlength="64" placeholder="e.g. India" required></div></div><div class="field"><label for="apiKey">OpenWeatherMap API key</label><input id="apiKey" name="apiKey" type="password" autocomplete="new-password" placeholder="Leave blank to keep existing"></div><div class="field"><label>Location time zone</label><div class="control-row"><span>Detected from your configured location</span><strong id="timeZoneStatus">Awaiting sync</strong></div></div><button class="btn primary" type="submit"><svg class="icon" width="16" height="16" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="1.75" stroke-linecap="round" stroke-linejoin="round" aria-hidden="true"><path d="M4 3h13l3 3v15H4zM8 3v6h8V3M8 21v-8h8v8"/></svg> Save weather settings</button></form><p class="hint">Private keys are not returned by the device status API.</p></section></div>
<div id="message" class="notice" role="status" aria-live="polite"></div><footer><span>Desk-Companion Control Center</span><span>Local dashboard · ESP32-S3</span></footer></main></div>
<div class="toast-stack" id="toastStack" role="status" aria-live="polite" aria-atomic="false"></div>
<div class="dialog-backdrop" id="customDialog" aria-hidden="true">
  <div class="dialog-panel" role="dialog" aria-modal="true" aria-labelledby="dialogTitle" aria-describedby="dialogDescription">
    <div class="dialog-icon" id="dialogIcon"><svg width="23" height="23" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="1.8" stroke-linecap="round" stroke-linejoin="round" aria-hidden="true"><path d="M12 3a9 9 0 1 0 0 18 9 9 0 0 0 0-18z"/><path d="M12 7v6M12 17h.01"/></svg></div>
    <h2 id="dialogTitle">Confirm action</h2><p id="dialogDescription"></p>
    <div class="dialog-actions"><button class="btn" id="dialogCancel" type="button">Cancel</button><button class="btn primary" id="dialogAccept" type="button">Confirm</button></div>
  </div>
</div>
<script>
const $=id=>document.getElementById(id);let filled=false;let state={};const emotions='Neutral,Happy,Sad,Angry,Surprised,Sleepy,Love,Wink,Confused,Scared,Bored,Excited,Crying,Laughing,Suspicious,Dizzy,Sick,Cool,Shocked,Annoyed,Curious,Thinking,Embarrassed,Proud,Worried,Blink,Jealous,Grumpy,Mischievous,Shy,Nervous,Ecstatic,Frustrated,Hopeful,Grateful,Nostalgic,Determined,Playful,Sarcastic,Smug,Guilty,Relieved,Anxious,Content,Irritated,Amused,Skeptical,Horrified,Peaceful,Starstruck,Drowsy,Puzzled,Devious,Humble,Optimistic,Pessimistic,Focused,Daydreaming,Miserable,Cheeky,Alert,Calm,Furious,Tearful,Hysterical,Smitten'.split(',');let chosenEmotion=-1,emotionFocus=-1;
const emotionChoices=[{name:'Automatic — all 66 emotions',id:-1},...emotions.map((name,id)=>({name,id}))];
const checkSvg='<svg width="16" height="16" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" stroke-linejoin="round"><path d="m5 12 4 4L19 6"/></svg>';
function closeEmotionPicker(){ $('emotionPicker').classList.remove('open');$('emotionSearch').setAttribute('aria-expanded','false');emotionFocus=-1; }
function renderEmotions(){
  const box=$('emotionOptions');box.replaceChildren();const query=$('emotionSearch').value.trim().toLowerCase();
  const results=emotionChoices.filter(x=>x.name.toLowerCase().includes(query));
  emotionFocus=results.length?Math.min(Math.max(emotionFocus,0),results.length-1):-1;
  if(!results.length){const empty=document.createElement('div');empty.className='emotion-empty';empty.textContent='No matching emotions';box.append(empty);return;}
  results.forEach((item,index)=>{
    const button=document.createElement('button');button.type='button';button.setAttribute('role','option');button.setAttribute('aria-selected',String(item.id===chosenEmotion));button.className='emotion-option'+(item.id===chosenEmotion?' selected':'')+(index===emotionFocus?' focused':'');
    const name=document.createElement('span');name.textContent=item.name;button.append(name);
    if(item.id===chosenEmotion)button.insertAdjacentHTML('beforeend',checkSvg);
    button.addEventListener('mousedown',e=>e.preventDefault());button.addEventListener('click',()=>chooseEmotion(item.id));box.append(button);
  });
}
function openEmotionPicker(){ $('emotionPicker').classList.add('open');$('emotionSearch').setAttribute('aria-expanded','true');renderEmotions(); }
function chooseEmotion(id){
  chosenEmotion=id; $('emotionCurrent').textContent='Mode: '+(id<0?'Automatic — all 66 emotions':emotions[id]);
  $('emotionSearch').value='';closeEmotionPicker();call('/api/emotion',{emotion:id});
}
$('emotionSearch').addEventListener('focus',openEmotionPicker);
$('emotionSearch').addEventListener('click',openEmotionPicker);
$('emotionSearch').addEventListener('input',()=>{emotionFocus=0;openEmotionPicker()});
$('emotionClear').addEventListener('click',()=>{$('emotionSearch').value='';$('emotionSearch').focus();openEmotionPicker()});
$('emotionSearch').addEventListener('keydown',e=>{
  if(e.key==='Escape'){e.preventDefault();closeEmotionPicker();$('emotionSearch').blur();return;}
  if(e.key==='ArrowDown'||e.key==='ArrowUp'){
    e.preventDefault();if(!$('emotionPicker').classList.contains('open'))openEmotionPicker();
    const count=$('emotionOptions').querySelectorAll('.emotion-option').length;
    if(count){emotionFocus=(emotionFocus+(e.key==='ArrowDown'?1:-1)+count)%count;renderEmotions();$('emotionOptions').querySelectorAll('.emotion-option')[emotionFocus]?.scrollIntoView({block:'nearest'});}return;
  }
  if(e.key==='Enter'&&$('emotionPicker').classList.contains('open')){e.preventDefault();$('emotionOptions').querySelectorAll('.emotion-option')[Math.max(emotionFocus,0)]?.click();}
});
document.addEventListener('click',e=>{if(!$('emotionPicker').contains(e.target))closeEmotionPicker()});;
const ICON_CHECK='<svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" stroke-linejoin="round"><path d="M20 6 9 17l-5-5"/></svg>';
const ICON_ERROR='<svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" stroke-linejoin="round"><circle cx="12" cy="12" r="9"/><path d="M12 7v6M12 17h.01"/></svg>';
const ICON_CLOSE='<svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="1.8" stroke-linecap="round"><path d="M18 6 6 18M6 6l12 12"/></svg>';
function msg(t,bad=false){
  // Also maintain the inline status for screen readers and as a non-popup fallback.
  $('message').textContent=t;$('message').style.color=bad?'#be123c':'#047857';
  const stack=$('toastStack'), toast=document.createElement('div');
  toast.className='app-toast'+(bad?' error':'');
  const symbol=document.createElement('span');symbol.className='toast-symbol';symbol.innerHTML=bad?ICON_ERROR:ICON_CHECK;
  const content=document.createElement('div');content.className='toast-body';
  const title=document.createElement('strong');title.textContent=bad?'Something went wrong':'Desk-Companion';
  const detail=document.createElement('p');detail.textContent=t;content.append(title,detail);
  const close=document.createElement('button');close.type='button';close.setAttribute('aria-label','Dismiss notification');close.innerHTML=ICON_CLOSE;close.onclick=()=>toast.remove();
  toast.append(symbol,content,close);stack.prepend(toast);
  while(stack.childElementCount>3)stack.lastElementChild.remove();
  setTimeout(()=>toast.remove(),bad?6500:4000);
}
let dialogResolver=null,dialogLastFocus=null;
function closeDialog(accepted){
  const overlay=$('customDialog');overlay.classList.remove('open');overlay.setAttribute('aria-hidden','true');
  document.body.style.overflow='';
  if(dialogLastFocus && dialogLastFocus.isConnected)dialogLastFocus.focus();
  const finish=dialogResolver;dialogResolver=null;if(finish)finish(accepted);
}
function confirmAction(title,description,actionLabel='Continue',danger=false){
  if(dialogResolver)closeDialog(false);
  return new Promise(resolve=>{
    dialogResolver=resolve;dialogLastFocus=document.activeElement;
    $('dialogTitle').textContent=title;$('dialogDescription').textContent=description;
    $('dialogAccept').textContent=actionLabel;
    $('dialogAccept').className='btn '+(danger?'destructive':'primary');
    $('dialogIcon').classList.toggle('danger',danger);
    const overlay=$('customDialog');overlay.classList.add('open');overlay.setAttribute('aria-hidden','false');
    document.body.style.overflow='hidden';$('dialogCancel').focus();
  });
}
$('dialogCancel').onclick=()=>closeDialog(false);
$('dialogAccept').onclick=()=>closeDialog(true);
$('customDialog').addEventListener('click',e=>{if(e.target===$('customDialog'))closeDialog(false)});
document.addEventListener('keydown',e=>{
  const open=$('customDialog').classList.contains('open');if(!open)return;
  if(e.key==='Escape'){e.preventDefault();closeDialog(false)}
  if(e.key==='Tab'){
    const first=$('dialogCancel'),last=$('dialogAccept');
    if(e.shiftKey&&document.activeElement===first){e.preventDefault();last.focus()}
    else if(!e.shiftKey&&document.activeElement===last){e.preventDefault();first.focus()}
  }
});
async function request(path,data){const r=await fetch(path,{method:'POST',headers:{'Content-Type':'application/x-www-form-urlencoded'},body:new URLSearchParams(data)});const o=await r.json();if(!r.ok)throw Error(o.error||'Request failed');msg(o.message||'Saved!');return o}
let statusBusy=false;async function status(){if(statusBusy)return;statusBusy=true;const started=performance.now();const controller=new AbortController();const timeout=setTimeout(()=>controller.abort(),3000);try{const r=await fetch('/api/status',{cache:'no-store',signal:controller.signal});if(!r.ok)throw Error('Not connected');state=await r.json();$('conn').textContent='Online · '+Math.round(performance.now()-started)+' ms';$('connectionBadge').classList.remove('offline');$('wifilabel').textContent=state.wifiConnected?state.ssid:'Offline / AP';$('temperature').textContent=Number.isFinite(state.cpuTempC)?state.cpuTempC.toFixed(1)+'°C':'Unavailable';$('timeZoneStatus').textContent=state.weatherReady?'UTC '+(state.offset>=0?'+':'')+(state.offset/60).toFixed(2)+' h':'Awaiting weather sync';$('uptime').textContent=Math.floor(state.uptime/3600)+'h '+(Math.floor(state.uptime/60)%60)+'m';$('ap').textContent=state.ap;$('ip').textContent=state.ip;$('emo').textContent=emotions[state.emotion]||'—';document.querySelectorAll('#screens button').forEach(b=>b.classList.toggle('active',+b.dataset.screen===state.screen));chosenEmotion=state.manualEmotion?state.emotion:-1;$('emotionCurrent').textContent='Mode: '+(chosenEmotion<0?'Automatic — all 66 emotions':emotions[chosenEmotion]||'Unknown');if($('emotionPicker').classList.contains('open'))renderEmotions();if(!filled){$('ssid').value=state.savedSSID||'';$('ssid2').value=state.savedSSID2||'';$('city').value=state.city||'';$('state').value=state.state||'';$('country').value=state.country||'';filled=true}}catch(e){$('conn').textContent='Offline';$('connectionBadge').classList.add('offline')}finally{clearTimeout(timeout);statusBusy=false}}
async function call(path,data){try{await request(path,data);await status()}catch(e){msg(e.message,true)}}
document.querySelectorAll('#screens button').forEach(b=>b.addEventListener('click',()=>call('/api/screen',{screen:b.dataset.screen})));$('next').onclick=()=>call('/api/screen',{screen:(state.screen+1)%4});$('refresh').onclick=status;$('sleep').onclick=async()=>{if(await confirmAction('Put Desk-Companion to sleep?','The display and web server will stop until you touch the sensor to wake it.','Sleep now'))await call('/api/sleep',{})};$('reboot').onclick=async()=>{if(await confirmAction('Restart Desk-Companion?','The dashboard will disconnect briefly while your device restarts.','Restart device',true))await call('/api/restart',{})};$('wifiForm').onsubmit=e=>{e.preventDefault();call('/api/wifi',{ssid:$('ssid').value,password:$('password').value})};$('wifiForm2').onsubmit=e=>{e.preventDefault();call('/api/wifi2',{ssid:$('ssid2').value,password:$('password2').value})};$('weatherForm').onsubmit=e=>{e.preventDefault();call('/api/weather',{city:$('city').value,state:$('state').value,country:$('country').value,apiKey:$('apiKey').value})};
async function scanWiFi(second=false){const btn=$(second?'scanWifi2':'scanWifi'),box=$(second?'scanResults2':'scanResults');btn.disabled=true;btn.textContent='Scanning…';box.classList.add('visible');box.replaceChildren();const note=document.createElement('div');note.className='scanhint';note.textContent='Looking for nearby 2.4 GHz networks…';box.append(note);try{const r=await fetch('/api/wifi/scan',{cache:'no-store'});const data=await r.json();if(!r.ok)throw Error(data.error||'Wi-Fi scan failed');box.replaceChildren();if(!data.networks.length){const n=document.createElement('div');n.className='scanhint';n.textContent='No visible networks found. You can still type an SSID manually.';box.append(n)}else{for(const net of data.networks){const b=document.createElement('button');b.type='button';b.className='scanitem';const name=document.createElement('span');name.textContent=net.ssid;const meta=document.createElement('span');meta.className='scanmeta';meta.textContent=net.rssi+' dBm · '+(net.secure?'Secured':'Open');b.append(name,meta);b.addEventListener('click',()=>{$(second?'ssid2':'ssid').value=net.ssid;box.classList.remove('visible');$(second?'password2':'password').focus()});box.append(b)}}}catch(err){box.replaceChildren();const n=document.createElement('div');n.className='scanhint';n.textContent='Scan unavailable: '+err.message;box.append(n)}finally{btn.disabled=false;btn.innerHTML='<svg class="icon" width="16" height="16" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="1.75" stroke-linecap="round" stroke-linejoin="round" aria-hidden="true"><path d="M2 8.8a16 16 0 0 1 20 0M5 12.5a11 11 0 0 1 14 0M8.5 16a6 6 0 0 1 7 0"/><circle cx="12" cy="20" r="1" fill="currentColor" stroke="none"/></svg> Scan Wi-Fi'}}
$('scanWifi').addEventListener('click',()=>scanWiFi(false));$('scanWifi2').addEventListener('click',()=>scanWiFi(true));
status();setInterval(status,4000);
</script></body></html>
)DBHTML";

bool authorizeWrite() { return true; } // Dashboard login intentionally disabled.
void jsonReply(int status, const String& message, bool error = false) {
  JsonDocument d;
  d[error ? "error" : "message"] = message;
  String out; serializeJson(d, out);
  server.sendHeader("Cache-Control", "no-store");
  server.send(status, "application/json", out);
}
// Captive portal detection endpoints used by Android, iOS/macOS, and Windows.
// A redirect to the local dashboard helps devices offer their Wi-Fi sign-in window.
void redirectToCaptivePortal() {
  server.sendHeader("Location", "http://192.168.4.1/", true);
  server.sendHeader("Cache-Control", "no-store, no-cache, must-revalidate");
  server.send(302, "text/plain", "Open Desk-Companion setup: http://192.168.4.1/");
}
// Resolve the user-entered city/state/country into coordinates using OpenWeatherMap.
// No latitude/longitude input is needed in the dashboard.
bool resolveLocation(const String& city, const String& region, const String& country,
                     const String& key, float& latOut, float& lonOut) {
  if (WiFi.status() != WL_CONNECTED || key.isEmpty()) return false;
  String countryCode = country;
  countryCode.trim();
  if (countryCode.equalsIgnoreCase("India")) countryCode = "IN";
  else if (countryCode.equalsIgnoreCase("United States") || countryCode.equalsIgnoreCase("USA")) countryCode = "US";
  else if (countryCode.equalsIgnoreCase("United Kingdom")) countryCode = "GB";
  else if (countryCode.equalsIgnoreCase("United Arab Emirates")) countryCode = "AE";
  String query = city + "," + region + "," + countryCode;
  query.replace(" ", "%20"); query.replace(",", "%2C");
  String url = "http://api.openweathermap.org/geo/1.0/direct?q=" + query + "&limit=5&appid=" + key;
  HTTPClient req;
  req.setTimeout(8000);
  req.begin(url);
  int response = req.GET();
  if (response != 200) { req.end(); return false; }
  JsonDocument doc;
  DeserializationError err = deserializeJson(doc, req.getStream());
  req.end();
  if (err || !doc.is<JsonArray>() || doc.size() == 0) return false;
  // Prefer the first result whose state/country matches the user's selection.
  for (JsonVariant result : doc.as<JsonArray>()) {
    String resultState = result["state"] | "";
    String resultCountry = result["country"] | "";
    if (resultState.equalsIgnoreCase(region) && resultCountry.equalsIgnoreCase(countryCode)) {
      latOut = result["lat"].as<float>(); lonOut = result["lon"].as<float>();
      return true;
    }
  }
  // Do not silently select a different city/state when the query is ambiguous.
  return false;
}

void setupWebServer() {
  server.on("/generate_204", HTTP_GET, redirectToCaptivePortal); // Android
  server.on("/gen_204", HTTP_GET, redirectToCaptivePortal);
  server.on("/hotspot-detect.html", HTTP_GET, redirectToCaptivePortal); // Apple
  server.on("/library/test/success.html", HTTP_GET, redirectToCaptivePortal);
  server.on("/ncsi.txt", HTTP_GET, redirectToCaptivePortal); // Windows
  server.on("/connecttest.txt", HTTP_GET, redirectToCaptivePortal);
  server.on("/redirect", HTTP_GET, redirectToCaptivePortal);
  server.on("/canonical.html", HTTP_GET, redirectToCaptivePortal);
  server.on("/success.txt", HTTP_GET, redirectToCaptivePortal);
  server.on("/", HTTP_GET, []() {
    server.sendHeader("Cache-Control", "no-store");
    server.send_P(200, "text/html; charset=utf-8", DASHBOARD_HTML);
  });
  server.on("/api/status", HTTP_GET, []() {
    JsonDocument d;
    d["wifiConnected"] = WiFi.status() == WL_CONNECTED;
    d["ssid"] = WiFi.status() == WL_CONNECTED ? WiFi.SSID() : "";
    d["savedSSID"] = wifiSSID;
    d["savedSSID2"] = wifiSSID2;
    d["ap"] = apSSID;
    d["ip"] = WiFi.status() == WL_CONNECTED ? WiFi.localIP().toString() : WiFi.softAPIP().toString();
    d["uptime"] = millis() / 1000;
    d["screen"] = displayState;
    d["emotion"] = (int)currentEmotion;
    d["manualEmotion"] = manualEmotion;
    d["weatherReady"] = weatherConfigured && lastWeatherUpdate != 0;
    // ESP32-S3 on-die temperature sensor; not room/ambient temperature.
    d["cpuTempC"] = temperatureRead();
    d["condition"] = currentCondition;
    d["city"] = cityLabel;
    d["state"] = locationState;
    d["country"] = locationCountry;
    d["offset"] = utcOffsetSeconds / 60;
    String out; serializeJson(d, out);
    server.sendHeader("Cache-Control", "no-store");
    server.send(200, "application/json", out);
  });
  // Scan nearby 2.4 GHz Wi-Fi networks. Never return saved credentials.
  server.on("/api/wifi/scan", HTTP_GET, []() {
    // A synchronous scan may briefly pause the dashboard while radio scans.
    int count = WiFi.scanNetworks(false, true);
    if (count < 0) return jsonReply(503, "Wi-Fi scan unavailable", true);
    JsonDocument d;
    JsonArray networks = d["networks"].to<JsonArray>();
    for (int i = 0; i < count && i < 24; i++) {
      String name = WiFi.SSID(i);
      if (name.length() == 0) continue;
      bool exists = false;
      for (JsonObject network : networks) {
        if (network["ssid"].as<String>() == name) { exists = true; break; }
      }
      if (exists) continue;
      JsonObject network = networks.add<JsonObject>();
      network["ssid"] = name;
      network["rssi"] = WiFi.RSSI(i);
      network["secure"] = WiFi.encryptionType(i) != WIFI_AUTH_OPEN;
    }
    WiFi.scanDelete();
    String out; serializeJson(d, out);
    server.sendHeader("Cache-Control", "no-store");
    server.send(200, "application/json", out);
  });
  server.on("/api/screen", HTTP_POST, []() {
    if (!authorizeWrite()) return;
    int s = server.arg("screen").toInt();
    if (s < 0 || s > 3) return jsonReply(400, "Invalid screen", true);
    displayState = s; jsonReply(200, "Display updated");
  });
  server.on("/api/emotion", HTTP_POST, []() {
    if (!authorizeWrite()) return;
    int e = server.arg("emotion").toInt();
    if (e < -1 || e >= EMOTION_COUNT) return jsonReply(400, "Invalid emotion", true);
    manualEmotion = e >= 0;
    if (manualEmotion) currentEmotion = (Emotion)e;
    displayState = 0;
    lastEmotionChange = millis();
    jsonReply(200, manualEmotion ? "Emotion changed" : "Automatic emotions enabled");
  });
  server.on("/api/wifi", HTTP_POST, []() {
    if (!authorizeWrite()) return;
    String s = server.arg("ssid"), p = server.arg("password");
    s.trim();
    if (s.isEmpty() || s.length() > 32 || p.length() > 63)
      return jsonReply(400, "Check Wi-Fi name and password length", true);
    if (p.length() > 0 && p.length() < 8)
      return jsonReply(400, "Wi-Fi password needs at least 8 characters", true);
    if (s != wifiSSID && p.isEmpty())
      return jsonReply(400, "Enter password for the new Wi-Fi network", true);
    wifiSSID = s;
    setupRequired = false; // Leave first-time setup immediately once Wi-Fi is saved
    displayState = 0;
    if (p.length()) wifiPassword = p;
    prefs.putString("wifiSSID", wifiSSID);
    prefs.putString("wifiPass", wifiPassword);
    jsonReply(200, "Wi-Fi saved; attempting connection");
    WiFi.disconnect(false);
    WiFi.begin(wifiSSID.c_str(), wifiPassword.c_str());
  });
  server.on("/api/wifi2", HTTP_POST, []() {
    if (!authorizeWrite()) return;
    String s = server.arg("ssid"), p = server.arg("password");
    s.trim();
    if (s.isEmpty() || s.length() > 32 || p.length() > 63)
      return jsonReply(400, "Check Wi-Fi name and password length", true);
    if (p.length() > 0 && p.length() < 8)
      return jsonReply(400, "Wi-Fi password needs at least 8 characters", true);
    if (s != wifiSSID2 && p.isEmpty())
      return jsonReply(400, "Enter password for the new Wi-Fi network", true);
    wifiSSID2 = s;
    setupRequired = false;
    displayState = 0;
    if (p.length()) wifiPassword2 = p;
    prefs.putString("wifiSSID2", wifiSSID2);
    prefs.putString("wifiPass2", wifiPassword2);
    jsonReply(200, "Backup Wi-Fi saved; will be used when primary is unavailable");
    if (WiFi.status() != WL_CONNECTED) lastWiFiAttempt = millis() - WIFI_RETRY_MS;
  });
  server.on("/api/weather", HTTP_POST, []() {
    if (!authorizeWrite()) return;
    String c = server.arg("city"), region = server.arg("state"), country = server.arg("country");
    String key = server.arg("apiKey");
    c.trim(); region.trim(); country.trim(); key.trim();
    if (c.isEmpty() || region.isEmpty() || country.isEmpty() ||
        c.length() > 64 || region.length() > 64 || country.length() > 64)
      return jsonReply(400, "Enter a valid city, state and country", true);
    if (key.length() > 128) return jsonReply(400, "API key is too long", true);
    String effectiveKey = key.isEmpty() ? weatherKey : key;
    if (effectiveKey.isEmpty()) return jsonReply(400, "Enter an OpenWeatherMap API key", true);
    if (WiFi.status() != WL_CONNECTED)
      return jsonReply(503, "Connect Desk-Companion to internet Wi-Fi to find your city", true);
    float resolvedLat = 0, resolvedLon = 0;
    if (!resolveLocation(c, region, country, effectiveKey, resolvedLat, resolvedLon))
      return jsonReply(422, "Location not found. Check city, full state name and country (e.g. India or IN)", true);
    cityLabel = c; locationState = region; locationCountry = country;
    weatherLat = resolvedLat; weatherLon = resolvedLon; weatherKey = effectiveKey;
    weatherConfigured = true;
    prefs.putString("city", cityLabel);
    prefs.putString("state", locationState);
    prefs.putString("country", locationCountry);
    prefs.putFloat("lat", weatherLat);
    prefs.putFloat("lon", weatherLon);
    prefs.putBool("located", true);
    if (key.length()) prefs.putString("weatherKey", weatherKey);
    lastWeatherUpdate = 0; // Next loop fetches weather + location-specific UTC offset.
    jsonReply(200, "Location saved. Clock will sync to the city's local time.");
  });
  server.on("/api/sleep", HTTP_POST, []() {
    if (!authorizeWrite()) return;
    jsonReply(200, "Desk-Companion is going to sleep");
    triggerDeepSleep();
  });
  server.on("/api/restart", HTTP_POST, []() {
    if (!authorizeWrite()) return;
    jsonReply(200, "Restarting");
    delay(300);
    ESP.restart();
  });
  server.onNotFound([]() {
    // Preserve proper API errors. All other unknown URLs lead to the dashboard.
    if (server.uri().startsWith("/api/")) {
      server.send(404, "application/json", "{\"error\":\"Unknown API endpoint\"}");
    } else {
      redirectToCaptivePortal();
    }
  });
}
