// ForexRate based - Dot Matrix RSS feed and Stock Ticker
// This code was written for the MAX7219 Parola 8x8 LED dot matrix. Adjust the code to suit

#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <WiFiManager.h>

#include <SPI.h>
#include <MD_Parola.h>
#include <MD_MAX72XX.h>

#include <WiFiUdp.h>
#include <NTPClient.h>
#include <time.h>

#include <ESP8266HTTPClient.h>
#include <ArduinoJson.h>
#include <EEPROM.h>

// -------- Display --------
#define HARDWARE_TYPE MD_MAX72XX::PAROLA_HW    // FC16_HW or GENERIC_HW if your display is garbled. Check for reversed or mirroed text below
#define MAX_DEVICES   12   // Number of individual cells
#define CS_PIN        D8

MD_Parola display = MD_Parola(HARDWARE_TYPE, CS_PIN, MAX_DEVICES);

// -------- Web server --------
ESP8266WebServer server(80);

// -------- NTP --------
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "***.***.***.***");   // This is my hime NTP server but you can change this to time.microsoft.com or any other NTP server

// -------- Pi proxy --------
const char* piHost = "***.***.***.***";     // This is the IP address of the Raspberry Pi Proxy / JSON resolver on your network. 
const uint16_t piPort = 5000;

// -------- Display state --------
char messageBuffer[256];
uint8_t brightness = 5;
uint16_t scrollSpeed = 50;

typedef struct {
  const char* name;
  textEffect_t effect;
} EffectItem;

EffectItem effects[] = {
  {"Scroll Right→Left", PA_SCROLL_RIGHT},
  {"Scroll Left→Right", PA_SCROLL_LEFT},
  {"Wipe",              PA_WIPE},
  {"Wipe Reverse",      PA_WIPE_CURSOR},
  {"Open",              PA_OPENING},
  {"Close",             PA_CLOSING},
  {"Dissolve",          PA_DISSOLVE},
  {"Blinds",            PA_BLINDS},
  {"Print",             PA_PRINT},
};

uint8_t currentEffect = 0;

// -------- Rotation items --------
enum ItemType : uint8_t {
  ITEM_TIME = 0,
  ITEM_DATE,
  ITEM_TEMP,
  ITEM_HUM,
  ITEM_PRESS,
  ITEM_BATT,
  ITEM_BRENT,
  ITEM_WTI,
  ITEM_NATGAS,
  ITEM_COPPER,
  ITEM_GOLD,
  ITEM_SILVER,
  ITEM_BTC,
  ITEM_ETH,
  ITEM_SOL,
  ITEM_XRP,
  ITEM_DOGE,
  ITEM_FCST,

  // Major FX
  ITEM_GBPUSD,
  ITEM_GBPEUR,
  ITEM_GBPJPY,
  ITEM_GBPCHF,
  ITEM_GBPAUD,
  ITEM_GBPCAD,
  ITEM_GBPNZD,

  // Europe FX
  ITEM_GBPSEK,
  ITEM_GBPDKK,
  ITEM_GBPNOK,
  ITEM_GBPPLN,
  ITEM_GBPCZK,
  ITEM_GBPHUF,
  ITEM_GBPRON,
  ITEM_GBPBGN,
  ITEM_GBPHRK,
  ITEM_GBPISK,

  // Asia FX
  ITEM_GBPCNY,
  ITEM_GBPINR,
  ITEM_GBPSGD,
  ITEM_GBPHKD,
  ITEM_GBPTHB,
  ITEM_GBPKRW,
  ITEM_GBPPHP,

  // Middle East FX
  ITEM_GBPAED,
  ITEM_GBPSAR,
  ITEM_GBPILS,

  // Americas FX
  ITEM_GBPMXN,
  ITEM_GBPBRL,

  // Other FX
  ITEM_GBPZAR,
  ITEM_GBPTRY,
  ITEM_GBPRUB,

  // News & Markets
  ITEM_BBC,
  ITEM_BBCUK,
  ITEM_SKYNEWS,
  ITEM_NASA,
  ITEM_TECHRADAR,
  ITEM_BBCSPORT,
  ITEM_MKT,
  ITEM_SPX,
  ITEM_NDX,
  ITEM_FTSE,
  ITEM_N225,
  ITEM_NYSE,
  ITEM_DAX,
  ITEM_CAC,
  ITEM_HSI,
  ITEM_SSEC,
  ITEM_F_US500,
  ITEM_F_USTECH,
  ITEM_F_US30,
  ITEM_COUNT // ITEM_STK is now unused but keeps count correct
};

struct RotationItem {
  ItemType type;
  uint8_t repeats;  // how many times to show in a row
};

RotationItem rotationList[96];
uint8_t rotationLen = 0;
uint8_t rotationIndex = 0;
uint8_t rotationRepeatCounter = 0;

// -------- Selections (from WebUI, persisted) --------
struct Settings {
  uint32_t magic;

  bool selTime;
  bool selDate;
  bool selTemp;
  bool selHum;
  bool selPress;
  bool selBatt;
  bool selBrent;
  bool selWTI;
  bool selNatGas;
  bool selCopper;
  bool selGold;
  bool selSilver;
  bool selBTC;
  bool selETH;
  bool selSOL;
  bool selXRP;
  bool selDOGE;
  bool selFcst;

  // Major FX
  bool selGBPUSD;
  bool selGBPEUR;
  bool selGBPJPY;
  bool selGBPCHF;
  bool selGBPAUD;
  bool selGBPCAD;
  bool selGBPNZD;

  // Europe FX
  bool selGBPSEK;
  bool selGBPDKK;
  bool selGBPNOK;
  bool selGBPPLN;
  bool selGBPCZK;
  bool selGBPHUF;
  bool selGBPRON;
  bool selGBPBGN;
  bool selGBPHRK;
  bool selGBPISK;

  // Asia FX
  bool selGBPCNY;
  bool selGBPINR;
  bool selGBPSGD;
  bool selGBPHKD;
  bool selGBPTHB;
  bool selGBPKRW;
  bool selGBPPHP;

  // Middle East FX
  bool selGBPAED;
  bool selGBPSAR;
  bool selGBPILS;

  // Americas FX
  bool selGBPMXN;
  bool selGBPBRL;

