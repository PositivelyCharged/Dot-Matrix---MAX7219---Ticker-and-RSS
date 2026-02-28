This project uses a Parola‑based MAX7219 8×8 LED matrix to display market‑related information. With the appropriate APIs, the display can show useful real‑time indicators such as FX movements, commodities, indices, crypto prices, and curated RSS headlines. The project integrates ForexRateAPI as the primary source for paid, licensed FX updates.

Data handling and architecture
Scraping RSS feeds directly from an Arduino‑class microcontroller can trigger bot‑protection systems and lead to HTTP 429 rate‑limit errors. To avoid this, all data requests are routed through a Raspberry Pi running a Python proxy service. This approach:

Converts RSS → JSON or JSON → JSON into Arduino‑friendly formats

Allows caching and rate‑limiting to stay within API quotas

Reduces load on the microcontroller

Ensures stable, predictable data delivery

Helps avoid accidental violations of provider terms of service

Some free API tiers allow only around 100 requests per month, so caching and controlled polling are essential.

Why ForexRateAPI is used
A scalable, legally licensed FX provider was required for this project. ForexRateAPI offers affordable paid plans with higher update frequency, making it suitable for minute‑level FX data. Multiple API keys can be used to access tailored RSS feeds and market data throughout the trading day.

The display currently focuses on GBP pairs, with more currencies planned. The WebUI tick‑boxes allow quick switching between data categories such as sports, metals, indices, crypto, and political news.

Legal and ethical use of market data
Foreign‑exchange market data is typically licensed, and most providers restrict automated scraping or high‑frequency retrieval unless you have a paid plan that permits it. These restrictions usually apply even for personal, non‑commercial use.

To remain compliant with data providers:

Use only sources whose terms of service allow automated access

Respect rate limits and licensing conditions

Do not modify the code to bypass access controls or obtain data at a frequency not permitted by the provider

Do not redistribute or repackage data unless the provider explicitly allows it

Attempting to circumvent these restrictions may violate the provider’s terms and could have legal consequences. This project is intended to be used responsibly and within the limits defined by each data source. If it becomes clear that forks of this project are being used to obtain FX data in ways that breach provider terms, the repository may be removed.

ForexRateAPI is inexpensive, reliable, and offers scalable plans for users who need more frequent updates. Users are encouraged to review their website and choose a plan that fits their needs.
