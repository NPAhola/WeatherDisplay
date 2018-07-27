

#include <time.h>
#include <ESP8266HTTPClient.h>
#include <ESP8266WiFi.h>
#include <ArduinoJson.h>
#include <GxEPD.h>
#include <GxGDE0213B1/GxGDE0213B1.cpp>
#include <GxIO/GxIO_SPI/GxIO_SPI.cpp>
#include <GxIO/GxIO.cpp>
#include <Fonts/FreeMono9pt7b.h>
#include <string>
#include "WDSensitiveData.h"

const char* ssid = WIFI_SSID;
const char* password = WIFI_PASS;
String API_KEY = APIKEY;

// E-Paper display initalization
GxIO_Class io(SPI, SS, D3, D4);
GxEPD_Class display(io);

// Define length of the sleep. Default is 900 s (= 15 min).
#define SLEEP_SECONDS 90

// HSL:n pysäkki-id:t voi hakea menemällä osoitteeseen
// https://www.hsl.fi/reitit-ja-aikataulut,
// kirjoittamalla pysäkkihakuun pysäkin nimen, ja
// kopioimalla osoitepalkista pysäkin tunnisteen,
// joka on muotoa HSL:<numerosarja>.

// Koko Suomen kattavia pysäkkitunnisteita voi hakea
// samasta rajapinnasta käyttämällä linkistä
// https://goo.gl/cwAC1H löytyvää kyselyä.

// GraphQL-pyyntö Digitransitin rajapintaan. Kokeile rajapintaa täällä: goo.gl/cwAC1H
//static const char digitransitQuery[] PROGMEM = "{\"query\":\"{stops(ids:[\\\"HSL:2215255\\\"]){name,stoptimesWithoutPatterns(numberOfDepartures:17){realtimeDeparture,realtime,trip{route{shortName}}}}}\"}";
// Query to Openwathermap interface.
//static const char weatherQuery[] = "{\"query\":\"{main\"}";

// ArduinoJSON-puskurin koko. Ks. https://arduinojson.org/assistant/
// Puskurin on oltava suurempi kuin oletettu JSON-vastaus rajapinnasta.
//const size_t bufferSize = JSON_ARRAY_SIZE(1) + JSON_ARRAY_SIZE(16) + 32 * JSON_OBJECT_SIZE(1) + JSON_OBJECT_SIZE(2) + 15 * JSON_OBJECT_SIZE(3) + 1140;
const size_t bufferSize = JSON_ARRAY_SIZE(1) + JSON_OBJECT_SIZE(1) + 2*JSON_OBJECT_SIZE(2) + JSON_OBJECT_SIZE(4) + JSON_OBJECT_SIZE(5) + JSON_OBJECT_SIZE(6) + JSON_OBJECT_SIZE(12) + 342;

void printTimetableRow(String busName, String departure, bool isRealtime, int idx) {
    /* Funktio tulostaa näytön puskuriin bussiaikataulurivin. Esim.
       110T  21:34~
    */
    display.setCursor(2, 2 + idx * 14);
    display.print(busName);
    display.setCursor(54, 2 + idx * 14);
    display.print(departure);
    if (isRealtime)
    {
        display.setCursor(108, 2 + idx * 14);
        display.print("~");
    }
}

void printWeatherRow(String type, String value, int idx) {
    /* 
    */
    display.setCursor(2, 10 + idx * 14);
    display.print(type);
    display.setCursor(44, 10 + idx * 14);
    display.print(value);

}

String timeToString(int time_int) {
  /* Transforms time given in unix UTC time to a string containing current
   * day, month, year, hour and minute.
   */
   time_t current_time = time_int + 3*60*60;  // From UTC to UTC+3
   struct tm *tm = localtime(&current_time);
   char date[20];
   strftime(date, sizeof(date), "%d.%m.%y%k:%M", tm); // dd.mm.yyhh:mm, hh:mm will be separated later

   return date;
}