  // Other FX
  bool selGBPZAR;
  bool selGBPTRY;
  bool selGBPRUB;

  // News & Markets
  bool selBBC;
  bool selBBCUK;
  bool selSkyNews;
  bool selNASA;
  bool selTechRadar;
  bool selBBCSport;
  bool selDJI;
  bool selSPX;
  bool selNDX;
  bool selFTSE;
  bool selN225;
  bool selNYSE;
  bool selDAX;
  bool selCAC;
  bool selHSI;
  bool selSSEC;
  bool selF_US500;
  bool selF_USTECH;
  bool selF_US30;
  uint8_t effect;
  uint8_t bright;
  uint16_t speed;
};

Settings settings;

// -------- ESP-side cache (60s per item) --------
String itemCacheText[ITEM_COUNT];
unsigned long itemCacheTs[ITEM_COUNT];
const unsigned long ITEM_CACHE_MS = 60000;    // This cache's the data for smooth recovery rather than pulling on script

// -------- HTML --------
const char MAIN_page[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
<meta charset="UTF-8">
<title>G5STO's - LED Matrix Full Selection</title>
<style>
body { font-family: Arial; margin: 40px; }
fieldset { margin-bottom: 15px; }
label { display: block; margin: 4px 0; }
details { margin-bottom: 8px; }
summary { font-weight: bold; cursor: pointer; }
</style>
</head>
<body>
<h2>G5STO's LED Matrix - Full Selection</h2>

<form action="/set" method="POST">
  <fieldset>
    <legend>Core Data</legend>
    <label><input type="checkbox" name="time" %TIME%> Time</label>
    <label><input type="checkbox" name="date" %DATE%> Date</label>
    <label><input type="checkbox" name="temp" %TEMP%> Temperature</label>
    <label><input type="checkbox" name="hum" %HUM%> Humidity</label>
    <label><input type="checkbox" name="press" %PRESS%> Pressure</label>
    <label><input type="checkbox" name="bat" %BATT%> Battery</label>
    <label><input type="checkbox" name="brent" %BRENT%> Brent Crude</label>
    <label><input type="checkbox" name="wti" %WTI%> WTI Crude</label>
    <label><input type="checkbox" name="natgas" %NATGAS%> Natural Gas</label>
    <label><input type="checkbox" name="copper" %COPPER%> Copper</label>
    <label><input type="checkbox" name="gold" %GOLD%> Gold</label>
    <label><input type="checkbox" name="silver" %SILVER%> Silver</label>
    <label><input type="checkbox" name="btc" %BTC%> Bitcoin</label>
    <label><input type="checkbox" name="eth" %ETH%> Ethereum</label>
    <label><input type="checkbox" name="sol" %SOL%> Solana</label>
    <label><input type="checkbox" name="xrp" %XRP%> XRP</label>
    <label><input type="checkbox" name="doge" %DOGE%> Dogecoin</label>
    <label><input type="checkbox" name="fcst" %FCST%> Forecast</label>
  </fieldset>

  <fieldset>
    <legend>FX Rates (GBP)</legend>

    <details>
      <summary>Major FX</summary>
      <label><input type="checkbox" name="gbpusd" %GBPUSD%> GBP/USD</label>
      <label><input type="checkbox" name="gbpeur" %GBPEUR%> GBP/EUR</label>
      <label><input type="checkbox" name="gbpjpy" %GBPJPY%> GBP/JPY</label>
      <label><input type="checkbox" name="gbpchf" %GBPCHF%> GBP/CHF</label>
      <label><input type="checkbox" name="gbpaud" %GBPAUD%> GBP/AUD</label>
      <label><input type="checkbox" name="gbpcad" %GBPCAD%> GBP/CAD</label>
      <label><input type="checkbox" name="gbpnzd" %GBPNZD%> GBP/NZD</label>
    </details>

    <details>
      <summary>Europe FX</summary>
      <label><input type="checkbox" name="gbpsek" %GBPSEK%> GBP/SEK</label>
      <label><input type="checkbox" name="gbpdkk" %GBPDKK%> GBP/DKK</label>
      <label><input type="checkbox" name="gbpnok" %GBPNOK%> GBP/NOK</label>
      <label><input type="checkbox" name="gbppln" %GBPPLN%> GBP/PLN</label>
      <label><input type="checkbox" name="gbpczk" %GBPCZK%> GBP/CZK</label>
      <label><input type="checkbox" name="gbphuf" %GBPHUF%> GBP/HUF</label>
      <label><input type="checkbox" name="gbpron" %GBPRON%> GBP/RON</label>
      <label><input type="checkbox" name="gbpbgn" %GBPBGN%> GBP/BGN</label>
      <label><input type="checkbox" name="gbphrk" %GBPHRK%> GBP/HRK</label>
      <label><input type="checkbox" name="gbpisk" %GBPISK%> GBP/ISK</label>
    </details>

    <details>
      <summary>Asia FX</summary>
      <label><input type="checkbox" name="gbpcny" %GBPCNY%> GBP/CNY</label>
      <label><input type="checkbox" name="gbpinr" %GBPINR%> GBP/INR</label>
      <label><input type="checkbox" name="gbpsgd" %GBPSGD%> GBP/SGD</label>
      <label><input type="checkbox" name="gbphkd" %GBPHKD%> GBP/HKD</label>
      <label><input type="checkbox" name="gbpthb" %GBPTHB%> GBP/THB</label>
      <label><input type="checkbox" name="gbpkrw" %GBPKRW%> GBP/KRW</label>
      <label><input type="checkbox" name="gbpphp" %GBPPHP%> GBP/PHP</label>
    </details>

    <details>
      <summary>Middle East FX</summary>
      <label><input type="checkbox" name="gbpaed" %GBPAED%> GBP/AED</label>
      <label><input type="checkbox" name="gbpsar" %GBPSAR%> GBP/SAR</label>
      <label><input type="checkbox" name="gbpils" %GBPILS%> GBP/ILS</label>
    </details>

    <details>
      <summary>Americas FX</summary>
      <label><input type="checkbox" name="gbpmxn" %GBPMXN%> GBP/MXN</label>
      <label><input type="checkbox" name="gbpbrl" %GBPBRL%> GBP/BRL</label>
    </details>

    <details>
      <summary>Other FX</summary>
      <label><input type="checkbox" name="gbpzar" %GBPZAR%> GBP/ZAR</label>
      <label><input type="checkbox" name="gbptry" %GBPTRY%> GBP/TRY</label>
      <label><input type="checkbox" name="gbprub" %GBPRUB%> GBP/RUB</label>
    </details>
  </fieldset>

  <fieldset>
    <legend>News & Markets</legend>
    <label><input type="checkbox" name="bbc" %BBC%> BBC News</label>
    <label><input type="checkbox" name="bbcuk" %BBCUK%> BBC UK</label>
    <label><input type="checkbox" name="skynews" %SKYNEWS%> Sky News</label>
    <label><input type="checkbox" name="nasa" %NASA%> NASA</label>
    <label><input type="checkbox" name="techradar" %TECHRADAR%> TechRadar</label>
    <label><input type="checkbox" name="bbcsport" %BBCSPORT%> BBC Sport</label>
    <label><input type="checkbox" name="dji" %DJI%> Dow Jones</label>
    <label><input type="checkbox" name="spx" %SPX%> S&P 500</label>
    <label><input type="checkbox" name="ndx" %NDX%> Nasdaq</label>
    <label><input type="checkbox" name="ftse" %FTSE%> FTSE 100</label>
    <label><input type="checkbox" name="n225" %N225%> Nikkei 225</label>
    <label><input type="checkbox" name="nyse" %NYSE%> NYSE</label>
    <label><input type="checkbox" name="dax" %DAX%> DAX (Germany)</label>
    <label><input type="checkbox" name="cac" %CAC%> CAC 40 (France)</label>
    <label><input type="checkbox" name="hsi" %HSI%> Hang Seng (HK)</label>
    <label><input type="checkbox" name="ssec" %SSEC%> Shanghai Comp</label>
    <label><input type="checkbox" name="fus500" %F_US500%> US 500 Futures</label>
    <label><input type="checkbox" name="fustech" %F_USTECH%> US Tech Futures</label>
    <label><input type="checkbox" name="fus30" %F_US30%> US 30 Futures</label>
  </fieldset>

  <fieldset>
    <legend>Display Settings</legend>
    <label>Effect:
      <select name="fx">
        <option value="0">Scroll Right→Left</option>
        <option value="1">Scroll Left→Right</option>
        <option value="2">Wipe</option>
        <option value="3">Wipe Reverse</option>
        <option value="4">Open</option>
        <option value="5">Close</option>
        <option value="6">Dissolve</option>
        <option value="7">Blinds</option>
        <option value="8">Print</option>
      </select>
    </label>
    <label>Brightness (0–15): <span id="bval">%BRIGHT%</span>
      <input type="range" name="bright" min="0" max="15" value="%BRIGHT%" oninput="bval.innerText=this.value">
    </label>
    <label>Speed (10–200): <span id="sval">%SPEED%</span>
      <input type="range" name="speed" min="10" max="200" value="%SPEED%" oninput="sval.innerText=this.value">
    </label>
  </fieldset>

  <input type="submit" value="Save & Restart Rotation">
</form>

<p>Current item: <b id="cur"></b></p>
<p>Current text: <b id="txt"></b></p>
<p>Time now: <b id="time"></b></p>
<p>Date now: <b id="date"></b></p>

<script>
function refreshStatus() {
  fetch('/current').then(r => r.json()).then(j => {
    document.getElementById('cur').innerText = j.item;
    document.getElementById('txt').innerText = j.text;
    document.getElementById('time').innerText = j.time;
    document.getElementById('date').innerText = j.date;
  });
}
setInterval(refreshStatus, 5000);
refreshStatus();
</script>

</body>
</html>
)rawliteral";

