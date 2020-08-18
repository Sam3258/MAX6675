/*
  Project 	: A MAX6675 driver for Artisan Coffee Roastor Application

  Purpose	: Normally, the original USB-UART serial port is used for debugging, No more port to communicate with Artisan application.
              This demonstration is to implement ESP32 "SECOND" serial port by Bluetooth Serial-Port-Profile (SPP),
              That is, this project uses Bluetooth SPP to comunicate with Artisan application
             
  Author	: Sam Chen
  Blog 		: https://www.sam4sharing.com
  Youtube	: https://www.youtube.com/channel/UCN7IzeZdi7kl1Egq_a9Tb4g

  History   :
  Date			Author		Ref		Revision
  20200611		Sam			0.01	Initial version
*/

#include <Arduino.h>
#include <WiFi.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <Thermocouple.h>
#include <MAX6675_Thermocouple.h>
#include "U8g2lib.h"
#include "BluetoothSerial.h"

#if !defined(CONFIG_BT_ENABLED) || !defined(CONFIG_BLUEDROID_ENABLED)
#error Bluetooth is not enabled! Please run `make menuconfig` to and enable it
#endif

#ifdef U8X8_HAVE_HW_SPI
#include <SPI.h>
#endif
#ifdef U8X8_HAVE_HW_I2C
#include <Wire.h>
#endif

#define SCK_PIN 14  // D8, GPIO3
#define CS_PIN 	15  // D7, GPIO4
#define SO_PIN 	13  // D6, GPIO5

#define PRINT_TEAMPERATURE_EVERY_SECOND	0	
#define PRINT_ARTISAN_WHOLE_MESSAGE		0

/* 
the default sample rate Artisan is 3 seconds, although the setting value can be modified by user.
I think this value is from experimental value, so change original 10 (1000msx10=10 sec) to 8 (750msx8=6 sec)
*/
#define TEMPERATURE_ARRAY_LENGTH		8	    // first version is 10
#define INTERVEL 						750		// read MAX6675 every "INTERVEL" ms, first version is 1000

bool 			unit_F = false;
float 			tempArray[TEMPERATURE_ARRAY_LENGTH];	// store last times temperature
int				arrayIndex = 0;
bool			firstTen = true;
bool			abnormalValue = false;
float			currentTemp, lastTemp, avgTemp;
float   		last10secTemp, deltaTemp;
unsigned long 	previousMillis = 0;    	// store last time of temperature reading
char 			printBuf[64];			// a buffer for OLED and character transfer
IPAddress       ip;

const char* ssid = "WiFi SSID";	// Replace with your WiFi SSID & Password
const char* password = "SSID Password";

// Create Bluetooth SPP object
BluetoothSerial SerialBT;

// Create object for MAX6675
Thermocouple* thermocouple;

// Create object for SSD1315
U8G2_SSD1306_128X64_NONAME_F_SW_I2C u8g2(U8G2_R0, /* clock=*/ SCL, /* data=*/ SDA, /* reset=*/ U8X8_PIN_NONE);

// Create AsyncWebServer object on port 80
AsyncWebServer server(80);

void callback(esp_spp_cb_event_t event, esp_spp_cb_param_t *param) {

	switch (event) {
		case ESP_SPP_SRV_OPEN_EVT:
//			SerialBT.println("Client Connected");
			break;

		case ESP_SPP_CLOSE_EVT:
//			Serial.println("Client disconnected");
			break;
		default:
//			Serial.print("Unhandle Event: ");
//			Serial.println(event);
			break;
	}
}

// A function to average last 10 times of MAX6675 temperature reading
float averageTemp ( void ) {

	float avg = 0.0;

	for (int i=0; i<=TEMPERATURE_ARRAY_LENGTH; i++){
		avg = avg + tempArray[i];
	}
	avg = avg/TEMPERATURE_ARRAY_LENGTH;

	return avg;
}

