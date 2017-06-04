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

/**
 * PROTOCOL V1.5
 **/

enum msgTypeV15
{
    PRESENTATION_V15 = 0,
    SET_V15 = 1,
    REQ_V15 = 2,
    INTERNAL_V15 = 3,
    STREAM_V15 = 4
};

enum deviceTypesV15
{
    S_DOOR_V15 = 0, //Door and window sensors
    S_MOTION_V15 = 1,   //Motion sensors
    S_SMOKE_V15 = 2,    //Smoke sensor
    //DEPRECATED S_LIGHT_V15 = 3,    //Light Actuator (on/off)
    S_BINARY_V15 = 3,   //Binary device (on/off), Alias for S_LIGHT
    S_DIMMER_V15 = 4,   //Dimmable device of some kind
    S_COVER_V15 = 5,    //Window covers or shades
    S_TEMP_V15 = 6, //Temperature sensor
    S_HUM_V15 = 7,  //Humidity sensor
    S_BARO_V15 = 8, //Barometer sensor (Pressure)
    S_WIND_V15 = 9, //Wind sensor
    S_RAIN_V15 = 10,    //Rain sensor
    S_UV_V15 = 11,  //UV sensor
    S_WEIGHT_V15 = 12,  //Weight sensor for scales etc.
    S_POWER_V15 = 13,   //Power measuring device, like power meters
    S_HEATER_V15 = 14,  //Heater device
    S_DISTANCE_V15 = 15,    //Distance sensor
    S_LIGHT_LEVEL_V15 = 16, //Light sensor
    S_ARDUINO_NODE_V15 = 17,    //Arduino node device
    S_ARDUINO_REPEATER_NODE_V15 = 18,   //Arduino repeating node device
    S_LOCK_V15 = 19,    //Lock device
    S_IR_V15 = 20,  //Ir sender/receiver device
    S_WATER_V15 = 21,   //Water meter
    S_AIR_QUALITY_V15 = 22, //Air quality sensor e.g. MQ-2
    S_CUSTOM_V15 = 23,  //Use this for custom sensors where no other fits.
    S_DUST_V15 = 24,    //Dust level sensor
    S_SCENE_CONTROLLER_V15 = 25,    //Scene controller device
    S_RGB_LIGHT_V15 = 26,   //RGB light
    S_RGBW_LIGHT_V15 = 27,  //RGBW light (with separate white component)
    S_COLOR_SENSOR_V15 = 28,    //Color sensor
    S_HVAC_V15 = 29,    //Thermostat/HVAC device
    S_MULTIMETER_V15 = 30,  //Multimeter device
    S_SPRINKLER_V15 = 31,   //Sprinkler device
    S_WATER_LEAK_V15 = 32,  //Water leak sensor
    S_SOUND_V15 = 33,   //Sound sensor
    S_VIBRATION_V15 = 34,   //Vibration sensor
    S_MOISTURE_V15 = 35 //Moisture sensor
};