// -------- Helpers for data acquisition --------
String getTimeString() {
  timeClient.update();
  return timeClient.getFormattedTime();
}

String getDateString() {
  timeClient.update();
  time_t raw = timeClient.getEpochTime();
  struct tm * ti = localtime(&raw);

  char buf[16];
  sprintf(buf, "%02d/%02d/%04d", ti->tm_mday, ti->tm_mon + 1, ti->tm_year + 1900);
  return String(buf);
}

bool fetchSingleTitleFromPi(const String& category, String& out) {
  WiFiClient client;
  HTTPClient http;

  String url = "http://" + String(piHost) + ":" + String(piPort) +
               "/rss?category=" + category;

  if (!http.begin(client, url)) {
    return false;
  }

  int code = http.GET();
  if (code != 200) {
    http.end();
    return false;
  }

  String payload = http.getString();
  http.end();

  StaticJsonDocument<4096> doc;
  DeserializationError err = deserializeJson(doc, payload);
  if (err) return false;

  const char* status = doc["status"] | "error";
  if (String(status) != "ok") return false;

  JsonArray items = doc["items"].as<JsonArray>();
  if (!items.size()) return false;

  const char* title = items[0]["title"] | "";
  out = String(title);
  out.trim();
  return out.length() > 0;
}

// -------- ESP cache wrapper --------
bool getCachedOrFetch(ItemType t, const String& category, String& out) {
  unsigned long now = millis();
  if (itemCacheText[t].length() > 0 && (now - itemCacheTs[t] < ITEM_CACHE_MS)) {
    Serial.print("Cache hit for ");
    Serial.println(category);

    out = itemCacheText[t];
    return true;
  }

  if (!fetchSingleTitleFromPi(category, out)) return false;

  itemCacheText[t] = out;
  itemCacheTs[t] = now;
  return true;
}

