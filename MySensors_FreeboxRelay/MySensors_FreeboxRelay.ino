/*
	MySensors Freebox Relay
	https://github.com/soif/MySensors_CoopFood
	Copyright 2017 François Déchery

	** Description **
	This Arduino ProMini (5V) based project is a [MySensors](https://www.mysensors.org/) node which controls
	the power supply of a Freebox using a relay and also senses nearby devices  temperature.

	** Compilation **
		- needs MySensors version 2.11+
*/

// debug #######################################################################
#define OWN_DEBUG	// Comment/uncomment to remove/show debug (May overflow Arduino memory when set)
#define MY_DEBUG	// Comment/uncomment to remove/show MySensors debug messages (May overflow Arduino memory when set)


// Define ######################################################################
#define INFO_NAME "Freebox Relay"
#define INFO_VERS "2.11.00"

// MySensors
#define MY_RADIO_NRF24
#define MY_NODE_ID 		161

//https://forum.mysensors.org/topic/5778/mys-library-startup-control-after-onregistration/7
#define MY_TRANSPORT_WAIT_READY_MS				(	5*1000ul)		// how long to wait for transport ready at boot
#define MY_TRANSPORT_SANITY_CHECK									// check if transport is available
#define MY_TRANSPORT_SANITY_CHECK_INTERVAL_MS	(15*60*1000ul)		// how often to  check if transport is available (already set as default)
#define MY_TRANSPORT_TIMEOUT_EXT_FAILURE_STATE	(5*	60*1000ul)	//  how often to reconnect if no transport
#define MY_REPEATER_FEATURE										// set as Repeater

#define REPORT_TIME		(	1*60) 	// report sensors every X seconds
#define FORCE_REPORT		10 				// force report every X cycles

#define RELAY_ON		true				// polarity when relay is in NC position
#define RELAY_OFF		false				// polarity when relay is in NO position

#define NUM_TEMP		1					// bumber of temperature sensors DS18B20


#define CHILD_ID_RELAY	0
#define CHILD_ID_TEMP	1

// includes ####################################################################
#include "own_debug.h"

// MySensors
#include <SPI.h>
#include <MySensors.h>

// dallas temperature
#include <OneWire.h>
#include <DallasTemperature.h>

// Pins ########################################################################
#define PIN_RELAY		5		// Relay pin
#define PIN_ONEWIRE		3		// OneWire (DS18B20) Bus


// Variables ###################################################################
boolean				init_msg_sent		=false;			//did we sent the init message?


unsigned long 		next_report 		= 0;		// last temperature report time : start 3s after boot
unsigned int		cycles_count		= 0;
boolean				force_report		= true;		// force report

float				last_temp[NUM_TEMP];

OneWire 			oneWire(PIN_ONEWIRE);
DallasTemperature 	dallas(&oneWire);

MyMessage 			msgRelay	(CHILD_ID_RELAY,	V_STATUS);
MyMessage 			msgTemp		(CHILD_ID_TEMP,V_TEMP);


// #############################################################################
// ## MAIN  ####################################################################
// #############################################################################

// --------------------------------------------------------------------
void before() {
	DEBUG_PRINTLN("");
	DEBUG_PRINTLN("+++++Before START+++++");

	// Setup Pins -----------------------
	pinMode(PIN_RELAY,	OUTPUT);
	digitalWriteFast(PIN_RELAY, RELAY_ON);

	dallas.begin();

	DEBUG_PRINTLN("+++++Before END  +++++");
}

// --------------------------------------------------------------------
void setup() {
	DEBUG_PRINTLN("");
	DEBUG_PRINTLN("----Setup START-------");

	DEBUG_PRINTLN("----Setup END  -------");
}


// --------------------------------------------------------------------
void loop() {
	SendInitialtMsg();

	if(init_msg_sent){
		if( (long) ( millis() - next_report)  >= 0 ){
			next_report +=  (long) REPORT_TIME * 1000 ;
			cycles_count++;

			DEBUG_PRINTLN("");
			DEBUG_PRINTLN("#############################");
			DEBUG_PRINT("# (");
			DEBUG_PRINT(cycles_count);
			DEBUG_PRINT(") ");

			if(cycles_count >= FORCE_REPORT ){
				DEBUG_PRINTLN("Forcing ALL reports !!!");
				force_report=true;
				cycles_count=0;
			}
			else{
				DEBUG_PRINTLN("Reporting changed :");
			}
			//DEBUG_PRINTLN("");

			reportsTemperatures();
			force_report=false;
		}
	}
}


// FUNCTIONS ###################################################################

// --------------------------------------------------------------------
void SendInitialtMsg(){
	if (init_msg_sent == false && isTransportReady() ) {
	   	init_msg_sent = true;
		DEBUG_PRINTLN("");
		DEBUG_PRINTLN("# Transport is Ready");
		DEBUG_PRINTLN("");
   	}
}

// --------------------------------------------------------------------
void presentation(){
	DEBUG_PRINTLN("");
	DEBUG_PRINTLN("*** Presentation START ******");
	sendSketchInfo(INFO_NAME ,	INFO_VERS );
	present(CHILD_ID_RELAY,		S_LIGHT);

	// temperatures sensors
	for (int counter = 0; counter < NUM_TEMP; counter++) {
		present(CHILD_ID_TEMP + counter,		S_TEMP);
	}

	DEBUG_PRINTLN("*** Presentation END ******");
}

// --------------------------------------------------------------------
void receive(const MyMessage &msg){
}



// --------------------------------------------------------------------
void reportsTemperatures(){
	dallas.requestTemperatures();
	wait( dallas.millisToWaitForConversion(dallas.getResolution()) +5 ); // make sure we get the latest temps

	// temperatures sensors
	for (int counter = 0; counter < NUM_TEMP; counter++) {
		float temp = dallas.getTempCByIndex(counter);

		DEBUG_PRINT("# Reading Temperature ");
		DEBUG_PRINT(counter);
		DEBUG_PRINT(" : ");
		DEBUG_PRINT(temp);
		DEBUG_PRINT(" ( last = ");
		DEBUG_PRINT(last_temp[counter]);
		DEBUG_PRINT(" )");

		if (! isnan(temp)) {
			temp = ( (int) (temp * 10 ) ) / 10.0 ; //rounded to 1 dec
			if ( (temp != last_temp[counter] || force_report ) && temp != -127.0 && temp != 85.0) {
				DEBUG_PRINTLN("  ==> Reporting !");
				msgTemp.setSensor(CHILD_ID_TEMP + counter);
				send(msgTemp.set(temp, 1), false);
				last_temp[counter] = temp;
			}
		}
		DEBUG_PRINTLN("");
	}
}