// A function to makeup HTML, to show latest and delta temperatures
String makeup(const String& var) {

	String ptr = "  ";

	if(var == "TEMPERATURE") {
		ptr += String(avgTemp).c_str();
	}
	else if(var == "DELTATEMP"){
		ptr += String( deltaTemp*(60000/(TEMPERATURE_ARRAY_LENGTH*INTERVEL)) ).c_str();
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
	ptr +=   String((deltaTemp*(60000/(TEMPERATURE_ARRAY_LENGTH*INTERVEL)))).c_str();
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
    ptr +=   "}, 6000 );";
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
    ptr +=   "}, 6000 );";
    ptr +="</script>";
    ptr +="</html>\n";
    return ptr;
}

void setup() {

	// Initial variables
	last10secTemp = deltaTemp = 0;
	currentTemp = lastTemp = avgTemp = 0;

	Serial.begin(115200);

    // Register SPP callback function
	SerialBT.register_callback(callback);

	// Setup Bluetooth device name as
	if ( !SerialBT.begin("ESP32BTSerial") ) {
		Serial.println("An error occurred during initialize");
	}
	else {
	    Serial.println("ESP32BTSerial is ready for pairing");
		// Use fixed pin code for Legacy Pairing
		char pinCode[5];
		memset(pinCode, 0, sizeof(pinCode));
		pinCode[0] = '1';
  		pinCode[1] = '2';
  		pinCode[2] = '3';
 		pinCode[3] = '4';
		SerialBT.setPin(pinCode); 
	}

	Serial.println("MAX6675 Artisan Driver v0.01");

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
    u8g2.drawStr(0, 12, "Max6675 Artisan Driver");    // print project to buffer
    memset(printBuf, 0, sizeof(printBuf));
	sprintf(printBuf, "%u.%u.%u.%u", ip[0], ip[1], ip[2], ip[3]);
    u8g2.drawStr(32, 36, printBuf);             // print IP Address
    u8g2.sendBuffer();                    		// Trigger buffer string on OLED display internal memory

	// Define WebServer Handles
    server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
    	request->send(200, "text/html", SendHTML());
    });

    server.on("/temperature", HTTP_GET, [](AsyncWebServerRequest *request) {
    	request->send(200, "text/plain", makeup("TEMPERATURE"));
    });

    server.on("/delta-temperature", HTTP_GET, [](AsyncWebServerRequest *request) {
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

void handleArtisanCommands(){   

    if ( Serial.available() ) {
        String msg = Serial.readStringUntil('\n');

#if PRINT_ARTISAN_WHOLE_MESSAGE
		// print whole message
		SerialBT.println(msg);
#endif
		if (msg.indexOf("READ")==0) {	// READ command
/*
	The READ command requests current temperature readings on all active channels. 
	Response from TC4 device is ambient temperature followed by a comma separated list of temperatures in current active units
	The logical channel order is : ambient,chan1,chan2,chan3,chan4
*/	
			Serial.print("0.00,");			// ambient temperature
			Serial.print(avgTemp);			// channel 1 : Environment Temperature (ET); no ET sensor, so uses BT instead					 
    		Serial.print(",");				
			Serial.print(avgTemp);			// channel 2 : Bean Temperature (BT)
    		Serial.println(",0.00,0.00");	// channel 3,4 : A vaule of zero indicates the channel is inactive
			
// The READ command be sent from Artisan every 3 seconds (sample rate), unmark below code carefully
//			SerialBT.println("Artisan \"READ\"");						
        } else if (msg.indexOf("UNITS;")== 0) {	// UNIT command 
            if (msg.substring(6,7)=="F") {   
			    unit_F = true;
                Serial.println("#OK Farenheit");
				SerialBT.println("Artisan \"Farenheit\"");
            }
            else if (msg.substring(6,7)=="C") {  
                unit_F = false;
                Serial.println("#OK Celsius");
				SerialBT.println("Artisan \"Celsius\"");
            }
        } else if (msg.indexOf("CHAN;")== 0) {  // CHAN command
            Serial.print("#OK");
			SerialBT.println("Artisan \"CHAN\"");
        } else if (msg.indexOf("FILT;")== 0) {  // FILT command
            Serial.print("#OK");
			SerialBT.println("Artisan \"FILT\"");
		} else {
			SerialBT.println("Artisan Unhandle command");
			SerialBT.println(msg);
		}
   }
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
				SerialBT.print(" *.*");
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

#if PRINT_TEAMPERATURE_EVERY_SECOND
	    	// print MAX6675 reading value on serial monitor
	    	if (arrayIndex == 0) {
				SerialBT.println(" ");
	    		SerialBT.print("Temperature: ");
	    	}
			
	    	SerialBT.print(" ");
	    	SerialBT.print(tempArray[arrayIndex]);
#endif			
	    	arrayIndex++;

	    	// update temperature on OLED Display
	    	u8g2.clearBuffer();
	    	u8g2.setFont(u8g2_font_ncenB14_tr);
	    	u8g2.drawStr(40, 64, printBuf);
            u8g2.setFont(u8g2_font_ncenB08_tr);   		 
			u8g2.drawStr(0, 12, "Max6675 Artisan Driver");  
            memset(printBuf, 0, sizeof(printBuf));
	        sprintf(printBuf, "%u.%u.%u.%u", ip[0], ip[1], ip[2], ip[3]);
            u8g2.drawStr(32, 36, printBuf);           
	    	u8g2.sendBuffer();

	    	if ( arrayIndex >= TEMPERATURE_ARRAY_LENGTH ) {
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
	    		SerialBT.print("  Avg: ");
	    		SerialBT.print( avgTemp );
	    		// print delta temperature
	    		SerialBT.print("  Delta: ");
		    	SerialBT.println( deltaTemp*(60000/(TEMPERATURE_ARRAY_LENGTH*INTERVEL)) );

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
	// A handle for Artisan application command from
	handleArtisanCommands();
}