// -------- Rotation building --------
void buildRotation() {
  rotationLen = 0;

  auto addItem = [&](ItemType t, uint8_t reps) {
    if (rotationLen >= 96) return;
    rotationList[rotationLen].type = t;
    rotationList[rotationLen].repeats = reps;
    rotationLen++;
  };

  // Stage A: core + FX, each ×2
  if (settings.selTime)   addItem(ITEM_TIME, 1);     // If you wish to disply each message moer than once change the 1 to 2 or desired amount. Default is 1
  if (settings.selDate)   addItem(ITEM_DATE, 1);
  if (settings.selTemp)   addItem(ITEM_TEMP, 1);
  if (settings.selHum)    addItem(ITEM_HUM, 1);
  if (settings.selPress)  addItem(ITEM_PRESS, 1);
  if (settings.selBatt)   addItem(ITEM_BATT, 1);
  if (settings.selBrent)  addItem(ITEM_BRENT, 1);
  if (settings.selWTI)    addItem(ITEM_WTI, 1);
  if (settings.selNatGas) addItem(ITEM_NATGAS, 1);
  if (settings.selCopper) addItem(ITEM_COPPER, 1);
  if (settings.selGold)   addItem(ITEM_GOLD, 1);
  if (settings.selSilver) addItem(ITEM_SILVER, 1);
  if (settings.selBTC)  addItem(ITEM_BTC, 1);
  if (settings.selETH)  addItem(ITEM_ETH, 1);
  if (settings.selSOL)  addItem(ITEM_SOL, 1);
  if (settings.selXRP)  addItem(ITEM_XRP, 1);
  if (settings.selDOGE) addItem(ITEM_DOGE, 1);
  if (settings.selFcst)   addItem(ITEM_FCST, 1);

  // Major FX
  if (settings.selGBPUSD) addItem(ITEM_GBPUSD, 1);
  if (settings.selGBPEUR) addItem(ITEM_GBPEUR, 1);
  if (settings.selGBPJPY) addItem(ITEM_GBPJPY, 1);
  if (settings.selGBPCHF) addItem(ITEM_GBPCHF, 1);
  if (settings.selGBPAUD) addItem(ITEM_GBPAUD, 1);
  if (settings.selGBPCAD) addItem(ITEM_GBPCAD, 1);
  if (settings.selGBPNZD) addItem(ITEM_GBPNZD, 1);

  // Europe FX
  if (settings.selGBPSEK) addItem(ITEM_GBPSEK, 1);
  if (settings.selGBPDKK) addItem(ITEM_GBPDKK, 1);
  if (settings.selGBPNOK) addItem(ITEM_GBPNOK, 1);
  if (settings.selGBPPLN) addItem(ITEM_GBPPLN, 1);
  if (settings.selGBPCZK) addItem(ITEM_GBPCZK, 1);
  if (settings.selGBPHUF) addItem(ITEM_GBPHUF, 1);
  if (settings.selGBPRON) addItem(ITEM_GBPRON, 1);
  if (settings.selGBPBGN) addItem(ITEM_GBPBGN, 1);
  if (settings.selGBPHRK) addItem(ITEM_GBPHRK, 1);
  if (settings.selGBPISK) addItem(ITEM_GBPISK, 1);

  // Asia FX
  if (settings.selGBPCNY) addItem(ITEM_GBPCNY, 1);
  if (settings.selGBPINR) addItem(ITEM_GBPINR, 1);
  if (settings.selGBPSGD) addItem(ITEM_GBPSGD, 1);
  if (settings.selGBPHKD) addItem(ITEM_GBPHKD, 1);
  if (settings.selGBPTHB) addItem(ITEM_GBPTHB, 1);
  if (settings.selGBPKRW) addItem(ITEM_GBPKRW, 1);
  if (settings.selGBPPHP) addItem(ITEM_GBPPHP, 1);

  // Middle East FX
  if (settings.selGBPAED) addItem(ITEM_GBPAED, 1);
  if (settings.selGBPSAR) addItem(ITEM_GBPSAR, 1);
  if (settings.selGBPILS) addItem(ITEM_GBPILS, 1);

  // Americas FX
  if (settings.selGBPMXN) addItem(ITEM_GBPMXN, 1);
  if (settings.selGBPBRL) addItem(ITEM_GBPBRL, 1);

  // Other FX
  if (settings.selGBPZAR) addItem(ITEM_GBPZAR, 1);
  if (settings.selGBPTRY) addItem(ITEM_GBPTRY, 1);
  if (settings.selGBPRUB) addItem(ITEM_GBPRUB, 1);

  // Stage B: news & markets, each ×1
  if (settings.selBBC)      addItem(ITEM_BBC, 1);
  if (settings.selBBCUK)    addItem(ITEM_BBCUK, 1);
  if (settings.selSkyNews)  addItem(ITEM_SKYNEWS, 1);
  if (settings.selNASA)     addItem(ITEM_NASA, 1);
  if (settings.selTechRadar)addItem(ITEM_TECHRADAR, 1);
  if (settings.selBBCSport) addItem(ITEM_BBCSPORT, 1);
  if (settings.selDJI)      addItem(ITEM_MKT, 1); // MKT is now DJI
  if (settings.selSPX)      addItem(ITEM_SPX, 1);
  if (settings.selNDX)      addItem(ITEM_NDX, 1);
  if (settings.selFTSE)     addItem(ITEM_FTSE, 1);
  if (settings.selN225)     addItem(ITEM_N225, 1);
  if (settings.selNYSE)     addItem(ITEM_NYSE, 1);
  if (settings.selDAX)      addItem(ITEM_DAX, 1);
  if (settings.selCAC)      addItem(ITEM_CAC, 1);
  if (settings.selHSI)      addItem(ITEM_HSI, 1);
  if (settings.selSSEC)     addItem(ITEM_SSEC, 1);
  if (settings.selF_US500)  addItem(ITEM_F_US500, 1);
  if (settings.selF_USTECH) addItem(ITEM_F_USTECH, 1);
  if (settings.selF_US30)   addItem(ITEM_F_US30, 1);
  if (rotationLen == 0) {
    addItem(ITEM_TIME, 2);
  }

  rotationIndex = 0;
  rotationRepeatCounter = rotationList[0].repeats;
}