enum varTypesV15
{
    V_TEMP_V15 = 0, //Temperature
    V_HUM_V15 = 1,  //Humidity
    V_STATUS_V15 = 2,   //Binary status. 0=off 1=on
    //DEPRECATED V_LIGHT_V15 = 2,   //Deprecated. Alias for V_STATUS. Light status. 0=off 1=on
    V_PERCENTAGE_V15 = 3,   //Percentage value. 0-100 (%)
    //DEPRECATED V_DIMMER_V15 = 3,  //Deprecated. Alias for V_PERCENTAGE. Dimmer value. 0-100 (%)
    V_PRESSURE_V15 = 4, //Atmospheric Pressure
    V_FORECAST_V15 = 5, //Whether forecast. One of "stable", "sunny", "cloudy", "unstable", "thunderstorm" or "unknown"
    V_RAIN_V15 = 6, //Amount of rain
    V_RAINRATE_V15 = 7, //Rate of rain
    V_WIND_V15 = 8, //Windspeed
    V_GUST_V15 = 9, //Gust
    V_DIRECTION_V15 = 10,   //Wind direction
    V_UV_V15 = 11,  //UV light level
    V_WEIGHT_V15 = 12,  //Weight (for scales etc)
    V_DISTANCE_V15 = 13,    //Distance
    V_IMPEDANCE_V15 = 14,   //Impedance value
    V_ARMED_V15 = 15,   //Armed status of a security sensor. 1=Armed, 0=Bypassed
    V_TRIPPED_V15 = 16, //Tripped status of a security sensor. 1=Tripped, 0=Untripped
    V_WATT_V15 = 17,    //Watt value for power meters
    V_KWH_V15 = 18, //Accumulated number of KWH for a power meter
    V_SCENE_ON_V15 = 19,    //Turn on a scene
    V_SCENE_OFF_V15 = 20,   //Turn of a scene
    V_HVAC_FLOW_STATE_V15 = 21, //Mode of header. One of "Off", "HeatOn", "CoolOn", or "AutoChangeOver"
    V_HVAC_SPEED_V15 = 22,  //HVAC/Heater fan speed ("Min", "Normal", "Max", "Auto")
    V_LIGHT_LEVEL_V15 = 23, //Uncalibrated light level. 0-100%. Use V_LEVEL for light level in lux.
    V_VAR1_V15 = 24,    //Custom value
    V_VAR2_V15 = 25,    //Custom value
    V_VAR3_V15 = 26,    //Custom value
    V_VAR4_V15 = 27,    //Custom value
    V_VAR5_V15 = 28,    //Custom value
    V_UP_V15 = 29,  //Window covering. Up.
    V_DOWN_V15 = 30,    //Window covering. Down.
    V_STOP_V15 = 31,    //Window covering. Stop.
    V_IR_SEND_V15 = 32, //Send out an IR-command
    V_IR_RECEIVE_V15 = 33,  //This message contains a received IR-command
    V_FLOW_V15 = 34,    //Flow of water (in meter)
    V_VOLUME_V15 = 35,  //Water volume
    V_LOCK_STATUS_V15 = 36, //Set or get lock status. 1=Locked, 0=Unlocked
    V_LEVEL_V15 = 37,   //Used for sending level-value
    V_VOLTAGE_V15 = 38, //Voltage level
    V_CURRENT_V15 = 39, //Current level
    V_RGB_V15 = 40, //RGB value transmitted as ASCII hex string (I.e "ff0000" for red)
    V_RGBW_V15 = 41,    //RGBW value transmitted as ASCII hex string (I.e "ff0000ff" for red + full white)
    V_ID_V15 = 42,  //Optional unique sensor id (e.g. OneWire DS1820b ids)
    V_UNIT_PREFIX_V15 = 43, //Allows sensors to send in a string representing the unit prefix to be displayed in GUI. This is not parsed by controller! E.g. cm, m, km, inch.
    V_HVAC_SETPOINT_COOL_V15 = 44,  //HVAC cold setpoint (Integer between 0-100)
    V_HVAC_SETPOINT_HEAT_V15 = 45,  //HVAC/Heater setpoint (Integer between 0-100)
    V_HVAC_FLOW_MODE_V15 = 46   //Flow mode for HVAC ("Auto", "ContinuousOn", "PeriodicOn")
};

enum internalTypesV15
{
    I_BATTERY_LEVEL_V15 = 0,    //Use this to report the battery level (in percent 0-100).
    I_TIME_V15 = 1, //Sensors can request the current time from the Controller using this message. The time will be reported as the seconds since 1970
    I_VERSION_V15 = 2,  //Used to request gateway version from controller.
    I_ID_REQUEST_V15 = 3,   //Use this to request a unique node id from the controller.
    I_ID_RESPONSE_V15 = 4,  //Id response back to sensor. Payload contains sensor id.
    I_INCLUSION_MODE_V15 = 5,   //Start/stop inclusion mode of the Controller (1=start, 0=stop).
    I_CONFIG_V15 = 6,   //Config request from node. Reply with (M)etric or (I)mperal back to sensor.
    I_FIND_PARENT_V15 = 7,  //When a sensor starts up, it broadcast a search request to all neighbor nodes. They reply with a I_FIND_PARENT_RESPONSE.
    I_FIND_PARENT_RESPONSE_V15 = 8, //Reply message type to I_FIND_PARENT request.
    I_LOG_MESSAGE_V15 = 9,  //Sent by the gateway to the Controller to trace-log a message
    I_CHILDREN_V15 = 10,    //A message that can be used to transfer child sensors (from EEPROM routing table) of a repeating node.
    I_SKETCH_NAME_V15 = 11, //Optional sketch name that can be used to identify sensor in the Controller GUI
    I_SKETCH_VERSION_V15 = 12,  //Optional sketch version that can be reported to keep track of the version of sensor in the Controller GUI.
    I_REBOOT_V15 = 13,  //Used by OTA firmware updates. Request for node to reboot.
    I_GATEWAY_READY_V15 = 14,   //Send by gateway to controller when startup is complete.
    I_REQUEST_SIGNING_V15 = 15, //Used between sensors when initialting signing.
    I_GET_NONCE_V15 = 16,   //Used between sensors when requesting nonce.
    I_GET_NONCE_RESPONSE_V15 = 17   //Used between sensors for nonce response.
};

