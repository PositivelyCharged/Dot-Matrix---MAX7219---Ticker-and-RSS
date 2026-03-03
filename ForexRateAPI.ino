// The G5STO - Dot Matrix RSS feed and Stock Ticker
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
NTPClient timeClient(ntpUDP, "192.168.0.222");   // This is my hime NTP server but you can change this to time.microsoft.com or any other NTP server

// -------- Pi proxy --------
const char* piHost = "192.168.0.181";     // This is the IP address of the Raspberry Pi Proxy / JSON resolver on your network. 
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
  ITEM_FX_USD,
  ITEM_FX_EUR,
  ITEM_FX_JPY,
  ITEM_FX_CHF,
  ITEM_FX_AUD,
  ITEM_FX_CAD,
  ITEM_FX_NZD,

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
  ITEM_FX_SEK,
  ITEM_FX_DKK,
  ITEM_FX_NOK,
  ITEM_FX_PLN,
  ITEM_FX_CZK,
  ITEM_FX_HUF,
  ITEM_FX_RON,
  ITEM_FX_BGN,
  ITEM_FX_HRK,
  ITEM_FX_ISK,

  // Asia FX
  ITEM_GBPCNY,
  ITEM_GBPINR,
  ITEM_GBPSGD,
  ITEM_GBPHKD,
  ITEM_GBPTHB,
  ITEM_GBPKRW,
  ITEM_GBPPHP,
  ITEM_FX_CNY,
  ITEM_FX_INR,
  ITEM_FX_SGD,
  ITEM_FX_HKD,
  ITEM_FX_THB,
  ITEM_FX_KRW,
  ITEM_FX_PHP,

  // Middle East FX
  ITEM_GBPAED,
  ITEM_GBPSAR,
  ITEM_GBPILS,
  ITEM_FX_AED,
  ITEM_FX_SAR,
  ITEM_FX_ILS,

  // Americas FX
  ITEM_GBPMXN,
  ITEM_GBPBRL,
  ITEM_FX_MXN,
  ITEM_FX_BRL,

  // Other FX
  ITEM_GBPZAR,
  ITEM_GBPTRY,
  ITEM_GBPRUB,
  ITEM_FX_ZAR,
  ITEM_FX_TRY,
  ITEM_FX_RUB,

  // Cross-Currency FX
  ITEM_USDEUR,
  ITEM_USDJPY,
  ITEM_EURGBP,
  ITEM_EURJPY,

  // USD Fixed
  ITEM_USDGBP,
  ITEM_USDCHF,
  ITEM_USDAUD,
  ITEM_USDCAD,
  ITEM_USDNZD,
  ITEM_USDCNY,
  ITEM_USDINR,
  ITEM_USDZAR,
  ITEM_USDTRY,
  ITEM_USDMXN,
  ITEM_USDBRL,

  // Extras
  ITEM_PACMAN,
  ITEM_WIFI,

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

RotationItem rotationList[128];
uint8_t rotationLen = 0;
uint8_t rotationIndex = 0;
uint8_t rotationRepeatCounter = 0;

// -------- Selections (from WebUI, persisted) --------
struct Settings {
  uint32_t magic;

  char base[4]; // Base currency (GBP, USD, EUR)

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
  bool selFX_USD;
  bool selFX_EUR;
  bool selFX_JPY;
  bool selFX_CHF;
  bool selFX_AUD;
  bool selFX_CAD;
  bool selFX_NZD;

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
  bool selFX_SEK;
  bool selFX_DKK;
  bool selFX_NOK;
  bool selFX_PLN;
  bool selFX_CZK;
  bool selFX_HUF;
  bool selFX_RON;
  bool selFX_BGN;
  bool selFX_HRK;
  bool selFX_ISK;

  // Asia FX
  bool selGBPCNY;
  bool selGBPINR;
  bool selGBPSGD;
  bool selGBPHKD;
  bool selGBPTHB;
  bool selGBPKRW;
  bool selGBPPHP;
  bool selFX_CNY;
  bool selFX_INR;
  bool selFX_SGD;
  bool selFX_HKD;
  bool selFX_THB;
  bool selFX_KRW;
  bool selFX_PHP;

  // Middle East FX
  bool selGBPAED;
  bool selGBPSAR;
  bool selGBPILS;
  bool selFX_AED;
  bool selFX_SAR;
  bool selFX_ILS;

  // Americas FX
  bool selGBPMXN;
  bool selGBPBRL;
  bool selFX_MXN;
  bool selFX_BRL;