// -------- Get text for current item --------
bool getItemText(ItemType t, String& out) {
  switch (t) {
    case ITEM_TIME:
      out = getTimeString();
      return true;
    case ITEM_DATE:
      out = getDateString();
      return true;
    case ITEM_TEMP:
      return getCachedOrFetch(t, "Temp", out);
    case ITEM_HUM:
      return getCachedOrFetch(t, "Humidity", out);
    case ITEM_PRESS:
      return getCachedOrFetch(t, "Pressure", out);
    case ITEM_BATT:
      return getCachedOrFetch(t, "Battery", out);
    case ITEM_BRENT:
      return getCachedOrFetch(t, "Brent", out);
    case ITEM_WTI:
      return getCachedOrFetch(t, "WTI", out);
    case ITEM_NATGAS:
      return getCachedOrFetch(t, "NatGas", out);
    case ITEM_COPPER:
      return getCachedOrFetch(t, "Copper", out);
    case ITEM_GOLD:
      return getCachedOrFetch(t, "Gold", out);
    case ITEM_SILVER:
      return getCachedOrFetch(t, "Silver", out);
    case ITEM_BTC:
      return getCachedOrFetch(t, "BTC", out);
    case ITEM_ETH:
      return getCachedOrFetch(t, "ETH", out);
    case ITEM_SOL:
      return getCachedOrFetch(t, "SOL", out);
    case ITEM_XRP:
      return getCachedOrFetch(t, "XRP", out);
    case ITEM_DOGE:
      return getCachedOrFetch(t, "DOGE", out);
    case ITEM_FCST:
      return getCachedOrFetch(t, "Forecast", out);

    // Major FX
    case ITEM_GBPUSD: return getCachedOrFetch(t, "GBPUSD", out);
    case ITEM_GBPEUR: return getCachedOrFetch(t, "GBPEUR", out);
    case ITEM_GBPJPY: return getCachedOrFetch(t, "GBPJPY", out);
    case ITEM_GBPCHF: return getCachedOrFetch(t, "GBPCHF", out);
    case ITEM_GBPAUD: return getCachedOrFetch(t, "GBPAUD", out);
    case ITEM_GBPCAD: return getCachedOrFetch(t, "GBPCAD", out);
    case ITEM_GBPNZD: return getCachedOrFetch(t, "GBPNZD", out);

    // Europe FX
    case ITEM_GBPSEK: return getCachedOrFetch(t, "GBPSEK", out);
    case ITEM_GBPDKK: return getCachedOrFetch(t, "GBPDKK", out);
    case ITEM_GBPNOK: return getCachedOrFetch(t, "GBPNOK", out);
    case ITEM_GBPPLN: return getCachedOrFetch(t, "GBPPLN", out);
    case ITEM_GBPCZK: return getCachedOrFetch(t, "GBPCZK", out);
    case ITEM_GBPHUF: return getCachedOrFetch(t, "GBPHUF", out);
    case ITEM_GBPRON: return getCachedOrFetch(t, "GBPRON", out);
    case ITEM_GBPBGN: return getCachedOrFetch(t, "GBPBGN", out);
    case ITEM_GBPHRK: return getCachedOrFetch(t, "GBPHRK", out);
    case ITEM_GBPISK: return getCachedOrFetch(t, "GBPISK", out);

    // Asia FX
    case ITEM_GBPCNY: return getCachedOrFetch(t, "GBPCNY", out);
    case ITEM_GBPINR: return getCachedOrFetch(t, "GBPINR", out);
    case ITEM_GBPSGD: return getCachedOrFetch(t, "GBPSGD", out);
    case ITEM_GBPHKD: return getCachedOrFetch(t, "GBPHKD", out);
    case ITEM_GBPTHB: return getCachedOrFetch(t, "GBPTHB", out);
    case ITEM_GBPKRW: return getCachedOrFetch(t, "GBPKRW", out);
    case ITEM_GBPPHP: return getCachedOrFetch(t, "GBPPHP", out);

    // Middle East FX
    case ITEM_GBPAED: return getCachedOrFetch(t, "GBPAED", out);
    case ITEM_GBPSAR: return getCachedOrFetch(t, "GBPSAR", out);
    case ITEM_GBPILS: return getCachedOrFetch(t, "GBPILS", out);

    // Americas FX
    case ITEM_GBPMXN: return getCachedOrFetch(t, "GBPMXN", out);
    case ITEM_GBPBRL: return getCachedOrFetch(t, "GBPBRL", out);

    // Other FX
    case ITEM_GBPZAR: return getCachedOrFetch(t, "GBPZAR", out);
    case ITEM_GBPTRY: return getCachedOrFetch(t, "GBPTRY", out);
    case ITEM_GBPRUB: return getCachedOrFetch(t, "GBPRUB", out);

    // News & Markets
    case ITEM_BBC:      return getCachedOrFetch(t, "BBC", out);
    case ITEM_BBCUK:    return getCachedOrFetch(t, "BBCUK", out);
    case ITEM_SKYNEWS:  return getCachedOrFetch(t, "SkyNews", out);
    case ITEM_NASA:     return getCachedOrFetch(t, "NASA", out);
    case ITEM_TECHRADAR:return getCachedOrFetch(t, "TechRadar", out);
    case ITEM_BBCSPORT: return getCachedOrFetch(t, "BBCSport", out);
    case ITEM_MKT:      return getCachedOrFetch(t, "DJI", out); // MKT is now DJI
    case ITEM_SPX:      return getCachedOrFetch(t, "SPX", out);
    case ITEM_NDX:      return getCachedOrFetch(t, "NDX", out);
    case ITEM_FTSE:     return getCachedOrFetch(t, "FTSE", out);
    case ITEM_N225:     return getCachedOrFetch(t, "N225", out);
    case ITEM_NYSE:     return getCachedOrFetch(t, "NYSE", out);
    case ITEM_DAX:      return getCachedOrFetch(t, "DAX", out);
    case ITEM_CAC:      return getCachedOrFetch(t, "CAC", out);
    case ITEM_HSI:      return getCachedOrFetch(t, "HSI", out);
    case ITEM_SSEC:     return getCachedOrFetch(t, "SSEC", out);
    case ITEM_F_US500:  return getCachedOrFetch(t, "F_US500", out);
    case ITEM_F_USTECH: return getCachedOrFetch(t, "F_USTECH", out);
    case ITEM_F_US30:   return getCachedOrFetch(t, "F_US30", out);
    default:
      out = "";
      return false;
  }
}

