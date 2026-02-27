# Dot-Matrix---MAX7219---Ticker-and-RSS
A Dot matrix display based on the Parola MAX7219 8x8 LED controller. This will display with the appropriate API's produce valuable data for keeping an eye on market changes. This project incorporates ForesRateAPI as a fundamental for "paid live" FX updates.

It is important to note that just scraping RSS feeds may trigger a BOT warning. To keep the load away from the Arduino and stop potential error 429 issues we channel the data through a Raspberry Pi running a Python script. This way we can offload the RSS <> JSON or JSON <> JSON etc. We can also Parse some data to something more readible to the Arduino ForesRateAPI display. As the code also allows the user to cache certain elements and slow down the per request and daily API calls. Some free accounts are limit to around 100 per month.

  Therefor it was decided early on that having a service that has scalability such as ForesRateAPI and that would enhance the frequency of the update and thus become an indensible tool for monitoring changing rates. The code has several API keys that enable taylored RSS feeds that can act switfly to grab the best data throughout the trading day.

   Of course the code can display other pairing (GPB <> ??? at the moment but will add more later and the convenient tick boxes allow quick changes between data. Be it sports, metal prices, indicies or live policitcal data.

  IMPORTANT - It is illegal to scrape or scavange FX data more frequenct than once per day without paying for it even if it is for personal use. So long as you are using this code for personal use then you must also abide with the license agreements from the feed supplier.
  Please DO NOT attempt to alter or modify the code to circumvent data acquisition outside of the agreements in line with the website or service supplying that data. it is highly unethical and any redistribution or extractionof more frequent data could have legal ramfications. I reserve the right to delete the code if it appears that forks of my work have been used to in effect "STEAL" FX data. 
  
  ForexRateAPI is cheap and offers excellent scability and I highly recommend users check their website for paid services.
