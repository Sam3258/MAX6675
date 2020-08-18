/*
  Project 	: MAX6675 Thermal Monitor by ESP32

  Purpose	: This sketch uses MAX6675 and SSD1315 OLED display to implement
            a WebServer thermal monitor for Quest M3 coffee roaster
  	  	  	Also, this source code is free software and WITHOUT ANY WARRANTY, enjoy!

  Author	: Sam Chen
  Blog 		: https://www.sam4sharing.com
  Youtube	: https://www.youtube.com/channel/UCN7IzeZdi7kl1Egq_a9Tb4g

  History   :
  Date			Author		Ref		Revision
  20200611		Sam			0.00	Initial version
  20200616		Sam			0.01	Add WebServer service
  20200712      Sam         0.02    Change development board from ESP8266 to ESP32 in order to add BT function
*/

#include <Arduino.h>
#include <WiFi.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <Thermocouple.h>
#include <MAX6675_Thermocouple.h>
#include "U8g2lib.h"


#ifdef U8X8_HAVE_HW_SPI
#include <SPI.h>
#endif
#ifdef U8X8_HAVE_HW_I2C
#include <Wire.h>
#endif

#define SCK_PIN 14  // D8, GPIO3
#define CS_PIN 	15  // D7, GPIO4
#define SO_PIN 	13  // D6, GPIO5

#define INTERVEL 	1000				// read MAX6675 every 10 seconds

float 			tempArray[10];			// store last 10 times temperature
int				  arrayIndex = 0;
bool			  firstTen = true;
bool			  abnormalValue = false;
float			  currentTemp, lastTemp, avgTemp;
float   		last10secTemp, deltaTemp;
unsigned long 	previousMillis = 0;    	// store last time of temperature reading
char 			      printBuf[64];			// a buffer for OLED and character transfer
IPAddress       ip;

const char* ssid = "WiFi SSID";	// Replace with your WiFi SSID & Password
const char* password = "Password";

// Create object for MAX6675
Thermocouple* thermocouple;

// Create object for SSD1315
U8G2_SSD1306_128X64_NONAME_F_SW_I2C u8g2(U8G2_R0, /* clock=*/ SCL, /* data=*/ SDA, /* reset=*/ U8X8_PIN_NONE);

// Create AsyncWebServer object on port 80
AsyncWebServer server(80);

// A function to average last 10 times of MAX6675 temperature reading
float averageTemp ( void ) {

	float avg = 0.0;

	for (int i=0; i<=10; i++){
		avg = avg + tempArray[i];
	}
	avg = avg/10;

	return avg;
}

// A function to makeup HTML, to show latest and delta temperatures
String makeup(const String& var) {

	String ptr = "  ";

	if(var == "TEMPERATURE") {
		ptr += String(avgTemp).c_str();
	}
	else if(var == "DELTATEMP"){
		ptr += String((deltaTemp*6)).c_str();
	}

	return ptr;
}