const char* getMsgTypeNameV15(enum msgTypeV15 type);
const char* getDeviceTypeNameV15(enum deviceTypesV15 type);
const char* getVariableTypeNameV15(enum varTypesV15 type);
const char* getInternalTypeNameV15(enum internalTypesV15 type);


/**
 * PROTOCOL V2.0
 **/

enum msgTypeV20
{
    PRESENTATION_V20 = 0,
    SET_V20 = 1,
    REQ_V20 = 2,
    INTERNAL_V20 = 3,
    STREAM_V20 = 4
};

enum deviceTypesV20
{
    S_DOOR_V20 = 0, //Door and window sensors
    S_MOTION_V20 = 1,   //Motion sensors
    S_SMOKE_V20 = 2,    //Smoke sensor
    S_BINARY_V20 = 3,   //Binary device (on/off), Alias for S_LIGHT
    S_DIMMER_V20 = 4,   //Dimmable device of some kind
    S_COVER_V20 = 5,    //Window covers or shades
    S_TEMP_V20 = 6, //Temperature sensor
    S_HUM_V20 = 7,  //Humidity sensor
    S_BARO_V20 = 8, //Barometer sensor (Pressure)
    S_WIND_V20 = 9, //Wind sensor
    S_RAIN_V20 = 10,    //Rain sensor
    S_UV_V20 = 11,  //UV sensor
    S_WEIGHT_V20 = 12,  //Weight sensor for scales etc.
    S_POWER_V20 = 13,   //Power measuring device, like power meters
    S_HEATER_V20 = 14,  //Heater device
    S_DISTANCE_V20 = 15,    //Distance sensor
    S_LIGHT_LEVEL_V20 = 16, //Light sensor
    S_ARDUINO_NODE_V20 = 17,    //Arduino node device
    S_ARDUINO_REPEATER_NODE_V20 = 18,   //Arduino repeating node device
    S_LOCK_V20 = 19,    //Lock device
    S_IR_V20 = 20,  //Ir sender/receiver device
    S_WATER_V20 = 21,   //Water meter
    S_AIR_QUALITY_V20 = 22, //Air quality sensor e.g. MQ-2
    S_CUSTOM_V20 = 23,  //Use this for custom sensors where no other fits.
    S_DUST_V20 = 24,    //Dust level sensor
    S_SCENE_CONTROLLER_V20 = 25,    //Scene controller device
    S_RGB_LIGHT_V20 = 26,   //RGB light
    S_RGBW_LIGHT_V20 = 27,  //RGBW light (with separate white component)
    S_COLOR_SENSOR_V20 = 28,    //Color sensor
    S_HVAC_V20 = 29,    //Thermostat/HVAC device
    S_MULTIMETER_V20 = 30,  //Multimeter device
    S_SPRINKLER_V20 = 31,   //Sprinkler device
    S_WATER_LEAK_V20 = 32,  //Water leak sensor
    S_SOUND_V20 = 33,   //Sound sensor
    S_VIBRATION_V20 = 34,   //Vibration sensor
    S_MOISTURE_V20 = 35, //Moisture sensor
    S_INFO_V20 = 36, //LCD text device
    S_GAS_V20 = 37, //Gas meter
    S_GPS_V20 = 38, //GPS Sensor
    S_WATER_QUALITY_V20 = 39 //Water quality sensor
};