// -------- This code stores information in the EEPROM and recovers it on reboot --------
void loadSettings() {
  EEPROM.begin(1024);
  EEPROM.get(0, settings);
  if (settings.magic != 0xA5A5A5A5) {
    // Defaults
    memset(&settings, 0, sizeof(settings));
    settings.magic = 0xA5A5A5A5;

    settings.selTime = true;
    settings.selDate = true;
    settings.selTemp = true;
    settings.selHum = true;
    settings.selPress = true;
    settings.selBatt = true;
    settings.selBrent = true;
    settings.selWTI = true;
    settings.selNatGas = true;
    settings.selCopper = true;
    settings.selGold = true;
    settings.selSilver = true;
    settings.selBTC = true;
    settings.selETH = true;
    settings.selSOL = true;
    settings.selXRP = true;
    settings.selDOGE = true;
    settings.selFcst = true;

    // Major FX default ON
    settings.selGBPUSD = true;
    settings.selGBPEUR = true;
    settings.selGBPJPY = true;
    settings.selGBPCHF = true;
    settings.selGBPAUD = true;
    settings.selGBPCAD = true;
    settings.selGBPNZD = true;

    // Others default OFF
    settings.effect = 0;
    settings.bright = 5;
    settings.speed = 50;

    EEPROM.put(0, settings);
    EEPROM.commit();
  }
}

void saveSettings() {
  EEPROM.put(0, settings);
  EEPROM.commit();
}

// -------- HTML helpers --------
String boolToAttr(bool v) { return v ? "checked" : ""; }

String processor(const String& var) {
  if (var == "TIME")     return boolToAttr(settings.selTime);
  if (var == "DATE")     return boolToAttr(settings.selDate);
  if (var == "TEMP")     return boolToAttr(settings.selTemp);
  if (var == "HUM")      return boolToAttr(settings.selHum);
  if (var == "PRESS")    return boolToAttr(settings.selPress);
  if (var == "BATT")     return boolToAttr(settings.selBatt);
  if (var == "BRENT")    return boolToAttr(settings.selBrent);
  if (var == "WTI")      return boolToAttr(settings.selWTI);
  if (var == "NATGAS")   return boolToAttr(settings.selNatGas);
  if (var == "COPPER")   return boolToAttr(settings.selCopper);
  if (var == "GOLD")     return boolToAttr(settings.selGold);
  if (var == "SILVER")   return boolToAttr(settings.selSilver);
  if (var == "BTC")     return boolToAttr(settings.selBTC);
  if (var == "ETH")     return boolToAttr(settings.selETH);
  if (var == "SOL")     return boolToAttr(settings.selSOL);
  if (var == "XRP")     return boolToAttr(settings.selXRP);
  if (var == "DOGE")    return boolToAttr(settings.selDOGE);
  if (var == "FCST")     return boolToAttr(settings.selFcst);

  if (var == "GBPUSD")   return boolToAttr(settings.selGBPUSD);
  if (var == "GBPEUR")   return boolToAttr(settings.selGBPEUR);
  if (var == "GBPJPY")   return boolToAttr(settings.selGBPJPY);
  if (var == "GBPCHF")   return boolToAttr(settings.selGBPCHF);
  if (var == "GBPAUD")   return boolToAttr(settings.selGBPAUD);
  if (var == "GBPCAD")   return boolToAttr(settings.selGBPCAD);
  if (var == "GBPNZD")   return boolToAttr(settings.selGBPNZD);

  if (var == "GBPSEK")   return boolToAttr(settings.selGBPSEK);
  if (var == "GBPDKK")   return boolToAttr(settings.selGBPDKK);
  if (var == "GBPNOK")   return boolToAttr(settings.selGBPNOK);
  if (var == "GBPPLN")   return boolToAttr(settings.selGBPPLN);
  if (var == "GBPCZK")   return boolToAttr(settings.selGBPCZK);
  if (var == "GBPHUF")   return boolToAttr(settings.selGBPHUF);
  if (var == "GBPRON")   return boolToAttr(settings.selGBPRON);
  if (var == "GBPBGN")   return boolToAttr(settings.selGBPBGN);
  if (var == "GBPHRK")   return boolToAttr(settings.selGBPHRK);
  if (var == "GBPISK")   return boolToAttr(settings.selGBPISK);

  if (var == "GBPCNY")   return boolToAttr(settings.selGBPCNY);
  if (var == "GBPINR")   return boolToAttr(settings.selGBPINR);
  if (var == "GBPSGD")   return boolToAttr(settings.selGBPSGD);
  if (var == "GBPHKD")   return boolToAttr(settings.selGBPHKD);
  if (var == "GBPTHB")   return boolToAttr(settings.selGBPTHB);
  if (var == "GBPKRW")   return boolToAttr(settings.selGBPKRW);
  if (var == "GBPPHP")   return boolToAttr(settings.selGBPPHP);

  if (var == "GBPAED")   return boolToAttr(settings.selGBPAED);
  if (var == "GBPSAR")   return boolToAttr(settings.selGBPSAR);
  if (var == "GBPILS")   return boolToAttr(settings.selGBPILS);

  if (var == "GBPMXN")   return boolToAttr(settings.selGBPMXN);
  if (var == "GBPBRL")   return boolToAttr(settings.selGBPBRL);

  if (var == "GBPZAR")   return boolToAttr(settings.selGBPZAR);
  if (var == "GBPTRY")   return boolToAttr(settings.selGBPTRY);
  if (var == "GBPRUB")   return boolToAttr(settings.selGBPRUB);

  if (var == "BBC")      return boolToAttr(settings.selBBC);
  if (var == "BBCUK")    return boolToAttr(settings.selBBCUK);
  if (var == "SKYNEWS")  return boolToAttr(settings.selSkyNews);
  if (var == "NASA")     return boolToAttr(settings.selNASA);
  if (var == "TECHRADAR")return boolToAttr(settings.selTechRadar);
  if (var == "BBCSPORT") return boolToAttr(settings.selBBCSport);
  if (var == "DJI")      return boolToAttr(settings.selDJI);
  if (var == "SPX")      return boolToAttr(settings.selSPX);
  if (var == "NDX")      return boolToAttr(settings.selNDX);
  if (var == "FTSE")     return boolToAttr(settings.selFTSE);
  if (var == "N225")     return boolToAttr(settings.selN225);
  if (var == "NYSE")     return boolToAttr(settings.selNYSE);
  if (var == "DAX")      return boolToAttr(settings.selDAX);
  if (var == "CAC")      return boolToAttr(settings.selCAC);
  if (var == "HSI")      return boolToAttr(settings.selHSI);
  if (var == "SSEC")     return boolToAttr(settings.selSSEC);
  if (var == "F_US500")  return boolToAttr(settings.selF_US500);
  if (var == "F_USTECH") return boolToAttr(settings.selF_USTECH);
  if (var == "F_US30")   return boolToAttr(settings.selF_US30);
  if (var == "BRIGHT")   return String(settings.bright);
  if (var == "SPEED")    return String(settings.speed);

  return "";
}