String SendHTML() {

	String ptr = "<!DOCTYPE html>";
	ptr += "<html>";
	ptr += "<head>";
	ptr +=   "<title>MAX6675 Thermal Monitor for Quest M3 coffee roaster</title>";
	ptr +=   "<meta name='viewport' content='width=device-width, initial-scale=1.0'>";
	ptr +=   "<link rel=\"stylesheet\" href=\"https://use.fontawesome.com/releases/v5.7.2/css/all.css\" ";
	ptr +=   "integrity=\"sha384-fnmOCqbTlWIlj8LyTjo7mOUStjsKC4pOpQbqyi7RrhN7udi9RwhKkMHpvLbHG9Sr\" crossorigin=\"anonymous\">";
	ptr += "<style>";
    ptr +=   "html {";
    ptr +=     "font-family: Arial;";
    ptr +=     "display: inline-block;";
    ptr +=     "margin: 0px auto;";
    ptr +=     "text-align: center;";
	ptr +=     "}";
    ptr +=   "h2 { font-size: 3.0rem; }";
    ptr +=   "p { font-size: 3.0rem; }";
    ptr +=   ".units { font-size: 1.2rem; }";
    ptr +=   ".dht-labels{";
    ptr +=     "font-size: 2.0rem;";
    ptr +=     "vertical-align:middle;";
    ptr +=     "padding-bottom: 15px;";
	ptr +=     "}";
    ptr += "</style>";
	ptr += "</head>";

	ptr += "<body>";
	ptr += "<h2>MAX6675 Thermal Monitor</h2>";
	ptr += "<p>";
	ptr +=   "<span class=\"dht-labels\"> Temperature </span>";
	ptr += "</p>";
	ptr += "<p>";
	ptr +=   "<i class=\"fas fa-thermometer-half\" style=\"color:#059e8a;\"></i>";
	ptr +=   "<span id=\"temperature\">  ";
	ptr +=   String(avgTemp).c_str();
	ptr +=   "</span>";
	ptr +=   "<sup class=\"units\">&deg;C</sup>";
	ptr += "</p>";
	ptr += "<p>";
	ptr +=   "<span class=\"dht-labels\">Delta Temperature </span>";
	ptr += "</p>";
	ptr += "<p>";
	ptr +=   "<i class=\"fas fa-exclamation-triangle\" style=\"color:#00add6;\"></i>";
	ptr +=   "<span id=\"delta-temperature\">  ";
	ptr +=   String((deltaTemp*6)).c_str();
	ptr +=   "</span>";
	ptr +=   "<sup class=\"units\">&deg;C</sup>";
	ptr += "</p>";
	ptr += "</body>";

    ptr += "<script>";
    ptr += "setInterval(function ( ) {";
    ptr +=   "var xhttp = new XMLHttpRequest();";
    ptr +=   "xhttp.onreadystatechange = function() {";
    ptr +=     "if (this.readyState == 4 && this.status == 200) {";
    ptr +=       "document.getElementById(\"temperature\").innerHTML = this.responseText;";
	ptr +=     "}";
	ptr +=   "};";
    ptr +=   "xhttp.open(\"GET\", \"/temperature\", true);";
	ptr +=   "xhttp.send();";
    ptr +=   "}, 10000 );";
    ptr +="</script>";

    ptr += "<script>";
    ptr += "setInterval(function ( ) {";
    ptr +=   "var xhttp = new XMLHttpRequest();";
    ptr +=   "xhttp.onreadystatechange = function() {";
    ptr +=     "if (this.readyState == 4 && this.status == 200) {";
    ptr +=       "document.getElementById(\"delta-temperature\").innerHTML = this.responseText;";
  	ptr +=     "}";
   	ptr +=   "};";
    ptr +=   "xhttp.open(\"GET\", \"/delta-temperature\", true);";
   	ptr +=   "xhttp.send();";
    ptr +=   "}, 10000 );";
    ptr +="</script>";
    ptr +="</html>\n";
    return ptr;
}

void setup() {

	// Initial variables
	last10secTemp = deltaTemp = 0;
	currentTemp = lastTemp = avgTemp = 0;

	Serial.begin(115200);
	Serial.println("MAX6675 Thermal Monitor v0.01");

	// Start WiFi
	Serial.print("WiFi is connecting to \"");
	Serial.print(ssid);
	Serial.println("\"");
	WiFi.begin(ssid, password);

	// Create object for MAX6675
	thermocouple = new MAX6675_Thermocouple(SCK_PIN, CS_PIN, SO_PIN);

	// Start SSD1315 OLED Display
	u8g2.begin();

	// Check WiFi status
	while (WiFi.status() != WL_CONNECTED) {
		delay(100);
		Serial.print(".");
	}

	// Print local IP address
	Serial.println("");
	Serial.print("WiFi connected and IP Address: ");
    ip = WiFi.localIP();
	Serial.println(ip);

	u8g2.clearBuffer();                   		// clear display the internal memory
	u8g2.setFont(u8g2_font_ncenB08_tr);   		// choose font
    u8g2.drawStr(12, 12, "Thermal Monitor");    // write string to buffer
    memset(printBuf, 0, sizeof(printBuf));
	sprintf(printBuf, "%u.%u.%u.%u", ip[0], ip[1], ip[2], ip[3]);
    u8g2.drawStr(32, 36, printBuf);
    u8g2.sendBuffer();                    		// transfer buffer string to display internal memory to show-up

	// Define WebServer Handles
    server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
    	Serial.println("Web root");
    	request->send(200, "text/html", SendHTML());
    });

    server.on("/temperature", HTTP_GET, [](AsyncWebServerRequest *request) {
    	Serial.print(" Web Temperature Update :");
    	Serial.print(avgTemp);
    	Serial.print(", ");
    	request->send(200, "text/plain", makeup("TEMPERATURE"));
    });

    server.on("/delta-temperature", HTTP_GET, [](AsyncWebServerRequest *request) {
        Serial.print("Web Delta Temperature Update :");
        Serial.print((deltaTemp*6));
        Serial.print(", ");
        request->send(200, "text/plain", makeup("DELTATEMP"));
    });

	server.onNotFound([](AsyncWebServerRequest *request) {
    	request->send(404, "text/plain", "Not found");
    });

    // start WebServer
    server.begin();
    Serial.println("Start WebServer");

    previousMillis = millis();					// store current time as previousMillis
}