  // Other FX
  bool selGBPZAR;
  bool selGBPTRY;
  bool selGBPRUB;
  bool selFX_ZAR;
  bool selFX_TRY;
  bool selFX_RUB;

  // Cross-Currency FX
  bool selUSDEUR;
  bool selUSDJPY;
  bool selEURGBP;
  bool selEURJPY;

  // USD Fixed
  bool selUSDGBP;
  bool selUSDCHF;
  bool selUSDAUD;
  bool selUSDCAD;
  bool selUSDNZD;
  bool selUSDCNY;
  bool selUSDINR;
  bool selUSDZAR;
  bool selUSDTRY;
  bool selUSDMXN;
  bool selUSDBRL;

  // Extras
  bool selPacman;
  bool selWifi;

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
    <label>Base Currency:
      <select name="base">
        <option value="GBP" %BASE_GBP%>GBP</option>
        <option value="USD" %BASE_USD%>USD</option>
        <option value="EUR" %BASE_EUR%>EUR</option>
      </select>
    </label>
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
    <legend>Currency Rates (vs Base)</legend>

    <details>
      <summary>Major FX</summary>
      <label><input type="checkbox" name="gbpusd" %GBPUSD%> GBP/USD</label>
      <label><input type="checkbox" name="gbpeur" %GBPEUR%> GBP/EUR</label>
      <label><input type="checkbox" name="gbpjpy" %GBPJPY%> GBP/JPY</label>
      <label><input type="checkbox" name="gbpchf" %GBPCHF%> GBP/CHF</label>
      <label><input type="checkbox" name="gbpaud" %GBPAUD%> GBP/AUD</label>
      <label><input type="checkbox" name="gbpcad" %GBPCAD%> GBP/CAD</label>
      <label><input type="checkbox" name="gbpnzd" %GBPNZD%> GBP/NZD</label>
      <label><input type="checkbox" name="fxusd" %FX_USD%> USD</label>
      <label><input type="checkbox" name="fxeur" %FX_EUR%> EUR</label>
      <label><input type="checkbox" name="fxjpy" %FX_JPY%> JPY</label>
      <label><input type="checkbox" name="fxchf" %FX_CHF%> CHF</label>
      <label><input type="checkbox" name="fxaud" %FX_AUD%> AUD</label>
      <label><input type="checkbox" name="fxcad" %FX_CAD%> CAD</label>
      <label><input type="checkbox" name="fxnzd" %FX_NZD%> NZD</label>
    </details>
    
    <details>
      <summary>Cross-Currency FX</summary>
      <label><input type="checkbox" name="eurgbp" %EURGBP%> EUR/GBP</label>
      <label><input type="checkbox" name="eurjpy" %EURJPY%> EUR/JPY</label>
    </details>

    <details>
      <summary>USD Rates (Fixed)</summary>
      <label><input type="checkbox" name="usdgbp" %USDGBP%> USD/GBP</label>
      <label><input type="checkbox" name="usdeur" %USDEUR%> USD/EUR</label>
      <label><input type="checkbox" name="usdjpy" %USDJPY%> USD/JPY</label>
      <label><input type="checkbox" name="usdchf" %USDCHF%> USD/CHF</label>
      <label><input type="checkbox" name="usdaud" %USDAUD%> USD/AUD</label>
      <label><input type="checkbox" name="usdcad" %USDCAD%> USD/CAD</label>
      <label><input type="checkbox" name="usdnzd" %USDNZD%> USD/NZD</label>
      <label><input type="checkbox" name="usdcny" %USDCNY%> USD/CNY</label>
      <label><input type="checkbox" name="usdinr" %USDINR%> USD/INR</label>
      <label><input type="checkbox" name="usdzar" %USDZAR%> USD/ZAR</label>
      <label><input type="checkbox" name="usdtry" %USDTRY%> USD/TRY</label>
      <label><input type="checkbox" name="usdmxn" %USDMXN%> USD/MXN</label>
      <label><input type="checkbox" name="usdbrl" %USDBRL%> USD/BRL</label>
    </details>

