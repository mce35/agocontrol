#include "MySensors.h"

/**
 * PROTOCOL v1.3
 * http://www.mysensors.org/build/serial_api_13
 */

const char* getMsgTypeNameV13(enum msgTypeV13 type)
{
	switch (type)
    {
		case PRESENTATION_V13: return "PRESENTATION";
		case SET_VARIABLE_V13: return "SET_VARIABLE";
		case REQUEST_VARIABLE_V13: return "REQUEST_VARIABLE";
		case VARIABLE_ACK_V13: return "VARIABLE_ACK";
		case INTERNAL_V13: return "INTERNAL";
	}
}

const char* getDeviceTypeNameV13(enum deviceTypesV13 type)
{
	switch (type)
    {
		case S_DOOR_V13: return "S_DOOR";
		case S_MOTION_V13: return "S_MOTION";
		case S_SMOKE_V13: return "S_SMOKE";
		case S_LIGHT_V13: return "S_LIGHT";
		case S_DIMMER_V13: return "S_DIMMER";
		case S_COVER_V13: return "S_COVER";
		case S_TEMP_V13: return "S_TEMP";
		case S_HUM_V13: return "S_HUM";
		case S_BARO_V13: return "S_BARO";
		case S_WIND_V13: return "S_WIND";
		case S_RAIN_V13: return "S_RAIN";
		case S_UV_V13: return "S_UV";
		case S_WEIGHT_V13: return "S_WEIGHT";
		case S_POWER_V13: return "S_POWER";
		case S_HEATER_V13: return "S_HEATER";
		case S_DISTANCE_V13: return "S_DISTANCE";
		case S_LIGHT_LEVEL_V13: return "S_LIGHT_LEVEL";
		case S_ARDUINO_NODE_V13: return "S_ARDUINO_NODE";
		case S_ARDUINO_RELAY_V13: return "S_ARDUINO_RELAY";
		case S_LOCK_V13: return "S_LOCK";
		case S_IR_V13: return "S_IR";
		case S_WATER_V13: return "S_WATER";
	}
}


const char* getVariableTypeNameV13(enum varTypesV13 type)
{
	switch (type)
    {
		case V_TEMP_V13: return "V_TEMP";
		case V_HUM_V13: return "V_HUM";
		case V_LIGHT_V13: return "V_LIGHT";
		case V_DIMMER_V13: return "V_DIMMER";
		case V_PRESSURE_V13: return "V_PRESSURE";
		case V_FORECAST_V13: return "V_FORECAST";
		case V_RAIN_V13: return "V_RAIN";
		case V_RAINRATE_V13: return "V_RAINRATE";
		case V_WIND_V13: return "V_WIND";
		case V_GUST_V13: return "V_GUST";
		case V_DIRECTION_V13: return "V_DIRECTION";
		case V_UV_V13: return "V_UV";
		case V_WEIGHT_V13: return "V_WEIGHT";
		case V_DISTANCE_V13: return "V_DISTANCE";
		case V_IMPEDANCE_V13: return "V_IMPEDANCE";
		case V_ARMED_V13: return "V_ARMED";
		case V_TRIPPED_V13: return "V_TRIPPED";
		case V_WATT_V13: return "V_WATT";
		case V_KWH_V13: return "V_KWH";
		case V_SCENE_ON_V13: return "V_SCENE_ON";
		case V_SCENE_OFF_V13: return "V_SCENE_OFF";
		case V_HEATER_V13: return "V_HEATER";
		case V_HEATER_SW_V13: return "V_HEATER_SW";
		case V_LIGHT_LEVEL_V13: return "V_LIGHT_LEVEL";
		case V_VAR1_V13: return "V_VAR1";
		case V_VAR2_V13: return "V_VAR2";
		case V_VAR3_V13: return "V_VAR3";
		case V_VAR4_V13: return "V_VAR4";
		case V_VAR5_V13: return "V_VAR5";
		case V_UP_V13: return "V_UP";
		case V_DOWN_V13: return "V_DOWN";
		case V_STOP_V13: return "V_STOP";
		case V_IR_SEND_V13: return "V_IR_SEND";
		case V_IR_RECEIVE_V13: return "V_IR_RECEIVE";
		case V_FLOW_V13: return "V_FLOW";
		case V_VOLUME_V13: return "V_VOLUME";
		case V_LOCK_STATUS_V13: return "V_LOCK_STATUS";
	}
}

const char* getInternalTypeNameV13(enum internalTypesV13 type)
{
	switch (type)
    {
		case I_BATTERY_LEVEL_V13: return "I_BATTERY_LEVEL";
		case I_TIME_V13: return "I_TIME";
		case I_VERSION_V13: return "I_VERSION";
		case I_REQUEST_ID_V13: return "I_REQUEST_ID";
		case I_INCLUSION_MODE_V13: return "I_INCLUSION_MODE";
		case I_RELAY_NODE_V13: return "I_RELAY_NODE";
		case I_PING_V13: return "I_PING";
		case I_PING_ACK_V13: return "I_PING_ACK";
		case I_LOG_MESSAGE_V13: return "I_LOG_MESSAGE";
		case I_CHILDREN_V13: return "I_CHILDREN";
		case I_UNIT_V13: return "I_UNIT";
		case I_SKETCH_NAME_V13: return "I_SKETCH_NAME";
		case I_SKETCH_VERSION_V13: return "I_SKETCH_VERSION";
	}
}

/**
 * PROTOCOL v1.4
 * http://www.mysensors.org/build/serial_api
 */

const char* getMsgTypeNameV14(enum msgTypeV14 type)
{
	switch (type)
    {
		case PRESENTATION_V14: return "PRESENTATION";
		case SET_V14: return "SET";
		case REQ_V14: return "REQ";
		case INTERNAL_V14: return "INTERNAL";
		case STREAM_V14: return "STREAM";
	}
}

