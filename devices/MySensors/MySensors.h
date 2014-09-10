/**
 * PROTOCOL V1.3
 **/

enum msgTypeV13
{
	PRESENTATION_V13 = 0,
	SET_VARIABLE_V13 = 1,
	REQUEST_VARIABLE_V13 = 2,
	VARIABLE_ACK_V13 = 3,
	INTERNAL_V13 = 4
};

enum deviceTypesV13
{
	S_DOOR_V13 = 0,	// Door and window sensors
	S_MOTION_V13 = 1,	// Motion sensors
	S_SMOKE_V13 = 2,	// Smoke sensor
	S_LIGHT_V13 = 3,	// Light Actuator (on/off)
	S_DIMMER_V13 = 4,	// Dimmable device of some kind
	S_COVER_V13 = 5,	// Window covers or shades
	S_TEMP_V13 = 6,	// Temperature sensor
	S_HUM_V13 = 7,	// Humidity sensor
	S_BARO_V13 = 8,	// Barometer sensor (Pressure)
	S_WIND_V13 = 9,	// Wind sensor
	S_RAIN_V13 = 10,	// Rain sensor
	S_UV_V13 = 11,	// UV sensor
	S_WEIGHT_V13 = 12,	// Weight sensor for scales etc.
	S_POWER_V13 = 13,	// Power measuring device, like power meters
	S_HEATER_V13 = 14,	// Heater device
	S_DISTANCE_V13 = 15,// Distance sensor
	S_LIGHT_LEVEL_V13 =	16,	// Light sensor
	S_ARDUINO_NODE_V13 = 17,	// Arduino node device
	S_ARDUINO_RELAY_V13 = 18,	// Arduino relaying node device
	S_LOCK_V13 = 19,	// Lock device
	S_IR_V13 = 20,	// Ir sender/receiver device
	S_WATER_V13 = 21	// Water meter
};

enum varTypesV13
{
	V_TEMP_V13 = 0, // Temperature
	V_HUM_V13 = 1, // Humidity
	V_LIGHT_V13 = 2, // Light status. 0=off 1=on
	V_DIMMER_V13 = 3, // Dimmer value. 0-100%
	V_PRESSURE_V13 = 4, // Atmospheric Pressure
	V_FORECAST_V13 = 5, // Whether forecast. One of "stable", "sunny", "cloudy", "unstable", "thunderstorm" or "unknown"
	V_RAIN_V13 = 6, // Amount of rain
	V_RAINRATE_V13 = 7, // Rate of rain
	V_WIND_V13 = 8, // Windspeed
	V_GUST_V13 = 9, // Gust
	V_DIRECTION_V13 = 10, // Wind direction
	V_UV_V13 = 11, // UV light level
	V_WEIGHT_V13 = 12, // Weight (for scales etc)
	V_DISTANCE_V13 = 13, // Distance
	V_IMPEDANCE_V13 = 14, // Impedance value
	V_ARMED_V13 = 15, // Armed status of a security sensor. 1=Armed, 0=Bypassed
	V_TRIPPED_V13 = 16, // Tripped status of a security sensor. 1=Tripped, 0=Untripped
	V_WATT_V13 = 17, // Watt value for power meters
	V_KWH_V13 = 18, // Accumulated number of KWH for a power meter
	V_SCENE_ON_V13 = 19, // Turn on a scene
	V_SCENE_OFF_V13 = 20, // Turn of a scene
	V_HEATER_V13 = 21, // Mode of header. One of "Off", "HeatOn", "CoolOn", or "AutoChangeOver"
	V_HEATER_SW_V13 = 22, // Heater switch power. 1=On, 0=Off
	V_LIGHT_LEVEL_V13 = 23, // Light level. 0-100%
	V_VAR1_V13 = 24, // Custom value
	V_VAR2_V13 = 25, // Custom value
	V_VAR3_V13 = 26, // Custom value
	V_VAR4_V13 = 27, // Custom value
	V_VAR5_V13 = 28, // Custom value
	V_UP_V13 = 29, // Window covering. Up.
	V_DOWN_V13 = 30, // Window covering. Down.
	V_STOP_V13 = 31, // Window covering. Stop.
	V_IR_SEND_V13 = 32, // Send out an IR-command
	V_IR_RECEIVE_V13 = 33, // This message contains a received IR-command
	V_FLOW_V13 = 34, // Flow of water (in meter)
	V_VOLUME_V13 = 35, // Water volume
	V_LOCK_STATUS_V13 = 36 // Set or get lock status. 1=Locked, 0=Unlocked
};