    <details>
      <summary>Extras</summary>
      <label><input type="checkbox" name="pacman" %PACMAN%> Pac-Man Animation</label>
      <label><input type="checkbox" name="wifi" %WIFI%> WiFi Signal</label>
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
      <label><input type="checkbox" name="fxsek" %FX_SEK%> SEK</label>
      <label><input type="checkbox" name="fxdkk" %FX_DKK%> DKK</label>
      <label><input type="checkbox" name="fxnok" %FX_NOK%> NOK</label>
      <label><input type="checkbox" name="fxpln" %FX_PLN%> PLN</label>
      <label><input type="checkbox" name="fxczk" %FX_CZK%> CZK</label>
      <label><input type="checkbox" name="fxhuf" %FX_HUF%> HUF</label>
      <label><input type="checkbox" name="fxron" %FX_RON%> RON</label>
      <label><input type="checkbox" name="fxbgn" %FX_BGN%> BGN</label>
      <label><input type="checkbox" name="fxhrk" %FX_HRK%> HRK</label>
      <label><input type="checkbox" name="fxisk" %FX_ISK%> ISK</label>
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
      <label><input type="checkbox" name="fxcny" %FX_CNY%> CNY</label>
      <label><input type="checkbox" name="fxinr" %FX_INR%> INR</label>
      <label><input type="checkbox" name="fxsgd" %FX_SGD%> SGD</label>
      <label><input type="checkbox" name="fxhkd" %FX_HKD%> HKD</label>
      <label><input type="checkbox" name="fxthb" %FX_THB%> THB</label>
      <label><input type="checkbox" name="fxkrw" %FX_KRW%> KRW</label>
      <label><input type="checkbox" name="fxphp" %FX_PHP%> PHP</label>
    </details>

    <details>
      <summary>Middle East FX</summary>
      <label><input type="checkbox" name="gbpaed" %GBPAED%> GBP/AED</label>
      <label><input type="checkbox" name="gbpsar" %GBPSAR%> GBP/SAR</label>
      <label><input type="checkbox" name="gbpils" %GBPILS%> GBP/ILS</label>
      <label><input type="checkbox" name="fxaed" %FX_AED%> AED</label>
      <label><input type="checkbox" name="fxsar" %FX_SAR%> SAR</label>
      <label><input type="checkbox" name="fxils" %FX_ILS%> ILS</label>
    </details>

    <details>
      <summary>Americas FX</summary>
      <label><input type="checkbox" name="gbpmxn" %GBPMXN%> GBP/MXN</label>
      <label><input type="checkbox" name="gbpbrl" %GBPBRL%> GBP/BRL</label>
      <label><input type="checkbox" name="fxmxn" %FX_MXN%> MXN</label>
      <label><input type="checkbox" name="fxbrl" %FX_BRL%> BRL</label>
    </details>

    <details>
      <summary>Other FX</summary>
      <label><input type="checkbox" name="gbpzar" %GBPZAR%> GBP/ZAR</label>
      <label><input type="checkbox" name="gbptry" %GBPTRY%> GBP/TRY</label>
      <label><input type="checkbox" name="gbprub" %GBPRUB%> GBP/RUB</label>
      <label><input type="checkbox" name="fxzar" %FX_ZAR%> ZAR</label>
      <label><input type="checkbox" name="fxtry" %FX_TRY%> TRY</label>
      <label><input type="checkbox" name="fxrub" %FX_RUB%> RUB</label>
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

<div style="margin-top: 40px; padding-top: 20px; border-top: 1px solid #ccc; text-align: center; font-size: 0.9em; color: #555;">
  <p>
    Powered by <a href="https://forexrateapi.com/" title="Free Currency Rates API" target="_blank">ForexRateAPI.com</a>
  </p>
  <a href="https://forexrateapi.com/" title="Free Currency Rates API" target="_blank">
    <img src='https://forexrateapi.com/logo-dark.png' alt="Currency Rates API by ForexRateAPI.com" border="0" height="24">
  </a>
</div>

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
    if (rotationLen >= 128) return;
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
  if (settings.selFX_USD) addItem(ITEM_FX_USD, 1);
  if (settings.selFX_EUR) addItem(ITEM_FX_EUR, 1);
  if (settings.selFX_JPY) addItem(ITEM_FX_JPY, 1);
  if (settings.selFX_CHF) addItem(ITEM_FX_CHF, 1);
  if (settings.selFX_AUD) addItem(ITEM_FX_AUD, 1);
  if (settings.selFX_CAD) addItem(ITEM_FX_CAD, 1);
  if (settings.selFX_NZD) addItem(ITEM_FX_NZD, 1);

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
  if (settings.selFX_SEK) addItem(ITEM_FX_SEK, 1);
  if (settings.selFX_DKK) addItem(ITEM_FX_DKK, 1);
  if (settings.selFX_NOK) addItem(ITEM_FX_NOK, 1);
  if (settings.selFX_PLN) addItem(ITEM_FX_PLN, 1);
  if (settings.selFX_CZK) addItem(ITEM_FX_CZK, 1);
  if (settings.selFX_HUF) addItem(ITEM_FX_HUF, 1);
  if (settings.selFX_RON) addItem(ITEM_FX_RON, 1);
  if (settings.selFX_BGN) addItem(ITEM_FX_BGN, 1);
  if (settings.selFX_HRK) addItem(ITEM_FX_HRK, 1);
  if (settings.selFX_ISK) addItem(ITEM_FX_ISK, 1);

