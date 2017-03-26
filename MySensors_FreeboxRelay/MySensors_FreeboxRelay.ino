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
#define INFO_NAME "FreeboxRelay"
#define INFO_VERS "2.11.00"

// MySensors
#define MY_RADIO_NRF24
#define MY_NODE_ID 		161

//https://forum.mysensors.org/topic/5778/mys-library-startup-control-after-onregistration/7
#define MY_TRANSPORT_WAIT_READY_MS				(	5*1000ul)		// how long to wait for transport ready at boot
#define MY_TRANSPORT_SANITY_CHECK									// check if transport is available
#define MY_TRANSPORT_SANITY_CHECK_INTERVAL_MS	(15*60*1000ul)		// how often to  check if transport is available (already set as default)
#define MY_TRANSPORT_TIMEOUT_EXT_FAILURE_STATE	(5*	60*1000ul)	//  how often to reconnect if no transport
//#define MY_REPEATER_FEATURE										// set as Repeater

#define REPORT_TIME			(2*60) 				// report sensors every X seconds
#define FORCE_REPORT		10 				// force report ALL every X cycles

#define FBX_TIME_REBOOT		(40+60)		// prevent another reboot command before this time (seconds) elapsed
#define FBX_TIME_FIRMWARE	(10*60)		// prevent another firmware command before this time (seconds) elapsed

#define FBX_DUR_ON		(1500)			// duration for relay ON
#define FBX_DUR_OFF		(500)			// duration for relay OFF

#define RELAY_ON		true				// polarity when relay is in NC position
#define RELAY_OFF		false				// polarity when relay is in NO position

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
unsigned int		cycles_count		= 0;		// cycles performed
boolean				force_report		= true;		// force report
unsigned long 		next_report 		= 0;		// last temperature report time : start 3s after boot

unsigned long		next_reboot			=0;			// next time reboot is allowed
boolean				force_reboot		=false;		// allow bypass reboot mask time
byte				current_mode		=1;			// 0=off, 1=on, 2=reboot,3=firmware

boolean				init_msg_sent		=false;		// did we sent the init message?
byte				sensors_count		=0;			// number of DS18B20 connected

OneWire 			owBus(PIN_ONEWIRE);
DallasTemperature 	owTempBus(&owBus);

MyMessage 			msgRelay	(CHILD_ID_RELAY,	V_PERCENTAGE);
MyMessage 			msgTemp		(CHILD_ID_TEMP,		V_TEMP);

struct ow_sensor {
	byte child_id;
	DeviceAddress address;
	String name;
	float last_temp;
};


// User Config #################################################################

// rename "config.default.h" to "config.h", and fill it with your own sensor addresses
#if __has_include("config.h")
#include "config.h"
#else
#include "config.default.h"
#endif


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

	//dallas begin
	owTempBus.begin();
	sensors_count=owTempBus.getDeviceCount();

	listAllOwSensors();

	DEBUG_PRINTLN("+++++Before END  +++++");
	DEBUG_PRINTLN("");
}

// --------------------------------------------------------------------
void setup() {
	DEBUG_PRINTLN("");
	DEBUG_PRINTLN("----Setup START-------");
	DEBUG_PRINTLN("----Setup END  -------");
	DEBUG_PRINTLN("");
}