// -------- HTTP handlers --------
void handleRoot() {
  String page = MAIN_page;
  page.replace("%TIME%",     processor("TIME"));
  page.replace("%DATE%",     processor("DATE"));
  page.replace("%TEMP%",     processor("TEMP"));
  page.replace("%HUM%",      processor("HUM"));
  page.replace("%PRESS%",    processor("PRESS"));
  page.replace("%BATT%",     processor("BATT"));
  page.replace("%BRENT%",     processor("BRENT"));
  page.replace("%WTI%",       processor("WTI"));
  page.replace("%NATGAS%",    processor("NATGAS"));
  page.replace("%COPPER%",    processor("COPPER"));
  page.replace("%GOLD%",      processor("GOLD"));
  page.replace("%SILVER%",    processor("SILVER"));
  page.replace("%BTC%",     processor("BTC"));
  page.replace("%ETH%",     processor("ETH"));
  page.replace("%SOL%",     processor("SOL"));
  page.replace("%XRP%",     processor("XRP"));
  page.replace("%DOGE%",    processor("DOGE"));
  page.replace("%FCST%",     processor("FCST"));

  page.replace("%GBPUSD%",   processor("GBPUSD"));
  page.replace("%GBPEUR%",   processor("GBPEUR"));
  page.replace("%GBPJPY%",   processor("GBPJPY"));
  page.replace("%GBPCHF%",   processor("GBPCHF"));
  page.replace("%GBPAUD%",   processor("GBPAUD"));
  page.replace("%GBPCAD%",   processor("GBPCAD"));
  page.replace("%GBPNZD%",   processor("GBPNZD"));

  page.replace("%GBPSEK%",   processor("GBPSEK"));
  page.replace("%GBPDKK%",   processor("GBPDKK"));
  page.replace("%GBPNOK%",   processor("GBPNOK"));
  page.replace("%GBPPLN%",   processor("GBPPLN"));
  page.replace("%GBPCZK%",   processor("GBPCZK"));
  page.replace("%GBPHUF%",   processor("GBPHUF"));
  page.replace("%GBPRON%",   processor("GBPRON"));
  page.replace("%GBPBGN%",   processor("GBPBGN"));
  page.replace("%GBPHRK%",   processor("GBPHRK"));
  page.replace("%GBPISK%",   processor("GBPISK"));

  page.replace("%GBPCNY%",   processor("GBPCNY"));
  page.replace("%GBPINR%",   processor("GBPINR"));
  page.replace("%GBPSGD%",   processor("GBPSGD"));
  page.replace("%GBPHKD%",   processor("GBPHKD"));
  page.replace("%GBPTHB%",   processor("GBPTHB"));
  page.replace("%GBPKRW%",   processor("GBPKRW"));
  page.replace("%GBPPHP%",   processor("GBPPHP"));

  page.replace("%GBPAED%",   processor("GBPAED"));
  page.replace("%GBPSAR%",   processor("GBPSAR"));
  page.replace("%GBPILS%",   processor("GBPILS"));

  page.replace("%GBPMXN%",   processor("GBPMXN"));
  page.replace("%GBPBRL%",   processor("GBPBRL"));

  page.replace("%GBPZAR%",   processor("GBPZAR"));
  page.replace("%GBPTRY%",   processor("GBPTRY"));
  page.replace("%GBPRUB%",   processor("GBPRUB"));

  page.replace("%BBC%",      processor("BBC"));
  page.replace("%BBCUK%",    processor("BBCUK"));
  page.replace("%SKYNEWS%",  processor("SKYNEWS"));
  page.replace("%NASA%",     processor("NASA"));
  page.replace("%TECHRADAR%",processor("TECHRADAR"));
  page.replace("%BBCSPORT%", processor("BBCSPORT"));
  page.replace("%DJI%",      processor("DJI"));
  page.replace("%SPX%",      processor("SPX"));
  page.replace("%NDX%",      processor("NDX"));
  page.replace("%FTSE%",     processor("FTSE"));
  page.replace("%N225%",     processor("N225"));
  page.replace("%NYSE%",     processor("NYSE"));
  page.replace("%DAX%",      processor("DAX"));
  page.replace("%CAC%",      processor("CAC"));
  page.replace("%HSI%",      processor("HSI"));
  page.replace("%SSEC%",     processor("SSEC"));
  page.replace("%F_US500%",  processor("F_US500"));
  page.replace("%F_USTECH%", processor("F_USTECH"));
  page.replace("%F_US30%",   processor("F_US30"));
  page.replace("%BRIGHT%",   processor("BRIGHT"));
  page.replace("%SPEED%",    processor("SPEED"));

  server.send(200, "text/html", page);
}