  // Asia FX
  if (settings.selGBPCNY) addItem(ITEM_GBPCNY, 1);
  if (settings.selGBPINR) addItem(ITEM_GBPINR, 1);
  if (settings.selGBPSGD) addItem(ITEM_GBPSGD, 1);
  if (settings.selGBPHKD) addItem(ITEM_GBPHKD, 1);
  if (settings.selGBPTHB) addItem(ITEM_GBPTHB, 1);
  if (settings.selGBPKRW) addItem(ITEM_GBPKRW, 1);
  if (settings.selGBPPHP) addItem(ITEM_GBPPHP, 1);
  if (settings.selFX_CNY) addItem(ITEM_FX_CNY, 1);
  if (settings.selFX_INR) addItem(ITEM_FX_INR, 1);
  if (settings.selFX_SGD) addItem(ITEM_FX_SGD, 1);
  if (settings.selFX_HKD) addItem(ITEM_FX_HKD, 1);
  if (settings.selFX_THB) addItem(ITEM_FX_THB, 1);
  if (settings.selFX_KRW) addItem(ITEM_FX_KRW, 1);
  if (settings.selFX_PHP) addItem(ITEM_FX_PHP, 1);

  // Middle East FX
  if (settings.selGBPAED) addItem(ITEM_GBPAED, 1);
  if (settings.selGBPSAR) addItem(ITEM_GBPSAR, 1);
  if (settings.selGBPILS) addItem(ITEM_GBPILS, 1);
  if (settings.selFX_AED) addItem(ITEM_FX_AED, 1);
  if (settings.selFX_SAR) addItem(ITEM_FX_SAR, 1);
  if (settings.selFX_ILS) addItem(ITEM_FX_ILS, 1);

  // Americas FX
  if (settings.selGBPMXN) addItem(ITEM_GBPMXN, 1);
  if (settings.selGBPBRL) addItem(ITEM_GBPBRL, 1);
  if (settings.selFX_MXN) addItem(ITEM_FX_MXN, 1);
  if (settings.selFX_BRL) addItem(ITEM_FX_BRL, 1);

  // Other FX
  if (settings.selGBPZAR) addItem(ITEM_GBPZAR, 1);
  if (settings.selGBPTRY) addItem(ITEM_GBPTRY, 1);
  if (settings.selGBPRUB) addItem(ITEM_GBPRUB, 1);
  if (settings.selFX_ZAR) addItem(ITEM_FX_ZAR, 1);
  if (settings.selFX_TRY) addItem(ITEM_FX_TRY, 1);
  if (settings.selFX_RUB) addItem(ITEM_FX_RUB, 1);

  // Cross-Currency FX
  if (settings.selUSDEUR) addItem(ITEM_USDEUR, 1);
  if (settings.selUSDJPY) addItem(ITEM_USDJPY, 1);
  if (settings.selEURGBP) addItem(ITEM_EURGBP, 1);
  if (settings.selEURJPY) addItem(ITEM_EURJPY, 1);

  // USD Fixed
  if (settings.selUSDGBP) addItem(ITEM_USDGBP, 1);
  if (settings.selUSDCHF) addItem(ITEM_USDCHF, 1);
  if (settings.selUSDAUD) addItem(ITEM_USDAUD, 1);
  if (settings.selUSDCAD) addItem(ITEM_USDCAD, 1);
  if (settings.selUSDNZD) addItem(ITEM_USDNZD, 1);
  if (settings.selUSDCNY) addItem(ITEM_USDCNY, 1);
  if (settings.selUSDINR) addItem(ITEM_USDINR, 1);
  if (settings.selUSDZAR) addItem(ITEM_USDZAR, 1);
  if (settings.selUSDTRY) addItem(ITEM_USDTRY, 1);
  if (settings.selUSDMXN) addItem(ITEM_USDMXN, 1);
  if (settings.selUSDBRL) addItem(ITEM_USDBRL, 1);