enum varTypesV20
{
    V_TEMP_V20 = 0, //Temperature
    V_HUM_V20 = 1,  //Humidity
    V_STATUS_V20 = 2,   //Binary status. 0=off 1=on
    V_PERCENTAGE_V20 = 3,   //Percentage value. 0-100 (%)
    V_PRESSURE_V20 = 4, //Atmospheric Pressure
    V_FORECAST_V20 = 5, //Whether forecast. One of "stable", "sunny", "cloudy", "unstable", "thunderstorm" or "unknown"
    V_RAIN_V20 = 6, //Amount of rain
    V_RAINRATE_V20 = 7, //Rate of rain
    V_WIND_V20 = 8, //Windspeed
    V_GUST_V20 = 9, //Gust
    V_DIRECTION_V20 = 10,   //Wind direction
    V_UV_V20 = 11,  //UV light level
    V_WEIGHT_V20 = 12,  //Weight (for scales etc)
    V_DISTANCE_V20 = 13,    //Distance
    V_IMPEDANCE_V20 = 14,   //Impedance value
    V_ARMED_V20 = 15,   //Armed status of a security sensor. 1=Armed, 0=Bypassed
    V_TRIPPED_V20 = 16, //Tripped status of a security sensor. 1=Tripped, 0=Untripped
    V_WATT_V20 = 17,    //Watt value for power meters
    V_KWH_V20 = 18, //Accumulated number of KWH for a power meter
    V_SCENE_ON_V20 = 19,    //Turn on a scene
    V_SCENE_OFF_V20 = 20,   //Turn of a scene
    V_HVAC_FLOW_STATE_V20 = 21, //Mode of header. One of "Off", "HeatOn", "CoolOn", or "AutoChangeOver"
    V_HVAC_SPEED_V20 = 22,  //HVAC/Heater fan speed ("Min", "Normal", "Max", "Auto")
    V_LIGHT_LEVEL_V20 = 23, //Uncalibrated light level. 0-100%. Use V_LEVEL for light level in lux.
    V_VAR1_V20 = 24,    //Custom value
    V_VAR2_V20 = 25,    //Custom value
    V_VAR3_V20 = 26,    //Custom value
    V_VAR4_V20 = 27,    //Custom value
    V_VAR5_V20 = 28,    //Custom value
    V_UP_V20 = 29,  //Window covering. Up.
    V_DOWN_V20 = 30,    //Window covering. Down.
    V_STOP_V20 = 31,    //Window covering. Stop.
    V_IR_SEND_V20 = 32, //Send out an IR-command
    V_IR_RECEIVE_V20 = 33,  //This message contains a received IR-command
    V_FLOW_V20 = 34,    //Flow of water (in meter)
    V_VOLUME_V20 = 35,  //Water volume
    V_LOCK_STATUS_V20 = 36, //Set or get lock status. 1=Locked, 0=Unlocked
    V_LEVEL_V20 = 37,   //Used for sending level-value
    V_VOLTAGE_V20 = 38, //Voltage level
    V_CURRENT_V20 = 39, //Current level
    V_RGB_V20 = 40, //RGB value transmitted as ASCII hex string (I.e "ff0000" for red)
    V_RGBW_V20 = 41,    //RGBW value transmitted as ASCII hex string (I.e "ff0000ff" for red + full white)
    V_ID_V20 = 42,  //Optional unique sensor id (e.g. OneWire DS1820b ids)
    V_UNIT_PREFIX_V20 = 43, //Allows sensors to send in a string representing the unit prefix to be displayed in GUI. This is not parsed by controller! E.g. cm, m, km, inch.
    V_HVAC_SETPOINT_COOL_V20 = 44,  //HVAC cold setpoint (Integer between 0-100)
    V_HVAC_SETPOINT_HEAT_V20 = 45,  //HVAC/Heater setpoint (Integer between 0-100)
    V_HVAC_FLOW_MODE_V20 = 46,   //Flow mode for HVAC ("Auto", "ContinuousOn", "PeriodicOn")
    V_TEXT_V20 = 47, //Text message to display on LCD or controller device
    V_CUSTOM_V20 = 48, //Custom messages used for controller/inter node specific commands, preferably using S_CUSTOM device type.
    V_POSITION_V20 = 49, //GPS position and altitude. Payload: latitude;longitude;altitude(m). E.g. "55.722526;13.017972;18"
    V_IR_RECORD_V20 = 50, //Record IR codes S_IR for playback
    V_PH_V20 = 51, //Water PH
    V_ORP_V20 = 52, //Water ORP: redox potential in mV
    V_EC_V20 = 53, //Water electric conductivity μS/cm (microSiemens/cm)
    V_VAR_V20 = 54, //Reactive power: volt-ampere reactive (var)
    V_VA_V20 = 55, //Apparent power: volt-ampere (VA)
    V_POWER_FACTOR_V20 = 56 //Ratio of real power to apparent power: floating point value in the range [-1,..,1]
};