void loop() {

	unsigned long currentMillis = millis();		// get current time

	// For the MAX6675 to update, 250ms delay is AT LEAST !
	// Here, we set 1000ms (1 second) to reading per time
	if ( currentMillis - previousMillis >= INTERVEL ) {
	    // means 1 second pass, read new temperature
	    previousMillis = currentMillis;

	    if ( !firstTen ) {
	    	currentTemp = thermocouple->readCelsius();
	    	avgTemp = averageTemp();
	    	// 進豆溫降會大於10度C
	    	// 所以只過濾異常溫升放掉溫降
	    	if (currentTemp < (lastTemp + 10)) {
	    		// compare with last temperature
	    		tempArray[arrayIndex] = lastTemp = currentTemp;
	    	}
	    	else {
	    		// set abnormal flag
	    		abnormalValue = true;
	    		// print *.* instead of abnormal value
	    		Serial.print(" *.*");
	    	}
	    }
	    else {
	    	// just read current temperature in "first 10" reading
	    	tempArray[arrayIndex] = lastTemp = thermocouple->readCelsius();
	    }

	    if ( !abnormalValue ) {
	    	// Normal temperature will into this loop
	    	memset(printBuf, 0, sizeof(printBuf));
	    	if ( firstTen ) {
	    		// in "first 10", print tempArray value directly
	    		snprintf(printBuf, sizeof(printBuf)-1, "%3.2f", tempArray[arrayIndex]);
	    	}
	    	else {
	    		// do an average, if not "first 10"
	    		avgTemp = averageTemp();
	    		snprintf(printBuf, sizeof(printBuf)-1, "%3.2f", avgTemp);
	    	}

	    	// print MAX6675 reading value on serial monitor
	    	if (arrayIndex == 0) {
	    		Serial.println(" ");
	    		Serial.print("Temperature: ");
	    	}
	    	Serial.print(" ");
	    	Serial.print(tempArray[arrayIndex]);
	    	arrayIndex++;

	    	// update temperature on OLED Display
	    	u8g2.clearBuffer();
	    	u8g2.setFont(u8g2_font_ncenB14_tr);
	    	u8g2.drawStr(40, 64, printBuf);
            u8g2.setFont(u8g2_font_ncenB08_tr);   		
            u8g2.drawStr(12, 12, "Thermal Monitor");  
            memset(printBuf, 0, sizeof(printBuf));
	        sprintf(printBuf, "%u.%u.%u.%u", ip[0], ip[1], ip[2], ip[3]);
            u8g2.drawStr(32, 36, printBuf);           
	    	u8g2.sendBuffer();

	    	if ( arrayIndex >= 10 ) {
	    		// force change index to 0
	    		arrayIndex = 0;
	    		// 10 seconds pass, update delta temperature in 10 seconds
	    		deltaTemp = avgTemp - last10secTemp;
	    		// avoid "zero" operation
	    		if (deltaTemp == 0.0)
	    			deltaTemp = 0.01;
	    		// update last 10 seconds temperature
	    		last10secTemp = avgTemp;

	    		// print average and delta temperature on serial monitor
	    		Serial.print("  Avg: ");
	    		Serial.print( avgTemp );
	    		// print delta temperature in 1 minute
	    		Serial.print("  Delta: ");
		    	Serial.print( deltaTemp*6 );

	    		if ( firstTen ) {
	    			// assign first average temperature
	    			last10secTemp = averageTemp();
	    			firstTen = false;
	    		}
	    	}
	    }
	    else {
	    	// After bypass abnormal value, reset flag here
	    	abnormalValue = false;
	    }
	}
}