void handleSet() {
  settings.selTime   = server.hasArg("time");
  settings.selDate   = server.hasArg("date");
  settings.selTemp   = server.hasArg("temp");
  settings.selHum    = server.hasArg("hum");
  settings.selPress  = server.hasArg("press");
  settings.selBatt   = server.hasArg("bat");
  settings.selBrent   = server.hasArg("brent");
  settings.selWTI     = server.hasArg("wti");
  settings.selNatGas  = server.hasArg("natgas");
  settings.selCopper  = server.hasArg("copper");
  settings.selGold    = server.hasArg("gold");
  settings.selSilver  = server.hasArg("silver");
  settings.selBTC   = server.hasArg("btc");
  settings.selETH   = server.hasArg("eth");
  settings.selSOL   = server.hasArg("sol");
  settings.selXRP   = server.hasArg("xrp");
  settings.selDOGE  = server.hasArg("doge");
  settings.selFcst   = server.hasArg("fcst");

  settings.selGBPUSD = server.hasArg("gbpusd");
  settings.selGBPEUR = server.hasArg("gbpeur");
  settings.selGBPJPY = server.hasArg("gbpjpy");
  settings.selGBPCHF = server.hasArg("gbpchf");
  settings.selGBPAUD = server.hasArg("gbpaud");
  settings.selGBPCAD = server.hasArg("gbpcad");
  settings.selGBPNZD = server.hasArg("gbpnzd");

  settings.selGBPSEK = server.hasArg("gbpsek");
  settings.selGBPDKK = server.hasArg("gbpdkk");
  settings.selGBPNOK = server.hasArg("gbpnok");
  settings.selGBPPLN = server.hasArg("gbppln");
  settings.selGBPCZK = server.hasArg("gbpczk");
  settings.selGBPHUF = server.hasArg("gbphuf");
  settings.selGBPRON = server.hasArg("gbpron");
  settings.selGBPBGN = server.hasArg("gbpbgn");
  settings.selGBPHRK = server.hasArg("gbphrk");
  settings.selGBPISK = server.hasArg("gbpisk");

  settings.selGBPCNY = server.hasArg("gbpcny");
  settings.selGBPINR = server.hasArg("gbpinr");
  settings.selGBPSGD = server.hasArg("gbpsgd");
  settings.selGBPHKD = server.hasArg("gbphkd");
  settings.selGBPTHB = server.hasArg("gbpthb");
  settings.selGBPKRW = server.hasArg("gbpkrw");
  settings.selGBPPHP = server.hasArg("gbpphp");

  settings.selGBPAED = server.hasArg("gbpaed");
  settings.selGBPSAR = server.hasArg("gbpsar");
  settings.selGBPILS = server.hasArg("gbpils");

  settings.selGBPMXN = server.hasArg("gbpmxn");
  settings.selGBPBRL = server.hasArg("gbpbrl");

  settings.selGBPZAR = server.hasArg("gbpzar");
  settings.selGBPTRY = server.hasArg("gbptry");
  settings.selGBPRUB = server.hasArg("gbprub");

  settings.selBBC      = server.hasArg("bbc");
  settings.selBBCUK    = server.hasArg("bbcuk");
  settings.selSkyNews  = server.hasArg("skynews");
  settings.selNASA     = server.hasArg("nasa");
  settings.selTechRadar= server.hasArg("techradar");
  settings.selBBCSport = server.hasArg("bbcsport");
  settings.selDJI      = server.hasArg("dji");
  settings.selSPX      = server.hasArg("spx");
  settings.selNDX      = server.hasArg("ndx");
  settings.selFTSE     = server.hasArg("ftse");
  settings.selN225     = server.hasArg("n225");
  settings.selNYSE     = server.hasArg("nyse");
  settings.selDAX      = server.hasArg("dax");
  settings.selCAC      = server.hasArg("cac");
  settings.selHSI      = server.hasArg("hsi");
  settings.selSSEC     = server.hasArg("ssec");
  settings.selF_US500  = server.hasArg("fus500");
  settings.selF_USTECH = server.hasArg("fustech");
  settings.selF_US30   = server.hasArg("fus30");
  if (server.hasArg("fx")) {
    settings.effect = constrain(server.arg("fx").toInt(), 0, 8);
  }
  if (server.hasArg("bright")) {
    settings.bright = constrain(server.arg("bright").toInt(), 0, 15);
    display.setIntensity(settings.bright);
  }
  if (server.hasArg("speed")) {
    settings.speed = constrain(server.arg("speed").toInt(), 10, 200);
  }

  saveSettings();
  buildRotation();

  server.sendHeader("Location", "/");
  server.send(303);
}

void handleCurrent() {
  String text;
  ItemType t = rotationList[rotationIndex].type;
  getItemText(t, text);

  String json = "{";
  json += "\"item\":" + String((int)t) + ",";
  json += "\"text\":\"" + text + "\",";
  json += "\"time\":\"" + getTimeString() + "\",";
  json += "\"date\":\"" + getDateString() + "\"";
  json += "}";
  server.send(200, "application/json", json);
}

// -------- Setup --------
void setup() {
  Serial.begin(115200);
  delay(200);

  loadSettings();

  WiFiManager wm;
  wm.autoConnect("ForexRate LED-Matrix");    // Look for this in your WiFi list when fisrt booting.

  timeClient.begin();
  timeClient.setUpdateInterval(60000);    // Waits for 1 minute before giving up

  display.begin();
  display.setIntensity(settings.bright);
  display.displayClear();
  display.setZoneEffect(0, true, PA_FLIP_LR);     // IMPORTANT, if your Matrix letters are mirrored back to front or upside down change to UD or RL

  for (int i = 0; i < ITEM_COUNT; i++) {
    itemCacheText[i] = "";
    itemCacheTs[i] = 0;
  }

  buildRotation();

  server.on("/", HTTP_GET, handleRoot);
  server.on("/set", HTTP_POST, handleSet);
  server.on("/current", HTTP_GET, handleCurrent);
  server.begin();
}

// -------- Standard Loop Handling --------
void loop() {
  server.handleClient();

  static bool first = true;
  static String currentText;

  if (first) {
    ItemType t = rotationList[rotationIndex].type;
    if (!getItemText(t, currentText)) currentText = "";
    currentText.toCharArray(messageBuffer, sizeof(messageBuffer));
    display.displayText(
      messageBuffer,
      PA_CENTER,
      settings.speed,
      0,
      effects[settings.effect].effect,
      effects[settings.effect].effect
    );
    first = false;
  }

  if (display.displayAnimate()) {
    if (rotationRepeatCounter > 1) {
      rotationRepeatCounter--;
    } else {
      rotationIndex++;
      if (rotationIndex >= rotationLen) rotationIndex = 0;
      rotationRepeatCounter = rotationList[rotationIndex].repeats;
    }

    ItemType t = rotationList[rotationIndex].type;
    if (!getItemText(t, currentText)) currentText = "";
    currentText.toCharArray(messageBuffer, sizeof(messageBuffer));
    display.displayText(
      messageBuffer,
      PA_CENTER,
      settings.speed,
      0,
      effects[settings.effect].effect,
      effects[settings.effect].effect
    );
  }
}
