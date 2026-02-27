Dot‑Matrix MAX7219 Ticker and RSS 

This project uses a Parola‑based MAX7219 8×8 LED matrix to display market‑related information. With the appropriate APIs, the display can show useful real‑time indicators such as FX movements, commodities, indices, crypto prices, and curated RSS headlines. The project integrates ForexRateAPI as the primary source for paid, legal, live FX updates.

Data handling and architecture
Scraping RSS feeds directly from an Arduino‑class microcontroller can trigger bot‑protection systems and lead to HTTP 429 rate‑limit errors. To avoid this, all data requests are routed through a Raspberry Pi running a Python proxy service. This approach provides several benefits:

Converts RSS → JSON or JSON → JSON into Arduino‑friendly formats

Allows caching and rate‑limiting to stay within API quotas

Reduces load on the microcontroller

Ensures stable, predictable data delivery

Avoids accidental ToS violations from excessive requests

Some free API tiers allow only ~100 requests per month, so caching and controlled polling are essential.

Why ForexRateAPI is used
Early in the project it became clear that a scalable, legally licensed FX provider was required. ForexRateAPI offers affordable paid plans with higher update frequency, making it a reliable source for minute‑level FX data. Multiple API keys can be used to access tailored RSS feeds and market data throughout the trading day.

The display currently focuses on GBP pairs, with more currencies planned. The WebUI tick‑boxes allow quick switching between data categories such as sports, metals, indices, crypto, and political news.

Legal and ethical use of market data
Important:  
Foreign‑exchange market data is licensed. Most providers prohibit scraping or automated retrieval more frequently than once per day unless you pay for a plan that allows it. This applies even for personal, non‑commercial use.

To stay compliant:

Use only data sources whose terms of service allow automated access

Respect rate limits and licensing restrictions

Do not modify the code to bypass access controls or extract data at a frequency not permitted by the provider

Do not redistribute or repackage data obtained from paid APIs unless the license explicitly allows it

Attempting to circumvent these restrictions is unethical and may have legal consequences. If it becomes clear that forks of this project are being used to obtain FX data in violation of provider terms, the repository may be removed.

ForexRateAPI is inexpensive, reliable, and offers scalable plans for users who need more frequent updates. Users are encouraged to review their website and choose a plan that fits their needs.
