from flask import Flask, Response, request
import requests
from requests.exceptions import RequestException, HTTPError, ConnectionError, Timeout
import threading
import time
import xml.etree.ElementTree as ET
import yfinance as yf
import urllib3

urllib3.disable_warnings(urllib3.exceptions.InsecureRequestWarning)
import logging

app = Flask(__name__)

logging.basicConfig(level=logging.INFO,
                    format='%(asctime)s - %(levelname)s - %(message)s')

HEADERS = {
    "User-Agent": (
        "Mozilla/5.0 (Windows NT 10.0; Win64; x64) "

        "AppleWebKit/537.36 (KHTML, like Gecko) "
        "Chrome/122.0 Safari/537.36"
    )
}

# ---------- RSS FEEDS ----------
RSS_FEEDS = {
    "BBC": "https://feeds.bbci.co.uk/news/rss.xml",
    "BBCUK": "https://feeds.bbci.co.uk/news/uk/rss.xml",
    "SkyNews": "https://feeds.skynews.com/feeds/rss/home.xml",
    "NASA": "https://www.nasa.gov/rss/dyn/breaking_news.rss",
    "TechRadar": "https://www.techradar.com/rss",
    "BBCSport": "https://feeds.bbci.co.uk/sport/rss.xml",
}

FOREX_API_URL = "https://api.forexrateapi.com/v1/latest"
FOREX_API_KEY = "** YOUR FOREXRATEAPI key HERE**"

BLYNK_BASE = "https://**YOUR LEGACY BLYNK IP HERE**:9443/**YOUR LEGACY BLYNK API HERE**/get"
BLYNK_ENDPOINTS = {
    "Temp": "/V0",
    "Humidity": "/V1",
    "Pressure": "/V2",
    "Battery": "/V4",
    "Forecast": "/V7",
}

CRYPTO_API_URL = "https://api.coindesk.com/v1/bpi/currentprice.json"
CRYPTO_API_URL_CG = "https://api.coingecko.com/api/v3/simple/price" # Using CoinGecko for ETH
BRENT_API_URL = "https://www.alphavantage.co/query" # IMPORTANT: Replace with your API key

# All GBP FX keys we care about (will be filled from ForexRate)
FX_CODES = [
    "USD","EUR","JPY","CHF","AUD","CAD","NZD",
    "SEK","DKK","NOK","PLN","CZK","HUF","RON","BGN","HRK","ISK",
    "CNY","INR","SGD","HKD","THB","KRW","PHP",
    "AED","SAR","ILS",
    "MXN","BRL",
    "ZAR","TRY","RUB"
]

# ---------- CACHE ----------
cache = {}

def init_cache():
    global cache
    keys = []

    keys += list(RSS_FEEDS.keys())
    keys += ["Temp","Humidity","Pressure","Battery","Forecast", "DJI", "SPX", "NDX", "FTSE", "N225", "NYSE", "DAX", "CAC", "HSI", "SSEC"]
    keys += ["Brent", "Gold", "Silver", "WTI", "NatGas", "Copper", "BTC", "ETH", "SOL", "XRP", "DOGE"]
    keys += ["F_US500", "F_USTECH", "F_US30"]
    for code in FX_CODES:
        keys.append("GBP" + code)

    cache = {k: {"json": None, "ts": 0} for k in keys}

init_cache()

RSS_INTERVAL = 300
FX_INTERVAL = 300
WEATHER_INTERVAL = 600
CRYPTO_INTERVAL = 60
BRENT_INTERVAL = 300
INDICES_INTERVAL = 300
FUTURES_INTERVAL = 300

last_rss = 0
last_fx = 0
last_weather = 0
last_crypto = 0
last_brent = 0
last_indices = 0
last_futures = 0