enum internalTypesV13
{
	I_BATTERY_LEVEL_V13 = 0, // Use this to report the battery level (in percent 0-100).
	I_TIME_V13 = 3, // Sensors can request the current time from the Controller using this message. The time will be reported as the seconds since 1970
	I_VERSION_V13 = 4, // Sensors report their library version at startup using this message type
	I_REQUEST_ID_V13 = 5, // Use this to request a unique radioId from the Controller.
	I_INCLUSION_MODE_V13 = 6, // Start/stop inclusion mode of the Controller (1=start, 0=stop).
	I_RELAY_NODE_V13 = 7, // When a sensor starts up, it reports it's current relay node (parent node).
	I_PING_V13 = 9, // When a sensor wakes up for the first time, it sends a PING-message to find its neighbors.
	I_PING_ACK_V13 = 10, // This is the reply message sent by relaying nodes and the gateway in reply to PING messages.
	I_LOG_MESSAGE_V13 = 11, // Sent by the gateway to the Controller to trace-log a message
	I_CHILDREN_V13 = 12, // A message that can be used to transfer child sensors (from EEPROM routing table) of a relaying node.
	I_UNIT_V13 = 13, // Sensors can request the units of measure that it should use when reporting sensor data. Payload can be "I" for imperial (Fahrenheit, inch, ...) or "M" for Metric (Celsius, meter, ...)
	I_SKETCH_NAME_V13 = 14, // Optional sketch name that can be used to identify sensor in the Controller GUI
	I_SKETCH_VERSION_V13 = 15 // Optional sketch version that can be reported to keep track of the version of sensor in the Controller GUI.
};

const char* getMsgTypeNameV13(enum msgTypeV13 type);
const char* getDeviceTypeNameV13(enum deviceTypesV13 type);
const char* getVariableTypeNameV13(enum varTypesV13 type);
const char* getInternalTypeNameV13(enum internalTypesV13 type);


/**
 * PROTOCOL V1.4
 **/

enum msgTypeV14
{
    PRESENTATION_V14 = 0,
    SET_V14 = 1,
    REQ_V14 = 2,
    INTERNAL_V14 = 3,
    STREAM_V14 = 4
};

enum deviceTypesV14
{
	S_DOOR_V14 = 0,	// Door and window sensors
	S_MOTION_V14 = 1,	// Motion sensors
	S_SMOKE_V14 = 2,	// Smoke sensor
	S_LIGHT_V14 = 3,	// Light Actuator (on/off)
	S_DIMMER_V14 = 4,	// Dimmable device of some kind
	S_COVER_V14 = 5,	// Window covers or shades
	S_TEMP_V14 = 6,	// Temperature sensor
	S_HUM_V14 = 7,	// Humidity sensor
	S_BARO_V14 = 8,	// Barometer sensor (Pressure)
	S_WIND_V14 = 9,	// Wind sensor
	S_RAIN_V14 = 10,	// Rain sensor
	S_UV_V14 = 11,	// UV sensor
	S_WEIGHT_V14 = 12,	// Weight sensor for scales etc.
	S_POWER_V14 = 13,	// Power measuring device, like power meters
	S_HEATER_V14 = 14,	// Heater device
	S_DISTANCE_V14 = 15,// Distance sensor
	S_LIGHT_LEVEL_V14 =	16,	// Light sensor
	S_ARDUINO_NODE_V14 = 17,	// Arduino node device
	S_ARDUINO_RELAY_V14 = 18,	// Arduino relaying node device
	S_LOCK_V14 = 19,	// Lock device
	S_IR_V14 = 20,	// Ir sender/receiver device
	S_WATER_V14 = 21,	// Water meter
    S_AIR_QUALITY_V14 = 22,  //Air quality sensor e.g. MQ-2
    S_CUSTOM_V14 = 23,  //Use this for custom sensors where no other fits.
    S_DUST_V14 = 24,  //Dust level sensor
    S_SCENE_CONTROLLER_V14 = 25  //Scene controller device
};