void setValuesToScreen(JsonObject &root) {
  /* Sets all the necessary info to the E-Display.
   */

}

String parseTime(int seconds) {
    /* Funktio parsii Digitransitin sekuntimuotoisesta lähtöajasta
       merkkijonon. Esimerkiksi 78840 -> "21:54" 
    */
    int hours = seconds / 3600;
    int minutes = (seconds % 3600) / 60;
    char buffer[5];
    if (hours == 25)
        hours = 0;
    sprintf(buffer, "%02d:%02d", hours, minutes);
    return buffer;
}

String convertToCardinal(int dir) {
  /* Converts degrees to cardinal directions (north, northeast etc.).
   * Left side included, right side not included.
   */
  // North     (N)
  if (dir >= 337.5 || dir < 22.5) return "N";
  // Northeast (NE)
  else if (dir >= 22.5 && dir < 67.5) return "NE";
  // East      (E)
  else if (dir >= 67.5 && dir < 112.5) return "E";
  // Southeast (SE)
  else if (dir >= 112.5 && dir < 157.5) return "SE";
  // South     (S)
  else if (dir >= 157.5 && dir < 202.5) return "S";
  // Southwest (SW)
  else if (dir >= 202.5 && dir < 247.5) return "SW";
  // West      (W)
  else if (dir >= 247.5 && dir < 292.5) return "W";
  // Northwest (NW)
  else return "NW";
}