def fetch_commodities():
    # 1. Brent Crude via Alpha Vantage (Daily)
    try:
        params = {"function": "BRENT", "interval": "daily", "apikey": "**YOUR ALPHA VANTAGE API HERE**"}
        r = requests.get(BRENT_API_URL, params=params, headers=HEADERS, timeout=10)
        if r.status_code == 200:
            data = r.json()
            if "data" in data and len(data["data"]) > 0:
                price_str = data["data"][0]["value"]
                if price_str == ".": price_str = data["data"][1]["value"]
                title = f"Brent: ${float(price_str):.2f}"
                cache["Brent"]["json"] = '{"status":"ok","items":[{"title":"' + title + '"}]}'
                cache["Brent"]["ts"] = time.time()
                logging.info(f"[CACHE] Updated Brent: {title}")
    except Exception as e:
        logging.error(f"[CACHE] Error fetching Brent: {e}")

    # 2. Gold & Silver via yfinance
    # Symbols: GC=F (Gold), SI=F (Silver), CL=F (WTI), NG=F (NatGas), HG=F (Copper)
    comm_list = [
        ("Gold", "GC=F"), 
        ("Silver", "SI=F"),
        ("WTI", "CL=F"),
        ("NatGas", "NG=F"),
        ("Copper", "HG=F")
    ]
    for name, symbol in comm_list:
        try:
            # Using yfinance for commodities as well for consistency
            ticker = yf.Ticker(symbol)
            hist = ticker.history(period="2d")
            if not hist.empty:
                price = hist["Close"].iloc[-1]
                change_str = ""
                if len(hist) >= 2:
                    prev = hist["Close"].iloc[-2]
                    change = price - prev
                    change_str = f" ({change:+.2f})"
                if price > 0:
                    title = f"{name}: ${price:,.2f}{change_str}"
                    cache[name]["json"] = '{"status":"ok","items":[{"title":"' + title + '"}]}'
                    cache[name]["ts"] = time.time()
                    logging.info(f"[CACHE] Updated {name}: {title}")
        except Exception as e:
            logging.error(f"[CACHE] Error fetching {name}: {e}")

def fetch_rss_xml(name, url):
    try:
        logging.info(f"Fetching RSS XML for {name} from {url}")
        r = requests.get(url, headers=HEADERS, timeout=10)
        r.raise_for_status()
        
        root = ET.fromstring(r.content)
        titles = []
        
        # Try standard RSS <item><title>
        for item in root.findall(".//item")[:3]:
            t = item.find("title")
            if t is not None and t.text:
                titles.append(t.text.strip())
        
        if titles:
            full_text = " | ".join(titles)
            j = '{"status":"ok","items":[{"title":"' + full_text.replace('"', '\\"') + '"}]}'
            cache[name]["json"] = j
            cache[name]["ts"] = time.time()
            logging.info(f"[CACHE] Updated {name}")
        else:
            logging.warning(f"[CACHE] No items found for {name}")
            
    except Exception as e:
        logging.error(f"[CACHE] Error fetching RSS {name}: {e}")


def fetch_forex_rates():
    try:
        logging.info("Fetching FX rates from ForexRateAPI...")
        symbols = ",".join(FX_CODES)
        params = {
            "api_key": FOREX_API_KEY,
            "base": "GBP",
            "currencies": symbols
        }
        r = requests.get(FOREX_API_URL, params=params, headers=HEADERS, timeout=10)
        r.raise_for_status()
        data = r.json()

        if not data.get("success"):
            logging.error(f"[CACHE] ForexRateAPI error: {data.get('error')}")
            return

        rates = data.get("rates", {})
        now = time.time()

        for code in FX_CODES:
            key = "GBP" + code
            if code in rates:
                rate = rates[code]
                title = f"GBP/{code} = {rate:.4f}"
                j = '{"status":"ok","items":[{"title":"' + title + '"}]}'
                cache[key]["json"] = j
                cache[key]["ts"] = now
                logging.info(f"[CACHE] Updated {key}: {title}")
            else:
                logging.warning(f"[CACHE] No rate for {code} in ForexRateAPI")

    except Exception as e:
        logging.error(f"[CACHE] Error fetching FX rates: {e}")