  // Extras
  if (settings.selPacman) addItem(ITEM_PACMAN, 1);
  if (settings.selWifi)   addItem(ITEM_WIFI, 1);

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
  String base = String(settings.base);
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
    case ITEM_FX_USD: return getCachedOrFetch(t, base + "USD", out);
    case ITEM_FX_EUR: return getCachedOrFetch(t, base + "EUR", out);
    case ITEM_FX_JPY: return getCachedOrFetch(t, base + "JPY", out);
    case ITEM_FX_CHF: return getCachedOrFetch(t, base + "CHF", out);
    case ITEM_FX_AUD: return getCachedOrFetch(t, base + "AUD", out);
    case ITEM_FX_CAD: return getCachedOrFetch(t, base + "CAD", out);
    case ITEM_FX_NZD: return getCachedOrFetch(t, base + "NZD", out);

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
    case ITEM_FX_SEK: return getCachedOrFetch(t, base + "SEK", out);
    case ITEM_FX_DKK: return getCachedOrFetch(t, base + "DKK", out);
    case ITEM_FX_NOK: return getCachedOrFetch(t, base + "NOK", out);
    case ITEM_FX_PLN: return getCachedOrFetch(t, base + "PLN", out);
    case ITEM_FX_CZK: return getCachedOrFetch(t, base + "CZK", out);
    case ITEM_FX_HUF: return getCachedOrFetch(t, base + "HUF", out);
    case ITEM_FX_RON: return getCachedOrFetch(t, base + "RON", out);
    case ITEM_FX_BGN: return getCachedOrFetch(t, base + "BGN", out);
    case ITEM_FX_HRK: return getCachedOrFetch(t, base + "HRK", out);
    case ITEM_FX_ISK: return getCachedOrFetch(t, base + "ISK", out);

    // Asia FX
    case ITEM_GBPCNY: return getCachedOrFetch(t, "GBPCNY", out);
    case ITEM_GBPINR: return getCachedOrFetch(t, "GBPINR", out);
    case ITEM_GBPSGD: return getCachedOrFetch(t, "GBPSGD", out);
    case ITEM_GBPHKD: return getCachedOrFetch(t, "GBPHKD", out);
    case ITEM_GBPTHB: return getCachedOrFetch(t, "GBPTHB", out);
    case ITEM_GBPKRW: return getCachedOrFetch(t, "GBPKRW", out);
    case ITEM_GBPPHP: return getCachedOrFetch(t, "GBPPHP", out);
    case ITEM_FX_CNY: return getCachedOrFetch(t, base + "CNY", out);
    case ITEM_FX_INR: return getCachedOrFetch(t, base + "INR", out);
    case ITEM_FX_SGD: return getCachedOrFetch(t, base + "SGD", out);
    case ITEM_FX_HKD: return getCachedOrFetch(t, base + "HKD", out);
    case ITEM_FX_THB: return getCachedOrFetch(t, base + "THB", out);
    case ITEM_FX_KRW: return getCachedOrFetch(t, base + "KRW", out);
    case ITEM_FX_PHP: return getCachedOrFetch(t, base + "PHP", out);

    // Middle East FX
    case ITEM_GBPAED: return getCachedOrFetch(t, "GBPAED", out);
    case ITEM_GBPSAR: return getCachedOrFetch(t, "GBPSAR", out);
    case ITEM_GBPILS: return getCachedOrFetch(t, "GBPILS", out);
    case ITEM_FX_AED: return getCachedOrFetch(t, base + "AED", out);
    case ITEM_FX_SAR: return getCachedOrFetch(t, base + "SAR", out);
    case ITEM_FX_ILS: return getCachedOrFetch(t, base + "ILS", out);

    // Americas FX
    case ITEM_GBPMXN: return getCachedOrFetch(t, "GBPMXN", out);
    case ITEM_GBPBRL: return getCachedOrFetch(t, "GBPBRL", out);
    case ITEM_FX_MXN: return getCachedOrFetch(t, base + "MXN", out);
    case ITEM_FX_BRL: return getCachedOrFetch(t, base + "BRL", out);

    // Other FX
    case ITEM_GBPZAR: return getCachedOrFetch(t, "GBPZAR", out);
    case ITEM_GBPTRY: return getCachedOrFetch(t, "GBPTRY", out);
    case ITEM_GBPRUB: return getCachedOrFetch(t, "GBPRUB", out);
    case ITEM_FX_ZAR: return getCachedOrFetch(t, base + "ZAR", out);
    case ITEM_FX_TRY: return getCachedOrFetch(t, base + "TRY", out);
    case ITEM_FX_RUB: return getCachedOrFetch(t, base + "RUB", out);

    // Cross-Currency FX
    case ITEM_USDEUR: return getCachedOrFetch(t, "USDEUR", out);
    case ITEM_USDJPY: return getCachedOrFetch(t, "USDJPY", out);
    case ITEM_EURGBP: return getCachedOrFetch(t, "EURGBP", out);
    case ITEM_EURJPY: return getCachedOrFetch(t, "EURJPY", out);

