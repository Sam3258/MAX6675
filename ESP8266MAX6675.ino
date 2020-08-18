/*
  Project 	: MAX6675 Thermal Monitor

  Purpose	: This sketch uses MAX6675 and SSD1315 OLED display
  	  	  to implement thermal monitor for Quest M3 coffee roaster
  	  	  Also, this source code is free software and WITHOUT ANY WARRANTY, enjoy!

  Author	: Sam Chen
  Blog 		: https://www.sam4sharing.com
  Youtube	: https://www.youtube.com/channel/UCN7IzeZdi7kl1Egq_a9Tb4g

  History   :
  Date			Author		Ref		Revision
  20200611		Sam			0.00	Initial version
*/

#include <Arduino.h>
#include <Thermocouple.h>
#include <MAX6675_Thermocouple.h>
#include "U8g2lib.h"


#ifdef U8X8_HAVE_HW_SPI
#include <SPI.h>
#endif
#ifdef U8X8_HAVE_HW_I2C
#include <Wire.h>
#endif

#define SCK_PIN D8	// GPIO3
#define CS_PIN 	D7	// GPIO4
#define SO_PIN 	D6	// GPIO5

float 	tempArray[10];				// store last 10 times temperature
int     arrayIndex = 0;
bool	firstTen = true;
bool	abnormalValue = false;
float	currentTemp, lastTemp, avgTemp;
float   last10secTemp, deltaTemp;
char 	printBuf[40];

unsigned long previousMillis = 0;    // store last time of temperature reading

// Updates DHT readings every 10 seconds
const long interval = 10000;

Thermocouple* thermocouple;

// Create object for SSD1315
U8G2_SSD1306_128X64_NONAME_F_SW_I2C u8g2(U8G2_R0, /* clock=*/ SCL, /* data=*/ SDA, /* reset=*/ U8X8_PIN_NONE);

// do an average for last 10 temperature
float averageTemp ( void ) {

	float avg = 0.0;

	for (int i=0; i<=10; i++){
		avg = avg + tempArray[i];
	}
	avg = avg/10;

	return avg;
}

void setup() {

	// Initial variables
	last10secTemp = deltaTemp = 0;
	currentTemp = lastTemp = avgTemp = 0;

	Serial.begin(115200);
	Serial.println("MAX6675 Thermal Monitor v0.00");

	// Create object for MAX6675
	thermocouple = new MAX6675_Thermocouple(SCK_PIN, CS_PIN, SO_PIN);

	// Start SSD1315 OLED Display
	u8g2.begin();
	delay(100);

	u8g2.clearBuffer();                   		// clear the internal memory
	u8g2.setFont(u8g2_font_ncenB08_tr);   		// choose font
    u8g2.drawStr(0, 16, "   Thermal Monitor");  // write something to the internal memory
    u8g2.sendBuffer();                    		// transfer internal memory to the display

    previousMillis = millis();					// store current time as previousMillis
}

void loop() {

	unsigned long currentMillis = millis();		// get current time

	// For the MAX6675 to update, 250ms delay is AT LEAST !
	// Here, we set 1000ms (1 second) to reading per time
	if ( currentMillis - previousMillis >= 1000 ) {
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
	    	u8g2.setFont(u8g2_font_ncenB08_tr);
	    	u8g2.drawStr(0, 16, "   Thermal Monitor ");
	    	u8g2.setFont(u8g2_font_ncenB14_tr);
	    	u8g2.drawStr(40, 48, printBuf);
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
#if 0
	else {
		// How much time does pass ?
		Serial.print(currentMillis - previousMillis);
		Serial.print("ms PASSED !");
	}
#endif
}