def fetch_blynk_value(name, endpoint):
    url = BLYNK_BASE + endpoint
    try:
        r = requests.get(url, headers=HEADERS, timeout=10, verify=False)
        r.raise_for_status()
        data = r.json()
        if not data:
            raise ValueError("Empty Blynk response")
        val = str(data[0])

        if name == "Temp":
            title = f"Temp: {val} Ã‚Â°C"
        elif name == "Humidity":
            title = f"Humidity: {val} %"
        elif name == "Pressure":
            title = f"Pressure: {val} hPa"
        elif name == "Battery":
            title = f"Battery: {val} V"
        elif name == "Forecast":
            title = f"Forecast: {val}"
        else:
            title = f"{name}: {val}"

        j = '{"status":"ok","items":[{"title":"' + title.replace('"', '\\"') + '"}]}'
        cache[name]["json"] = j
        cache[name]["ts"] = time.time()
        logging.info(f"[CACHE] Updated {name}: {title}")
    except HTTPError as e:
        logging.error(f"[CACHE] HTTP error fetching {name}: {e}")
    except ConnectionError as e:
        logging.error(f"[CACHE] Connection error fetching {name}: {e}")
    except Timeout as e:
        logging.error(f"[CACHE] Timeout error fetching {name}: {e}")
    except Exception as e:
        logging.error(f"[CACHE] An unexpected error occurred while fetching {name}: {e}")

def fetch_crypto():
    try:
        ids = "bitcoin,ethereum,solana,ripple,dogecoin"
        params = {"ids": ids, "vs_currencies": "usd"}
        r = requests.get(CRYPTO_API_URL_CG, params=params, headers=HEADERS, timeout=10)
        r.raise_for_status()
        data = r.json()
        
        mapping = {"bitcoin": "BTC", "ethereum": "ETH", "solana": "SOL", "ripple": "XRP", "dogecoin": "DOGE"}
        
        for id_name, key in mapping.items():
            if id_name in data:
                price = data[id_name]["usd"]
                title = f"{key}: ${price:,.2f}"
                cache[key]["json"] = '{"status":"ok","items":[{"title":"' + title + '"}]}'
                cache[key]["ts"] = time.time()
                logging.info(f"[CACHE] Updated {key}: {title}")
    except Exception as e:
        logging.error(f"[CACHE] ERROR fetching Crypto: {e}")

def fetch_indices():
    try:
        indices = {
            "DJI": "^DJI",
            "SPX": "^GSPC",
            "NDX": "^IXIC",
            "FTSE": "^FTSE",
            "N225": "^N225",
            "NYSE": "^NYA",
            "DAX": "^GDAXI",
            "CAC": "^FCHI",
            "HSI": "^HSI",
            "SSEC": "000001.SS"
        }
        names = {
            "DJI": "Dow",
            "SPX": "S&P 500",
            "NDX": "Nasdaq",
            "FTSE": "FTSE 100",
            "N225": "Nikkei 225",
            "NYSE": "NYSE",
            "DAX": "DAX",
            "CAC": "CAC 40",
            "HSI": "Hang Seng",
            "SSEC": "Shanghai Comp"
        }
        
        for key, symbol in indices.items():
            ticker = yf.Ticker(symbol)
            # Get 2 days of history to calculate change if market is open/closed
            hist = ticker.history(period="2d")
            if not hist.empty:
                price = hist["Close"].iloc[-1]
                change_str = ""
                if len(hist) >= 2:
                    prev = hist["Close"].iloc[-2]
                    change = price - prev
                    change_str = f" ({change:+.2f})"
                title = f"{names[key]}: {price:,.2f}{change_str}"
                cache[key]["json"] = '{"status":"ok","items":[{"title":"' + title + '"}]}'
                cache[key]["ts"] = time.time()
                logging.info(f"[CACHE] Updated {key}: {title}")
    except Exception as e:
        logging.error(f"[CACHE] ERROR fetching indices: {e}")