// --------------------------------------------------------------------
void loop() {
	SendInitialtMsg();

	if(init_msg_sent){
		//report temperatures
		if( (long) ( millis() - next_report)  >= 0 ){
			next_report +=  (long) REPORT_TIME * 1000 ;
			cycles_count++;

			DEBUG_PRINT("### (");
			DEBUG_PRINT(cycles_count);
			DEBUG_PRINT(") ");

			if(cycles_count >= FORCE_REPORT ){
				DEBUG_PRINT("Forcing ALL reports !!! ");
				reportsMode(current_mode);
				force_report=true;
				cycles_count=0;
			}
			else{
				DEBUG_PRINT("Reporting changed ");
			}
			DEBUG_PRINTLN("#############################");

			reportsTemperatures();
			force_report=false;
			//DEBUG_PRINTLN("");
		}
		//reset mode when done
		if( ( (long) ( millis() - next_reboot)  >= 0) && current_mode > 1 ){
			DEBUG_PRINTLN("# Reporting End of Locked time: back to mode 1 ###########");
			reportsMode(1);
			current_mode=1;
			DEBUG_PRINTLN("");
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

	present(CHILD_ID_RELAY,		S_DIMMER);

	// temperatures sensors
	for (int i = 0; i < NUM_SENSORS_USED; i++) {
		present(CHILD_ID_TEMP + i,		S_TEMP);
	}
	DEBUG_PRINTLN("*** Presentation END ******");
	DEBUG_PRINTLN("");
}

// --------------------------------------------------------------------
void receive(const MyMessage &message){
	if (message.isAck() ) {
		DEBUG_PRINTLN(" <-- ACK from gateway IGNORED !");
		DEBUG_PRINTLN("");
		return;
	}

	if (message.type == V_PERCENTAGE) {
		DEBUG_PRINT("#  <-- Receiving mode : ");
		DEBUG_PRINTLN(message.getByte());
		switchMode(message.getByte());
		DEBUG_PRINTLN("");
   }
}

// --------------------------------------------------------------------
void switchMode(byte mode){
	DEBUG_PRINT("# Switching mode : ");
	DEBUG_PRINTLN(mode);

	boolean ok;
	if(mode == 0){
		ok=freeboxOff();
	}
	else if(mode == 1){
		ok=freeboxOn();
	}
	else if(mode == 2){
		ok=freeboxRebootOne();
	}
	else if(mode == 3){
		ok=freeboxFirmware();
	}

	if(ok){
		reportsMode(mode);
	}
}

// --------------------------------------------------------------------
boolean freeboxReboot(byte count=1, unsigned long mask=0){
	unsigned long time_remaining=0;
	//http://playground.arduino.cc/Code/TimingRollover
	if( (long) ( millis() - next_reboot)  >= 0 ){
		next_reboot += millis() + mask ;
	}
	else{
		time_remaining=( next_reboot - millis() ) /1000;
	}

	DEBUG_PRINT("# Rebooting: count=");
	DEBUG_PRINT(count);
	DEBUG_PRINT(", remaining=");
	DEBUG_PRINT(time_remaining);
	DEBUG_PRINT(" sec. ");

	if( (time_remaining > 0 ) && !force_reboot){
		DEBUG_PRINTLN(" => CANCELED !");
		return false;
	}
	DEBUG_PRINT(" => ");

	for (int i = 0; i < count; i++) {
		DEBUG_PRINT(i+1);
		//set OFF
		digitalWrite(PIN_RELAY, RELAY_OFF);
		wait(FBX_DUR_OFF);
		DEBUG_PRINT(". ");
		//sef ON
		digitalWrite(PIN_RELAY, RELAY_ON);
		if(count > 1){
			wait(FBX_DUR_ON);
		}
	}
	DEBUG_PRINTLN("  REBOOTED!");
	return true;
}

// --------------------------------------------------------------------
boolean freeboxOff(){
	if(current_mode > 1){
		unsigned long time_remaining=( next_reboot - millis() ) /1000;
		DEBUG_PRINT("# Currently BUSY doing mode : ");
		DEBUG_PRINT(current_mode);
		DEBUG_PRINT(". Please wait ");
		DEBUG_PRINT(time_remaining);
		DEBUG_PRINTLN(" sec. , then try again!");
		return false;
	}
	current_mode = 0;
	digitalWrite(PIN_RELAY, RELAY_OFF);
	return true;
}

// --------------------------------------------------------------------
boolean freeboxOn(){
	current_mode = 1;
	digitalWrite(PIN_RELAY, RELAY_ON);
	return true;
}

// --------------------------------------------------------------------
boolean freeboxRebootOne(){
	if( freeboxReboot(1, FBX_TIME_REBOOT * 1000ul) ){
		current_mode=2;
		return true;
	}
	return false;
}

// --------------------------------------------------------------------
boolean freeboxFirmware(){
	if( freeboxReboot(5, FBX_TIME_FIRMWARE * 1000ul) ){
		current_mode=3;
		return true;
	}
	return false;
}

// --------------------------------------------------------------------
void reportsMode(unsigned int mode){
	send(msgRelay.set(mode), true);
}

// --------------------------------------------------------------------
void reportsTemperatures(){
	owTempBus.requestTemperatures();
	wait( owTempBus.millisToWaitForConversion(owTempBus.getResolution()) +5 ); // make sure we get the latest temps

	// temperatures sensors
	for (byte i = 0; i < NUM_SENSORS_USED; i++) {

		DEBUG_PRINT("# - Sensor ");
		DEBUG_PRINT(sensors_used[i].child_id);
		DEBUG_PRINT(" : ");
		float temp = formatTemperature(owTempBus.getTempC(sensors_used[i].address));
		if(temp==999){
			DEBUG_PRINT("[ERR]");
		}
		else{
			DEBUG_PRINT(temp);
		}
		DEBUG_PRINT(" ( last = ");
		DEBUG_PRINT(sensors_used[i].last_temp);
		DEBUG_PRINT(" )");

		if ( temp !=999 && (temp != sensors_used[i].last_temp || force_report ) ) {
				DEBUG_PRINTLN("  --> Reporting :");
				msgTemp.setSensor(sensors_used[i].child_id);
				send(msgTemp.set(temp, 1), false);
				sensors_used[i].last_temp = temp;
		}
		else{
			DEBUG_PRINTLN("");
		}
		DEBUG_PRINTLN("");
	}
}

// --------------------------------------------------------------------
float formatTemperature(float temp){
	if (! isnan(temp)) {
		temp = ( (int) (temp * 10 ) ) / 10.0 ; //rounded to 1 dec
		if ( temp != -127.0 && temp != 85.0) {
			return temp;
		}
	}
	return 999;	//error
}

// --------------------------------------------------------------------
void listAllOwSensors(){
	//DEBUG_PRINTLN("");
	DEBUG_PRINT("# Found ");
	DEBUG_PRINTDEC(sensors_count);
	DEBUG_PRINTLN(" sensors :");

	owTempBus.requestTemperatures();
	delay( owTempBus.millisToWaitForConversion(owTempBus.getResolution()) +5 ); // make sure we get the latest temps

	if(sensors_count > 0){
		DeviceAddress id;
		for (byte i = 0; i < sensors_count; i++) {
			owTempBus.getAddress(id, i);
			DEBUG_PRINT(" - [ ");
			DEBUG_PRINT(i);
			DEBUG_PRINT(" ] , ");
			printAddress(id);
			DEBUG_PRINT(" : Temp= ");
			float temp = formatTemperature(owTempBus.getTempC(id));
			if(temp !=999){
				DEBUG_PRINT(temp);
			}
			else{
				DEBUG_PRINT("ERR");
			}

			DEBUG_PRINT(" ==> ");
			ow_sensor s= selectOwSensor(id);
			if(s.child_id !=0){
				DEBUG_PRINT("Child ");
				DEBUG_PRINT(s.child_id);
				DEBUG_PRINT(" , ");
				DEBUG_PRINT(s.name);
			}
			else{
				DEBUG_PRINT("......... UNUSED !");
			}

			DEBUG_PRINTLN("");
		}
	}
}

// --------------------------------------------------------------------
ow_sensor selectOwSensor(DeviceAddress address){
	for (byte i = 0; i < NUM_SENSORS_USED; i++) {
		if(equalsDeviceAddress(sensors_used[i].address, address)){
			return sensors_used[i];
		}
	}
	ow_sensor s={0, "0",""};
	return s;
}

// --------------------------------------------------------------------
void printAddress(DeviceAddress deviceAddress){
	for (byte i = 0; i < 8; i++) {
		// zero pad the address if necessary
		if (deviceAddress[i] < 16) {
			DEBUG_PRINT("0");
		}
		DEBUG_PRINTHEX(deviceAddress[i]);
	}
	DEBUG_PRINT(" { ");
	for (byte i = 0; i < 8; i++) {
		DEBUG_PRINT("0x");
		// zero pad the address if necessary
		if (deviceAddress[i] < 16){
			DEBUG_PRINT("0");
		}
		DEBUG_PRINTHEX(deviceAddress[i]);
		if( i < 7){
			DEBUG_PRINT(", ");
		}
	}
	DEBUG_PRINT(" }");
}

// --------------------------------------------------------------------
boolean equalsDeviceAddress(DeviceAddress a, DeviceAddress b){
	for (byte i = 0; i < 8; i++) {
		if(a[i] != b[i]){
			return false;
		}
	}
	return true;
}