enum varTypesV14
{
	V_TEMP_V14 = 0, // Temperature
	V_HUM_V14 = 1, // Humidity
	V_LIGHT_V14 = 2, // Light status. 0=off 1=on
	V_DIMMER_V14 = 3, // Dimmer value. 0-100%
	V_PRESSURE_V14 = 4, // Atmospheric Pressure
	V_FORECAST_V14 = 5, // Whether forecast. One of "stable", "sunny", "cloudy", "unstable", "thunderstorm" or "unknown"
	V_RAIN_V14 = 6, // Amount of rain
	V_RAINRATE_V14 = 7, // Rate of rain
	V_WIND_V14 = 8, // Windspeed
	V_GUST_V14 = 9, // Gust
	V_DIRECTION_V14 = 10, // Wind direction
	V_UV_V14 = 11, // UV light level
	V_WEIGHT_V14 = 12, // Weight (for scales etc)
	V_DISTANCE_V14 = 13, // Distance
	V_IMPEDANCE_V14 = 14, // Impedance value
	V_ARMED_V14 = 15, // Armed status of a security sensor. 1=Armed, 0=Bypassed
	V_TRIPPED_V14 = 16, // Tripped status of a security sensor. 1=Tripped, 0=Untripped
	V_WATT_V14 = 17, // Watt value for power meters
	V_KWH_V14 = 18, // Accumulated number of KWH for a power meter
	V_SCENE_ON_V14 = 19, // Turn on a scene
	V_SCENE_OFF_V14 = 20, // Turn of a scene
	V_HEATER_V14 = 21, // Mode of header. One of "Off", "HeatOn", "CoolOn", or "AutoChangeOver"
	V_HEATER_SW_V14 = 22, // Heater switch power. 1=On, 0=Off
	V_LIGHT_LEVEL_V14 = 23, // Light level. 0-100%
	V_VAR1_V14 = 24, // Custom value
	V_VAR2_V14 = 25, // Custom value
	V_VAR3_V14 = 26, // Custom value
	V_VAR4_V14 = 27, // Custom value
	V_VAR5_V14 = 28, // Custom value
	V_UP_V14 = 29, // Window covering. Up.
	V_DOWN_V14 = 30, // Window covering. Down.
	V_STOP_V14 = 31, // Window covering. Stop.
	V_IR_SEND_V14 = 32, // Send out an IR-command
	V_IR_RECEIVE_V14 = 33, // This message contains a received IR-command
	V_FLOW_V14 = 34, // Flow of water (in meter)
	V_VOLUME_V14 = 35, // Water volume
	V_LOCK_STATUS_V14 = 36, // Set or get lock status. 1=Locked, 0=Unlocked
    V_DUST_LEVEL_V14 = 37,  //Dust level
    V_VOLTAGE_V14 = 38,  //Voltage level
    V_CURRENT_V14 = 39  //Current level
};

enum internalTypesV14
{
    I_BATTERY_LEVEL_V14 = 0,   //Use this to report the battery level (in percent 0-100).
    I_TIME_V14 = 1,   //Sensors can request the current time from the Controller using this message. The time will be reported as the seconds since 1970
    I_VERSION_V14 = 2,   //Sensors report their library version at startup using this message type
    I_ID_REQUEST_V14 = 3,   //Use this to request a unique node id from the controller.
    I_ID_RESPONSE_V14 = 4,   //Id response back to sensor. Payload contains sensor id.
    I_INCLUSION_MODE_V14 = 5,   //Start/stop inclusion mode of the Controller (1=start, 0=stop).
    I_CONFIG_V14 = 6,   //Config request from node. Reply with (M)etric or (I)mperal back to sensor.
    I_FIND_PARENT_V14 = 7,   //When a sensor starts up, it broadcast a search request to all neighbor nodes. They reply with a I_FIND_PARENT_RESPONSE.
    I_FIND_PARENT_RESPONSE_V14 = 8,   //Reply message type to I_FIND_PARENT request.
    I_LOG_MESSAGE_V14 = 9,   //Sent by the gateway to the Controller to trace-log a message
    I_CHILDREN_V14 = 10,  //A message that can be used to transfer child sensors (from EEPROM routing table) of a repeating node.
    I_SKETCH_NAME_V14 = 11,  //Optional sketch name that can be used to identify sensor in the Controller GUI
    I_SKETCH_VERSION_V14 = 12,  //Optional sketch version that can be reported to keep track of the version of sensor in the Controller GUI.
    I_REBOOT_V14 = 13,  //Used by OTA firmware updates. Request for node to reboot.
    I_GATEWAY_READY_V14 = 14  //Send by gateway to controller when startup is complete.
};

const char* getMsgTypeNameV14(enum msgTypeV14 type);
const char* getDeviceTypeNameV14(enum deviceTypesV14 type);
const char* getVariableTypeNameV14(enum varTypesV14 type);
const char* getInternalTypeNameV14(enum internalTypesV14 type);
