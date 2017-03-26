/* #############################################################################
    RENAME THIS FILE TO : config.h
############################################################################# */

// number of DS18B20 temperature sensors USED (in the following array) ---------
#define NUM_SENSORS_USED	1

/* -----------------------------------------------------------------------------
    An array of all temperature sensors used on the OneWire bus.

    Each member of the array is a ow_sensor struct, that contains the following elements:
    - the (MyMessage) Chid ID used when sending/receiving MySensors messages
    - the Address of the DS18B20 sensor (watch the initial startup debug, to discover new addresses)
    - a name (only used for YOU to remember the location of the sensor)
    - the initial temperature. Use -1000

    Example :

    #define NUM_SENSORS_USED 3
    ow_sensor	sensors_used[NUM_SENSORS_USED]={
        //  Child ID,           Address of the DS18B20,                           "Name of the sensor"  ,   initial temperature
    	{	CHILD_ID_TEMP + 0 ,	{0x28, 0x19, 0xF0, 0x4D, 0x05, 0x00, 0x00, 0x3D }, "Room",                -1000},
        {	CHILD_ID_TEMP + 0 ,	{0x28, 0x14, 0xF0, 0x4D, 0x05, 0x00, 0x00, 0x2D }, "Computer 1",          -1000},
        {	CHILD_ID_TEMP + 1 ,	{0x28, 0x0F, 0x2F, 0x4E, 0x05, 0x00, 0x00, 0x6D }, "Computer 2",          -1000}
    };
*/

ow_sensor	sensors_used[NUM_SENSORS_USED]={
	{	CHILD_ID_TEMP + 0 ,	{0x28, 0x19, 0xF0, 0x4D, 0x05, 0x00, 0x00, 0x3D }, "Room",        -1000}
};