void setup()
{
    // Open serial port at 9600 bps.
    Serial.begin(9600);
    Serial.println("wifia");
    // Note: One should always create a specific file for sensitive data such as
    // SSIDs and passwords. However I accidentally pushed the data into Github
    // already and Github happens to cache the data, so easy fixes such as 'git reset'
    // wouldn't actually remove the data from the cache. Hence I just changed the password
    // afterwards and let the old one be here.
    WiFi.begin(ssid, password);

    // Voit myös asettaa itsellesi staattisen IP:n
    // säästääksesi akkua. Tämä lyhentää Wifi-verkkoon yhdistämistä
    // usealla sekunnilla.
    //IPAddress ip(192,168,1,50);
    //IPAddress gateway(192,168,1,1);
    //IPAddress subnet(255,255,255,0);
    //WiFi.config(ip, gateway, subnet);

    Serial.println("setupissa");
    
    // Yhdistetään langattomaan verkkoon
    while (WiFi.status() != WL_CONNECTED)
    {
        Serial.println("connecting...");
        delay(250);
    }

    /* Seuraavilla riveillä luodaan ja lähetetään HTTP-pyyntö Digitransitin rajapintaan */

    HTTPClient http; // Alustetaan HTTP-Client -instanssi

    // Huomaa kaksi vaihtoehtoista osoitetta Digitransitin rajapintoihin,
    // koko Suomen haku, ja HSL:n haku.
    //http.begin("http://api.digitransit.fi/routing/v1/routers/hsl/index/graphql"); // <- HSL
    //http.begin("http://api.digitransit.fi/routing/v1/routers/finland/index/graphql"); // <- koko Suomi
    // API init. Notice that units are metric.
    http.begin("http://api.openweathermap.org/data/2.5/weather?id=634964&units=metric&APPID=" + API_KEY);

    http.addHeader("Content-Type", "application/json"); // Rajapinta vaatii pyynnön JSON-pakettina
    //int httpCode = http.POST(weatherQuery);             // POST-muotoinen pyyntö
    int httpCode = http.GET();

    String payload;
    if (httpCode > 0)
    {
      payload = http.getString();                  // Otetaan Digitransitin lähettämä vastaus talteen muuttujaan 'payload'
      Serial.println(payload);
    }
    else
    {
      Serial.println("HTTP request error");
    }
       
    http.end();

    // Test print.
    //Serial.println(payload);

    

    // Parsitaan vastaus helpomminkäsiteltävään muotoon
    DynamicJsonBuffer jsonBuffer(bufferSize);
    JsonObject &root = jsonBuffer.parseObject(payload.c_str());

    // otetaan referenssi JSON-muotoisen vastauksen bussilähdöistä 'departures'
    //JsonArray &departures = root["data"]["stops"][0]["stoptimesWithoutPatterns"];
    //JsonObject &main = root["main"];

    // Hyödylliset rivit debuggaukseen:
     if (!root.success()) {
          Serial.println("Parsing failed");
     }

    //int pressure = main["pressure"];    
    //Serial.println(pressure);

    // Alustetaan E-paperinäyttö
    display.init();
    display.setTextColor(GxEPD_BLACK);
    display.setFont(&FreeMono9pt7b);

    
    // *****************************************************************************
    // This section sets all the necessary values to the E-Display.
    String city_str = root["name"];
  
    int current_time = root["dt"];
    String current_time_str = timeToString(current_time);
    String current_date_str = current_time_str.substring(0,8);
    String current_clock_str = current_time_str.substring(8);
    
    int temp = root["main"]["temp"];
    String temp_str = String(temp);
  
    int pressure = root["main"]["pressure"];
    String pressure_str = String(pressure);
  
    int humidity = root["main"]["humidity"];
    String humidity_str = String(humidity);
  
    double wind_speed = root["wind"]["speed"];
    String wind_speed_str = "";
    wind_speed_str = String(wind_speed,1);

    int wind_direction = root["wind"]["deg"];
    Serial.println(wind_direction);
    String wind_direction_str = convertToCardinal(wind_direction);
  
    int cloud_percentage = root["clouds"]["all"];
    String cloud_percentage_str = String(cloud_percentage);

    int visibility = root["visibility"];
    String visibility_str = String(visibility);

    JsonArray &weather_array = root["weather"];
    for (int i = 0; i < weather_array.size(); i++) {
      printWeatherRow(weather_array[i]["main"], "", 13 + i);
    }
  
    printWeatherRow(city_str, "",             0);
    printWeatherRow(current_date_str, "",     1);
    printWeatherRow(current_clock_str, "",    2);
    printWeatherRow("", "",                   3);
    printWeatherRow("Tmp", temp_str + "C",             4);
    printWeatherRow("Prs", pressure_str + "hPa",       5);
    printWeatherRow("Hum", humidity_str + "%",         6);
    printWeatherRow("Wnd", wind_speed_str + "m/s",     7);
    printWeatherRow("", "from " + wind_direction_str,            8);
    printWeatherRow("Cld", cloud_percentage_str + "%", 9);
    printWeatherRow("Vis", visibility_str + "m",       10);
    printWeatherRow("Weather:", "",                    12);

    

    // *****************************************************************************

    
    
    
    /*
    // Käydään kaikki bussilähdöt yksitellen läpi.
    // Jokainen bussilähtö piirretään e-paperinäytön puskuriin.
    int idx = 0;
    for (auto &dep : departures)
    {
        int departureTime = dep["realtimeDeparture"]; // lähtöaika
        String departure = parseTime(departureTime);  // parsittu lähtöaika
        bool realTime = dep["realtime"]; // onko lähtö tarkka (käyttääkö HSL:n GPS-seurantaa?)
        String busName = dep["trip"]["route"]["shortName"]; // Bussin reittinumero
        printTimetableRow(busName, departure, realTime, ++idx); // tulostetaan rivi näytölle oikeaan kohtaan
    }
    */
    

    display.update(); // Piirrä näyttöpuskurin sisältö E-paperinäytölle

    // Komennetaan ESP8266 syväunitilaan.
    // Herättyään koodi suoritetaan setup()-funktion alusta
    Serial.println("deepsleep");
    ESP.deepSleep(SLEEP_SECONDS * 1000000);

    
}

void loop() {
    // loop() is left empty due to deepSleep.
}