enum internalTypesV20
{
    I_BATTERY_LEVEL_V20 = 0,    //Use this to report the battery level (in percent 0-100).
    I_TIME_V20 = 1, //Sensors can request the current time from the Controller using this message. The time will be reported as the seconds since 1970
    I_VERSION_V20 = 2,  //Used to request gateway version from controller.
    I_ID_REQUEST_V20 = 3,   //Use this to request a unique node id from the controller.
    I_ID_RESPONSE_V20 = 4,  //Id response back to sensor. Payload contains sensor id.
    I_INCLUSION_MODE_V20 = 5,   //Start/stop inclusion mode of the Controller (1=start, 0=stop).
    I_CONFIG_V20 = 6,   //Config request from node. Reply with (M)etric or (I)mperal back to sensor.
    I_FIND_PARENT_V20 = 7,  //When a sensor starts up, it broadcast a search request to all neighbor nodes. They reply with a I_FIND_PARENT_RESPONSE.
    I_FIND_PARENT_RESPONSE_V20 = 8, //Reply message type to I_FIND_PARENT request.
    I_LOG_MESSAGE_V20 = 9,  //Sent by the gateway to the Controller to trace-log a message
    I_CHILDREN_V20 = 10,    //A message that can be used to transfer child sensors (from EEPROM routing table) of a repeating node.
    I_SKETCH_NAME_V20 = 11, //Optional sketch name that can be used to identify sensor in the Controller GUI
    I_SKETCH_VERSION_V20 = 12,  //Optional sketch version that can be reported to keep track of the version of sensor in the Controller GUI.
    I_REBOOT_V20 = 13,  //Used by OTA firmware updates. Request for node to reboot.
    I_GATEWAY_READY_V20 = 14,   //Send by gateway to controller when startup is complete.
    I_REQUEST_SIGNING_V20 = 15, //Used between sensors when initialting signing.
    I_GET_NONCE_V20 = 16,   //Used between sensors when requesting nonce.
    I_GET_NONCE_RESPONSE_V20 = 17,   //Used between sensors for nonce response.
    I_HEARTBEAT_V20 = 18, //Heartbeat request
    I_PRESENTATION_V20 = 19, //Presentation message
    I_DISCOVER_V20 = 20, //Discover request
    I_DISCOVER_RESPONSE_V20 = 21, //Discover response
    I_HEARTBEAT_RESPONSE_V20 = 22, //Heartbeat response
    I_LOCKED_V20 = 23, //Node is locked (reason in string-payload)
    I_PING_V20 = 24, //Ping sent to node, payload incremental hop counter
    I_PONG_V20 = 25, //In return to ping, sent back to sender, payload incremental hop counter
    I_REGISTRATION_REQUEST_V20 = 26, //Register request to GW
    I_REGISTRATION_RESPONSE_V20 = 27, //Register response from GW
    I_DEBUG_V20 = 28 //Debug message
};

const char* getMsgTypeNameV20(enum msgTypeV20 type);
const char* getDeviceTypeNameV20(enum deviceTypesV20 type);
const char* getVariableTypeNameV20(enum varTypesV20 type);
const char* getInternalTypeNameV20(enum internalTypesV20 type);