def fetch_futures():
    try:
        futures = {
            "F_US500": "ES=F",   # S&P 500 Futures
            "F_USTECH": "NQ=F",  # Nasdaq 100 Futures
            "F_US30": "YM=F"     # Dow Jones Futures
        }
        names = {
            "F_US500": "US 500 Fut",
            "F_USTECH": "US Tech 100 Fut",
            "F_US30": "US 30 Fut"
        }
        
        for key, symbol in futures.items():
            ticker = yf.Ticker(symbol)
            hist = ticker.history(period="2d")
            if not hist.empty:
                price = hist["Close"].iloc[-1]
                change_str = ""
                if len(hist) >= 2:
                    prev = hist["Close"].iloc[-2]
                    change = price - prev
                    change_str = f" ({change:+.2f})"
                title = f"{names[key]}: {price:,.2f}{change_str}"
                cache[key]["json"] = '{"status":"ok","items":[{"title":"' + title + '"}]}'
                cache[key]["ts"] = time.time()
                logging.info(f"[CACHE] Updated {key}: {title}")
    except Exception as e:
        logging.error(f"[CACHE] ERROR fetching futures: {e}")

def cache_worker():
    global last_rss, last_fx, last_weather, last_crypto, last_brent, last_indices, last_futures
    while True:
        now = time.time()

        if now - last_rss > RSS_INTERVAL:
            logging.info("[CACHE] Refreshing RSS feeds...")
            for k, u in RSS_FEEDS.items():
                fetch_rss_xml(k, u)
            last_rss = now

        if now - last_fx > FX_INTERVAL:
            logging.info("[CACHE] Refreshing FX feeds...")
            fetch_forex_rates()
            last_fx = now

        if now - last_weather > WEATHER_INTERVAL:
            logging.info("[CACHE] Refreshing weather feeds...")
            for name, ep in BLYNK_ENDPOINTS.items():
                fetch_blynk_value(name, ep)
            last_weather = now
        
        if now - last_crypto > CRYPTO_INTERVAL:
            fetch_crypto()
            last_crypto = now
        
        if now - last_brent > BRENT_INTERVAL:
            fetch_commodities()
            last_brent = now
        
        if now - last_indices > INDICES_INTERVAL:
            logging.info("[CACHE] Refreshing market indices...")
            fetch_indices()
            last_indices = now
        
        if now - last_futures > FUTURES_INTERVAL:
            logging.info("[CACHE] Refreshing futures...")
            fetch_futures()
            last_futures = now

        time.sleep(5)


@app.route("/rss")
def rss():
    cat = request.args.get("category", "BBC")
    if cat not in cache:
        cat = "BBC"

    if cache[cat]["json"] is None:
        logging.info(f"[CACHE] First request for {cat}, fetching immediately...")
        if cat in RSS_FEEDS:
            fetch_rss_xml(cat, RSS_FEEDS[cat])
        elif cat.startswith("GBP"):
            fetch_forex_rates()
        elif cat in BLYNK_ENDPOINTS:
            fetch_blynk_value(cat, BLYNK_ENDPOINTS[cat])
        elif cat in ["BTC", "ETH", "SOL", "XRP", "DOGE"]:
            fetch_crypto()
        elif cat in ["DJI", "SPX", "NDX", "FTSE", "N225", "NYSE", "DAX", "CAC", "HSI", "SSEC"]:
            fetch_indices()
        elif cat in ["Brent", "Gold", "Silver", "WTI", "NatGas", "Copper"]:
            fetch_commodities()
        elif cat in ["F_US500", "F_USTECH", "F_US30"]:
            fetch_futures()

    return Response(cache[cat]["json"], mimetype="application/json")


if __name__ == "__main__":
    t = threading.Thread(target=cache_worker, daemon=True)
    t.start()
    logging.info("[SERVER] Starting info caching proxy on port 5000...")
    app.run(host="0.0.0.0", port=5000)
