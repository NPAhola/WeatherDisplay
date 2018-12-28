
#include <time.h>
#include <ESP8266HTTPClient.h>
#include <ESP8266WiFi.h>
#include <ArduinoJson.h>
#include <GxEPD.h>
#include <GxGDE0213B1/GxGDE0213B1.cpp>
#include <GxIO/GxIO_SPI/GxIO_SPI.cpp>
#include <GxIO/GxIO.cpp>
#include <Fonts/FreeMono9pt7b.h>
#include "WDSensitiveData.h"

// Get sensitive data from a header file.
const char* ssid = WIFI_SSID;
const char* password = WIFI_PASS;
String API_KEY = APIKEY;

// SPI initalization.
GxIO_Class io(SPI, SS, D3, D4);
// Adafruit GFX compatible class.
GxEPD_Class display(io);

// Define length of the sleep. Default is 900 s (= 15 min).
#define SLEEP_SECONDS 30*60

// Size of the ArduinoJSON buffer (https://arduinojson.org/assistant/).
// Buffer size must be larger than expected JSON reply.
// 5 day / 3 hour forecast.
//const size_t bufferSize = JSON_ARRAY_SIZE(1) + JSON_ARRAY_SIZE(16) + 32 * JSON_OBJECT_SIZE(1) + JSON_OBJECT_SIZE(2) + 15 * JSON_OBJECT_SIZE(3) + 1140;
// Current weather
const size_t bufferSize = JSON_ARRAY_SIZE(1) + JSON_OBJECT_SIZE(1) + 2*JSON_OBJECT_SIZE(2) + JSON_OBJECT_SIZE(4) + JSON_OBJECT_SIZE(5) + JSON_OBJECT_SIZE(6) + JSON_OBJECT_SIZE(12) + 342;

void printWeatherRow(String type, String value, int idx) {
   /* Prints a type and its value on the display.
    * Current row is given by the index (idx).
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
   
   time_t current_time = time_int;   // UTC time
   struct tm *time_data = localtime(&current_time);

   if (time_data->tm_isdst > 0)
   {
    // Summer time (Daylight Saving Time)
    time_data->tm_hour += 3; // UTC+3
   }
   else
   {
    // Standard time or no info available
    time_data->tm_hour += 2; // UTC+2
   }
   
   char date[20];
   // Specific data can be gotten from struct tm by strftime.
   // See: http://man7.org/linux/man-pages/man3/strftime.3.html
   strftime(date, sizeof(date), "%d.%m.%y%k:%M", time_data); // dd.mm.yyhh:mm, hh:mm will be separated later.

   return date;
}

String convertToCardinal(int dir) {
  /* Converts degrees to cardinal directions (north, northeast etc.).
   * Left side included, right side not included.
   */
  // North     (N)
  if (dir >= 337.5 || dir < 22.5)       return "N";
  // Northeast (NE)
  else if (dir >= 22.5 && dir < 67.5)   return "NE";
  // East      (E)
  else if (dir >= 67.5 && dir < 112.5)  return "E";
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
    // Connect to WiFi.
    WiFi.begin(ssid, password);

    // Option for static IP-address. Connecting to WiFi should
    // decrease by seconds.
    //IPAddress ip(192,168,1,50);
    //IPAddress gateway(192,168,1,1);
    //IPAddress subnet(255,255,255,0);
    //WiFi.config(ip, gateway, subnet);
    
    // WiFi returns WL_CONNECTED when a connection has been established.
    while (WiFi.status() != WL_CONNECTED)
    {
        Serial.println("Connecting to WiFi...");
        delay(250);
    }

    // Creating and sending the HTTP request.

    HTTPClient http; // HTTP-Client initalization.

    // API initalization. Notice that units are metric.
    http.begin("http://api.openweathermap.org/data/2.5/weather?id=634964&units=metric&APPID=" + API_KEY);

    http.addHeader("Content-Type", "application/json"); // Interface requires the request as JSON packet.
    int httpCode = http.GET();  // GET-request.

    String payload;
    if (httpCode > 0)
    {
      payload = http.getString();   // Reply from the API.
    }
    else
    {
      Serial.println("HTTP request error");
    }
       
    http.end();
    
    // ParseObject returns a JsonObject of which data can be easily accessed by []-operator.
    DynamicJsonBuffer jsonBuffer(bufferSize);
    JsonObject &root = jsonBuffer.parseObject(payload.c_str());

     if (!root.success()) {
          Serial.println("Parsing failed");
     }

    // E-paper display initalization.
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
    String wind_speed_str = String(wind_speed,1);

    int wind_direction = root["wind"]["deg"];
    String wind_direction_str = convertToCardinal(wind_direction);
  
    int cloud_percentage = root["clouds"]["all"];
    String cloud_percentage_str = String(cloud_percentage);

    int visibility = root["visibility"];
    String visibility_str = String(visibility);

    JsonArray &weather_array = root["weather"];
    
    printWeatherRow(city_str, "",             0);
    printWeatherRow(current_date_str, "",     1);
    printWeatherRow(current_clock_str, "",    2);
    printWeatherRow("", "",                   3);
    printWeatherRow("Tmp", temp_str + "C",             4);
    printWeatherRow("Prs", pressure_str + "hPa",       5);
    printWeatherRow("Hum", humidity_str + "%",         6);
    printWeatherRow("Wnd", wind_speed_str + "m/s",     7);
    printWeatherRow("", "from " + wind_direction_str,  8);
    printWeatherRow("Cld", cloud_percentage_str + "%", 9);
    printWeatherRow("Vis", visibility_str + "m",       10);
    printWeatherRow("Weather:", "",                    12);
    for (int i = 0; i < weather_array.size(); i++) {
      printWeatherRow(weather_array[i]["main"], "", 13 + i);
    }

    
    // *****************************************************************************

    display.update(); // Updates everything in the buffer to the screen.

    // Set ESP8266 to deepsleep. Upon awakening code starts from the beginning
    // of this function.
    ESP.deepSleep(SLEEP_SECONDS * 1000000);
}

void loop() {
    // loop() is left empty due to deepSleep.
}