    // USD Fixed
    case ITEM_USDGBP: return getCachedOrFetch(t, "USDGBP", out);
    case ITEM_USDCHF: return getCachedOrFetch(t, "USDCHF", out);
    case ITEM_USDAUD: return getCachedOrFetch(t, "USDAUD", out);
    case ITEM_USDCAD: return getCachedOrFetch(t, "USDCAD", out);
    case ITEM_USDNZD: return getCachedOrFetch(t, "USDNZD", out);
    case ITEM_USDCNY: return getCachedOrFetch(t, "USDCNY", out);
    case ITEM_USDINR: return getCachedOrFetch(t, "USDINR", out);
    case ITEM_USDZAR: return getCachedOrFetch(t, "USDZAR", out);
    case ITEM_USDTRY: return getCachedOrFetch(t, "USDTRY", out);
    case ITEM_USDMXN: return getCachedOrFetch(t, "USDMXN", out);
    case ITEM_USDBRL: return getCachedOrFetch(t, "USDBRL", out);

    // Extras
    case ITEM_PACMAN:
      // Custom char \x80 is defined in setup()
      out = ". . . . . . \x80"; 
      return true;
    case ITEM_WIFI: {
      long rssi = WiFi.RSSI();
      int bars = 0;
      if (rssi > -55) bars = 100;
      else if (rssi > -65) bars = 80;
      else if (rssi > -75) bars = 60;
      else if (rssi > -85) bars = 40;
      else if (rssi > -95) bars = 20;
      out = "WiFi: " + String(rssi) + "dBm (" + String(bars) + "%)";
      return true;
    }

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
  if (settings.magic != 0xA5A5A5A8) {
    // Defaults
    memset(&settings, 0, sizeof(settings));
    settings.magic = 0xA5A5A5A8;

    strcpy(settings.base, "GBP");

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
    settings.selFX_USD = true;
    settings.selFX_EUR = true;
    settings.selFX_JPY = true;
    settings.selFX_CHF = true;
    settings.selFX_AUD = true;
    settings.selFX_CAD = true;
    settings.selFX_NZD = true;

    // Extras default ON
    settings.selPacman = true;
    settings.selWifi = true;

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
  if (var == "BASE_GBP") return String(settings.base) == "GBP" ? "selected" : "";
  if (var == "BASE_USD") return String(settings.base) == "USD" ? "selected" : "";
  if (var == "BASE_EUR") return String(settings.base) == "EUR" ? "selected" : "";

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
  if (var == "FX_USD")   return boolToAttr(settings.selFX_USD);
  if (var == "FX_EUR")   return boolToAttr(settings.selFX_EUR);
  if (var == "FX_JPY")   return boolToAttr(settings.selFX_JPY);
  if (var == "FX_CHF")   return boolToAttr(settings.selFX_CHF);
  if (var == "FX_AUD")   return boolToAttr(settings.selFX_AUD);
  if (var == "FX_CAD")   return boolToAttr(settings.selFX_CAD);
  if (var == "FX_NZD")   return boolToAttr(settings.selFX_NZD);

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
  if (var == "FX_SEK")   return boolToAttr(settings.selFX_SEK);
  if (var == "FX_DKK")   return boolToAttr(settings.selFX_DKK);
  if (var == "FX_NOK")   return boolToAttr(settings.selFX_NOK);
  if (var == "FX_PLN")   return boolToAttr(settings.selFX_PLN);
  if (var == "FX_CZK")   return boolToAttr(settings.selFX_CZK);
  if (var == "FX_HUF")   return boolToAttr(settings.selFX_HUF);
  if (var == "FX_RON")   return boolToAttr(settings.selFX_RON);
  if (var == "FX_BGN")   return boolToAttr(settings.selFX_BGN);
  if (var == "FX_HRK")   return boolToAttr(settings.selFX_HRK);
  if (var == "FX_ISK")   return boolToAttr(settings.selFX_ISK);

  if (var == "GBPCNY")   return boolToAttr(settings.selGBPCNY);
  if (var == "GBPINR")   return boolToAttr(settings.selGBPINR);
  if (var == "GBPSGD")   return boolToAttr(settings.selGBPSGD);
  if (var == "GBPHKD")   return boolToAttr(settings.selGBPHKD);
  if (var == "GBPTHB")   return boolToAttr(settings.selGBPTHB);
  if (var == "GBPKRW")   return boolToAttr(settings.selGBPKRW);
  if (var == "GBPPHP")   return boolToAttr(settings.selGBPPHP);
  if (var == "FX_CNY")   return boolToAttr(settings.selFX_CNY);
  if (var == "FX_INR")   return boolToAttr(settings.selFX_INR);
  if (var == "FX_SGD")   return boolToAttr(settings.selFX_SGD);
  if (var == "FX_HKD")   return boolToAttr(settings.selFX_HKD);
  if (var == "FX_THB")   return boolToAttr(settings.selFX_THB);
  if (var == "FX_KRW")   return boolToAttr(settings.selFX_KRW);
  if (var == "FX_PHP")   return boolToAttr(settings.selFX_PHP);

  if (var == "GBPAED")   return boolToAttr(settings.selGBPAED);
  if (var == "GBPSAR")   return boolToAttr(settings.selGBPSAR);
  if (var == "GBPILS")   return boolToAttr(settings.selGBPILS);
  if (var == "FX_AED")   return boolToAttr(settings.selFX_AED);
  if (var == "FX_SAR")   return boolToAttr(settings.selFX_SAR);
  if (var == "FX_ILS")   return boolToAttr(settings.selFX_ILS);

  if (var == "GBPMXN")   return boolToAttr(settings.selGBPMXN);
  if (var == "GBPBRL")   return boolToAttr(settings.selGBPBRL);
  if (var == "FX_MXN")   return boolToAttr(settings.selFX_MXN);
  if (var == "FX_BRL")   return boolToAttr(settings.selFX_BRL);

  if (var == "GBPZAR")   return boolToAttr(settings.selGBPZAR);
  if (var == "GBPTRY")   return boolToAttr(settings.selGBPTRY);
  if (var == "GBPRUB")   return boolToAttr(settings.selGBPRUB);
  if (var == "FX_ZAR")   return boolToAttr(settings.selFX_ZAR);
  if (var == "FX_TRY")   return boolToAttr(settings.selFX_TRY);
  if (var == "FX_RUB")   return boolToAttr(settings.selFX_RUB);

  if (var == "USDEUR")   return boolToAttr(settings.selUSDEUR);
  if (var == "USDJPY")   return boolToAttr(settings.selUSDJPY);
  if (var == "EURGBP")   return boolToAttr(settings.selEURGBP);
  if (var == "EURJPY")   return boolToAttr(settings.selEURJPY);

  if (var == "USDGBP")   return boolToAttr(settings.selUSDGBP);
  if (var == "USDCHF")   return boolToAttr(settings.selUSDCHF);
  if (var == "USDAUD")   return boolToAttr(settings.selUSDAUD);
  if (var == "USDCAD")   return boolToAttr(settings.selUSDCAD);
  if (var == "USDNZD")   return boolToAttr(settings.selUSDNZD);
  if (var == "USDCNY")   return boolToAttr(settings.selUSDCNY);
  if (var == "USDINR")   return boolToAttr(settings.selUSDINR);
  if (var == "USDZAR")   return boolToAttr(settings.selUSDZAR);
  if (var == "USDTRY")   return boolToAttr(settings.selUSDTRY);
  if (var == "USDMXN")   return boolToAttr(settings.selUSDMXN);
  if (var == "USDBRL")   return boolToAttr(settings.selUSDBRL);

  if (var == "PACMAN")   return boolToAttr(settings.selPacman);
  if (var == "WIFI")     return boolToAttr(settings.selWifi);

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

  page.replace("%USDEUR%",   processor("USDEUR"));
  page.replace("%USDJPY%",   processor("USDJPY"));
  page.replace("%EURGBP%",   processor("EURGBP"));
  page.replace("%EURJPY%",   processor("EURJPY"));

  page.replace("%USDGBP%",   processor("USDGBP"));
  page.replace("%USDCHF%",   processor("USDCHF"));
  page.replace("%USDAUD%",   processor("USDAUD"));
  page.replace("%USDCAD%",   processor("USDCAD"));
  page.replace("%USDNZD%",   processor("USDNZD"));
  page.replace("%USDCNY%",   processor("USDCNY"));
  page.replace("%USDINR%",   processor("USDINR"));
  page.replace("%USDZAR%",   processor("USDZAR"));
  page.replace("%USDTRY%",   processor("USDTRY"));
  page.replace("%USDMXN%",   processor("USDMXN"));
  page.replace("%USDBRL%",   processor("USDBRL"));

  page.replace("%PACMAN%",   processor("PACMAN"));
  page.replace("%WIFI%",     processor("WIFI"));

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
  if (server.hasArg("base")) {
    strncpy(settings.base, server.arg("base").c_str(), 4);
  }

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
  settings.selFX_USD = server.hasArg("fxusd");
  settings.selFX_EUR = server.hasArg("fxeur");
  settings.selFX_JPY = server.hasArg("fxjpy");
  settings.selFX_CHF = server.hasArg("fxchf");
  settings.selFX_AUD = server.hasArg("fxaud");
  settings.selFX_CAD = server.hasArg("fxcad");
  settings.selFX_NZD = server.hasArg("fxnzd");

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
  settings.selFX_SEK = server.hasArg("fxsek");
  settings.selFX_DKK = server.hasArg("fxdkk");
  settings.selFX_NOK = server.hasArg("fxnok");
  settings.selFX_PLN = server.hasArg("fxpln");
  settings.selFX_CZK = server.hasArg("fxczk");
  settings.selFX_HUF = server.hasArg("fxhuf");
  settings.selFX_RON = server.hasArg("fxron");
  settings.selFX_BGN = server.hasArg("fxbgn");
  settings.selFX_HRK = server.hasArg("fxhrk");
  settings.selFX_ISK = server.hasArg("fxisk");

  settings.selGBPCNY = server.hasArg("gbpcny");
  settings.selGBPINR = server.hasArg("gbpinr");
  settings.selGBPSGD = server.hasArg("gbpsgd");
  settings.selGBPHKD = server.hasArg("gbphkd");
  settings.selGBPTHB = server.hasArg("gbpthb");
  settings.selGBPKRW = server.hasArg("gbpkrw");
  settings.selGBPPHP = server.hasArg("gbpphp");
  settings.selFX_CNY = server.hasArg("fxcny");
  settings.selFX_INR = server.hasArg("fxinr");
  settings.selFX_SGD = server.hasArg("fxsgd");
  settings.selFX_HKD = server.hasArg("fxhkd");
  settings.selFX_THB = server.hasArg("fxthb");
  settings.selFX_KRW = server.hasArg("fxkrw");
  settings.selFX_PHP = server.hasArg("fxphp");

  settings.selGBPAED = server.hasArg("gbpaed");
  settings.selGBPSAR = server.hasArg("gbpsar");
  settings.selGBPILS = server.hasArg("gbpils");
  settings.selFX_AED = server.hasArg("fxaed");
  settings.selFX_SAR = server.hasArg("fxsar");
  settings.selFX_ILS = server.hasArg("fxils");

  settings.selGBPMXN = server.hasArg("gbpmxn");
  settings.selGBPBRL = server.hasArg("gbpbrl");
  settings.selFX_MXN = server.hasArg("fxmxn");
  settings.selFX_BRL = server.hasArg("fxbrl");

  settings.selGBPZAR = server.hasArg("gbpzar");
  settings.selGBPTRY = server.hasArg("gbptry");
  settings.selGBPRUB = server.hasArg("gbprub");
  settings.selFX_ZAR = server.hasArg("fxzar");
  settings.selFX_TRY = server.hasArg("fxtry");
  settings.selFX_RUB = server.hasArg("fxrub");

  settings.selUSDEUR = server.hasArg("usdeur");
  settings.selUSDJPY = server.hasArg("usdjpy");
  settings.selEURGBP = server.hasArg("eurgbp");
  settings.selEURJPY = server.hasArg("eurjpy");

  settings.selUSDGBP = server.hasArg("usdgbp");
  settings.selUSDCHF = server.hasArg("usdchf");
  settings.selUSDAUD = server.hasArg("usdaud");
  settings.selUSDCAD = server.hasArg("usdcad");
  settings.selUSDNZD = server.hasArg("usdnzd");
  settings.selUSDCNY = server.hasArg("usdcny");
  settings.selUSDINR = server.hasArg("usdinr");
  settings.selUSDZAR = server.hasArg("usdzar");
  settings.selUSDTRY = server.hasArg("usdtry");
  settings.selUSDMXN = server.hasArg("usdmxn");
  settings.selUSDBRL = server.hasArg("usdbrl");

  settings.selPacman = server.hasArg("pacman");
  settings.selWifi   = server.hasArg("wifi");

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
  wm.autoConnect("G5STO's LED-Matrix-Control");    // Look for this in your WiFi list when fisrt booting.

  timeClient.begin();
  timeClient.setUpdateInterval(60000);    // Waits for 1 minute before giving up

  display.begin();
  
  // Define Pac-Man character (facing left) for code \x80
  uint8_t pacman[8] = { 7, 0x81, 0xC3, 0xE7, 0xFF, 0x7E, 0x7E, 0x3C };
  display.addChar('\x80', pacman);

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