const char* getDeviceTypeNameV14(enum deviceTypesV14 type)
{
	switch (type)
    {
		case S_DOOR_V14: return "S_DOOR";
		case S_MOTION_V14: return "S_MOTION";
		case S_SMOKE_V14: return "S_SMOKE";
		case S_LIGHT_V14: return "S_LIGHT";
		case S_DIMMER_V14: return "S_DIMMER";
		case S_COVER_V14: return "S_COVER";
		case S_TEMP_V14: return "S_TEMP";
		case S_HUM_V14: return "S_HUM";
		case S_BARO_V14: return "S_BARO";
		case S_WIND_V14: return "S_WIND";
		case S_RAIN_V14: return "S_RAIN";
		case S_UV_V14: return "S_UV";
		case S_WEIGHT_V14: return "S_WEIGHT";
		case S_POWER_V14: return "S_POWER";
		case S_HEATER_V14: return "S_HEATER";
		case S_DISTANCE_V14: return "S_DISTANCE";
		case S_LIGHT_LEVEL_V14: return "S_LIGHT_LEVEL";
		case S_ARDUINO_NODE_V14: return "S_ARDUINO_NODE";
		case S_ARDUINO_RELAY_V14: return "S_ARDUINO_RELAY";
		case S_LOCK_V14: return "S_LOCK";
		case S_IR_V14: return "S_IR";
		case S_WATER_V14: return "S_WATER";
		case S_AIR_QUALITY_V14: return "S_AIR_QUALITY";
		case S_CUSTOM_V14: return "S_CUSTOM";
		case S_DUST_V14: return "S_DUST";
		case S_SCENE_CONTROLLER_V14: return "S_SCENE_CONTROLLER";
	}
}


const char* getVariableTypeNameV14(enum varTypesV14 type)
{
	switch (type)
    {
		case V_TEMP_V14: return "V_TEMP";
		case V_HUM_V14: return "V_HUM";
		case V_LIGHT_V14: return "V_LIGHT";
		case V_DIMMER_V14: return "V_DIMMER";
		case V_PRESSURE_V14: return "V_PRESSURE";
		case V_FORECAST_V14: return "V_FORECAST";
		case V_RAIN_V14: return "V_RAIN";
		case V_RAINRATE_V14: return "V_RAINRATE";
		case V_WIND_V14: return "V_WIND";
		case V_GUST_V14: return "V_GUST";
		case V_DIRECTION_V14: return "V_DIRECTION";
		case V_UV_V14: return "V_UV";
		case V_WEIGHT_V14: return "V_WEIGHT";
		case V_DISTANCE_V14: return "V_DISTANCE";
		case V_IMPEDANCE_V14: return "V_IMPEDANCE";
		case V_ARMED_V14: return "V_ARMED";
		case V_TRIPPED_V14: return "V_TRIPPED";
		case V_WATT_V14: return "V_WATT";
		case V_KWH_V14: return "V_KWH";
		case V_SCENE_ON_V14: return "V_SCENE_ON";
		case V_SCENE_OFF_V14: return "V_SCENE_OFF";
		case V_HEATER_V14: return "V_HEATER";
		case V_HEATER_SW_V14: return "V_HEATER_SW";
		case V_LIGHT_LEVEL_V14: return "V_LIGHT_LEVEL";
		case V_VAR1_V14: return "V_VAR1";
		case V_VAR2_V14: return "V_VAR2";
		case V_VAR3_V14: return "V_VAR3";
		case V_VAR4_V14: return "V_VAR4";
		case V_VAR5_V14: return "V_VAR5";
		case V_UP_V14: return "V_UP";
		case V_DOWN_V14: return "V_DOWN";
		case V_STOP_V14: return "V_STOP";
		case V_IR_SEND_V14: return "V_IR_SEND";
		case V_IR_RECEIVE_V14: return "V_IR_RECEIVE";
		case V_FLOW_V14: return "V_FLOW";
		case V_VOLUME_V14: return "V_VOLUME";
		case V_LOCK_STATUS_V14: return "V_LOCK_STATUS";
		case V_DUST_LEVEL_V14: return "V_DUST_LEVEL";
		case V_VOLTAGE_V14: return "V_VOLTAGE";
		case V_CURRENT_V14: return "V_CURRENT";
	}
}

const char* getInternalTypeNameV14(enum internalTypesV14 type)
{
	switch (type)
    {
		case I_BATTERY_LEVEL_V14: return "I_BATTERY_LEVEL";
		case I_TIME_V14: return "I_TIME";
		case I_VERSION_V14: return "I_VERSION";
		case I_ID_REQUEST_V14: return "I_ID_REQUEST";
		case I_ID_RESPONSE_V14: return "I_ID_RESPONSE";
		case I_INCLUSION_MODE_V14: return "I_INCLUSION_MODE";
		case I_CONFIG_V14: return "I_CONFIG";
		case I_FIND_PARENT_V14: return "I_FIND_PARENT";
		case I_FIND_PARENT_RESPONSE_V14: return "I_FIND_PARENT_RESPONSE";
		case I_LOG_MESSAGE_V14: return "I_LOG_MESSAGE";
		case I_CHILDREN_V14: return "I_CHILDREN";
        case I_SKETCH_NAME_V14: return "I_SKETCH_NAME";
		case I_SKETCH_VERSION_V14: return "I_SKETCH_VERSION";
		case I_REBOOT_V14: return "I_REBOOT";
		case I_GATEWAY_READY_V14: return "I_GATEWAY_READY";
	}
}
