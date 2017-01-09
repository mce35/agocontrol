#include "MySensors.h"
#include <string>

using namespace std;

/**
 * PROTOCOL v1.3
 * http://www.mysensors.org/download/serial_api_13
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
        default:
            string out = "UNKNOWN[";
            out += (int)type;
            out += "]";
            return out.c_str();
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
        default:
            string out = "UNKNOWN[";
            out += (int)type;
            out += "]";
            return out.c_str();
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
        default:
            string out = "UNKNOWN[";
            out += (int)type;
            out += "]";
            return out.c_str();
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
        default:
            string out = "UNKNOWN[";
            out += (int)type;
            out += "]";
            return out.c_str();
    }
}

/**
 * PROTOCOL v1.4
 * http://www.mysensors.org/download/serial_api_14
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
        default:
            string out = "UNKNOWN[";
            out += (int)type;
            out += "]";
            return out.c_str();
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
        default:
            string out = "UNKNOWN[";
            out += (int)type;
            out += "]";
            return out.c_str();
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
        default:
            string out = "UNKNOWN[";
            out += (int)type;
            out += "]";
            return out.c_str();
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
        default:
            string out = "UNKNOWN[";
            out += (int)type;
            out += "]";
            return out.c_str();
    }
}

/**
 * PROTOCOL v1.5
 * http://www.mysensors.org/download/serial_api_15
 */

const char* getMsgTypeNameV15(enum msgTypeV15 type)
{
    switch(type)
    {
        case PRESENTATION_V15: return "PRESENTATION";
        case SET_V15: return "SET";
        case REQ_V15: return "REQ";
        case INTERNAL_V15: return "INTERNAL";
        case STREAM_V15: return "STREAM";
        default:
            string out = "UNKNOWN[";
            out += (int)type;
            out += "]";
            return out.c_str();
    }
}

const char* getDeviceTypeNameV15(enum deviceTypesV15 type)
{
    switch(type)
    {
        case S_DOOR_V15: return "S_DOOR";
        case S_MOTION_V15: return "S_MOTION";
        case S_SMOKE_V15: return "S_SMOKE";
        case S_BINARY_V15: return "S_BINARY";
        case S_DIMMER_V15: return "S_DIMMER";
        case S_COVER_V15: return "S_COVER";
        case S_TEMP_V15: return "S_TEMP";
        case S_HUM_V15: return "S_HUM";
        case S_BARO_V15: return "S_BARO";
        case S_WIND_V15: return "S_WIND";
        case S_RAIN_V15: return "S_RAIN";
        case S_UV_V15: return "S_UV";
        case S_WEIGHT_V15: return "S_WEIGHT";
        case S_POWER_V15: return "S_POWER";
        case S_HEATER_V15: return "S_HEATER";
        case S_DISTANCE_V15: return "S_DISTANCE";
        case S_LIGHT_LEVEL_V15: return "S_LIGHT_LEVEL";
        case S_ARDUINO_NODE_V15: return "S_ARDUINO_NODE";
        case S_ARDUINO_REPEATER_NODE_V15: return "S_ARDUINO_REPEATER_NODE";
        case S_LOCK_V15: return "S_LOCK";
        case S_IR_V15: return "S_IR";
        case S_WATER_V15: return "S_WATER";
        case S_AIR_QUALITY_V15: return "S_AIR_QUALITY";
        case S_CUSTOM_V15: return "S_CUSTOM";
        case S_DUST_V15: return "S_DUST";
        case S_SCENE_CONTROLLER_V15: return "S_SCENE_CONTROLLER";
        case S_RGB_LIGHT_V15: return "S_RGB_LIGHT";
        case S_RGBW_LIGHT_V15: return "S_RGBW_LIGHT";
        case S_COLOR_SENSOR_V15: return "S_COLOR_SENSOR";
        case S_HVAC_V15: return "S_HVAC";
        case S_MULTIMETER_V15: return "S_MULTIMETER";
        case S_SPRINKLER_V15: return "S_SPRINKLER";
        case S_WATER_LEAK_V15: return "S_WATER_LEAK";
        case S_SOUND_V15: return "S_SOUND";
        case S_VIBRATION_V15: return "S_VIBRATION";
        case S_MOISTURE_V15: return "S_MOISTURE";
        default:
            string out = "UNKNOWN[";
            out += (int)type;
            out += "]";
            return out.c_str();
    }
}

const char* getVariableTypeNameV15(enum varTypesV15 type)
{
    switch (type)
    {
        case V_TEMP_V15: return "V_TEMP";
        case V_HUM_V15: return "V_HUM";
        case V_STATUS_V15: return "V_STATUS";
        case V_PERCENTAGE_V15: return "V_PERCENTAGE";
        case V_PRESSURE_V15: return "V_PRESSURE";
        case V_FORECAST_V15: return "V_FORECAST";
        case V_RAIN_V15: return "V_RAIN";
        case V_RAINRATE_V15: return "V_RAINRATE";
        case V_WIND_V15: return "V_WIND";
        case V_GUST_V15: return "V_GUST";
        case V_DIRECTION_V15: return "V_DIRECTION";
        case V_UV_V15: return "V_UV";
        case V_WEIGHT_V15: return "V_WEIGHT";
        case V_DISTANCE_V15: return "V_DISTANCE";
        case V_IMPEDANCE_V15: return "V_IMPEDANCE";
        case V_ARMED_V15: return "V_ARMED";
        case V_TRIPPED_V15: return "V_TRIPPED";
        case V_WATT_V15: return "V_WATT";
        case V_KWH_V15: return "V_KWH";
        case V_SCENE_ON_V15: return "V_SCENE_ON";
        case V_SCENE_OFF_V15: return "V_SCENE_OFF";
        case V_HVAC_FLOW_STATE_V15: return "V_HVAC_FLOW_STATE";
        case V_HVAC_SPEED_V15: return "V_HVAC_SPEED";
        case V_LIGHT_LEVEL_V15: return "V_LIGHT_LEVEL";
        case V_VAR1_V15: return "V_VAR1";
        case V_VAR2_V15: return "V_VAR2";
        case V_VAR3_V15: return "V_VAR3";
        case V_VAR4_V15: return "V_VAR4";
        case V_VAR5_V15: return "V_VAR5";
        case V_UP_V15: return "V_UP";
        case V_DOWN_V15: return "V_DOWN";
        case V_STOP_V15: return "V_STOP";
        case V_IR_SEND_V15: return "V_IR_SEND";
        case V_IR_RECEIVE_V15: return "V_IR_RECEIVE";
        case V_FLOW_V15: return "V_FLOW";
        case V_VOLUME_V15: return "V_VOLUME";
        case V_LOCK_STATUS_V15: return "V_LOCK_STATUS";
        case V_LEVEL_V15: return "V_LEVEL";
        case V_VOLTAGE_V15: return "V_VOLTAGE";
        case V_CURRENT_V15: return "V_CURRENT";
        case V_RGB_V15: return "V_RGB";
        case V_RGBW_V15: return "V_RGBW";
        case V_ID_V15: return "V_ID";
        case V_UNIT_PREFIX_V15: return "V_UNIT_PREFIX";
        case V_HVAC_SETPOINT_COOL_V15: return "V_HVAC_SETPOINT_COOL";
        case V_HVAC_SETPOINT_HEAT_V15: return "V_HVAC_SETPOINT_HEAT";
        case V_HVAC_FLOW_MODE_V15: return "V_HVAC_FLOW_MODE";
        default:
            string out = "UNKNOWN[";
            out += (int)type;
            out += "]";
            return out.c_str();
    }
}

const char* getInternalTypeNameV15(enum internalTypesV15 type)
{
    switch (type)
    {
        case I_BATTERY_LEVEL_V15: return "I_BATTERY_LEVEL";
        case I_TIME_V15: return "I_TIME";
        case I_VERSION_V15: return "I_VERSION";
        case I_ID_REQUEST_V15: return "I_ID_REQUEST";
        case I_ID_RESPONSE_V15: return "I_ID_RESPONSE";
        case I_INCLUSION_MODE_V15: return "I_INCLUSION_MODE";
        case I_CONFIG_V15: return "I_CONFIG";
        case I_FIND_PARENT_V15: return "I_FIND_PARENT";
        case I_FIND_PARENT_RESPONSE_V15: return "I_FIND_PARENT_RESPONSE";
        case I_LOG_MESSAGE_V15: return "I_LOG_MESSAGE";
        case I_CHILDREN_V15: return "I_CHILDREN";
        case I_SKETCH_NAME_V15: return "I_SKETCH_NAME";
        case I_SKETCH_VERSION_V15: return "I_SKETCH_VERSION";
        case I_REBOOT_V15: return "I_REBOOT";
        case I_GATEWAY_READY_V15: return "I_GATEWAY_READY";
        case I_REQUEST_SIGNING_V15: return "I_REQUEST_SIGNING";
        case I_GET_NONCE_V15: return "I_GET_NONCE";
        case I_GET_NONCE_RESPONSE_V15: return "I_GET_NONCE_RESPONSE";
        default:
            string out = "UNKNOWN[";
            out += (int)type;
            out += "]";
            return out.c_str();
    }
}

/**
 * PROTOCOL v2.0
 * http://www.mysensors.org/download/serial_api_20
 */

const char* getMsgTypeNameV20(enum msgTypeV20 type)
{
    switch(type)
    {
        case PRESENTATION_V20: return "PRESENTATION";
        case SET_V20: return "SET";
        case REQ_V20: return "REQ";
        case INTERNAL_V20: return "INTERNAL";
        case STREAM_V20: return "STREAM";
        default:
            string out = "UNKNOWN[";
            out += (int)type;
            out += "]";
            return out.c_str();
    }
}

const char* getDeviceTypeNameV20(enum deviceTypesV20 type)
{
    switch(type)
    {
        case S_DOOR_V20: return "S_DOOR";
        case S_MOTION_V20: return "S_MOTION";
        case S_SMOKE_V20: return "S_SMOKE";
        case S_BINARY_V20: return "S_BINARY";
        case S_DIMMER_V20: return "S_DIMMER";
        case S_COVER_V20: return "S_COVER";
        case S_TEMP_V20: return "S_TEMP";
        case S_HUM_V20: return "S_HUM";
        case S_BARO_V20: return "S_BARO";
        case S_WIND_V20: return "S_WIND";
        case S_RAIN_V20: return "S_RAIN";
        case S_UV_V20: return "S_UV";
        case S_WEIGHT_V20: return "S_WEIGHT";
        case S_POWER_V20: return "S_POWER";
        case S_HEATER_V20: return "S_HEATER";
        case S_DISTANCE_V20: return "S_DISTANCE";
        case S_LIGHT_LEVEL_V20: return "S_LIGHT_LEVEL";
        case S_ARDUINO_NODE_V20: return "S_ARDUINO_NODE";
        case S_ARDUINO_REPEATER_NODE_V20: return "S_ARDUINO_REPEATER_NODE";
        case S_LOCK_V20: return "S_LOCK";
        case S_IR_V20: return "S_IR";
        case S_WATER_V20: return "S_WATER";
        case S_AIR_QUALITY_V20: return "S_AIR_QUALITY";
        case S_CUSTOM_V20: return "S_CUSTOM";
        case S_DUST_V20: return "S_DUST";
        case S_SCENE_CONTROLLER_V20: return "S_SCENE_CONTROLLER";
        case S_RGB_LIGHT_V20: return "S_RGB_LIGHT";
        case S_RGBW_LIGHT_V20: return "S_RGBW_LIGHT";
        case S_COLOR_SENSOR_V20: return "S_COLOR_SENSOR";
        case S_HVAC_V20: return "S_HVAC";
        case S_MULTIMETER_V20: return "S_MULTIMETER";
        case S_SPRINKLER_V20: return "S_SPRINKLER";
        case S_WATER_LEAK_V20: return "S_WATER_LEAK";
        case S_SOUND_V20: return "S_SOUND";
        case S_VIBRATION_V20: return "S_VIBRATION";
        case S_MOISTURE_V20: return "S_MOISTURE";
        case S_INFO_V20: return "S_INFO";
        case S_GAS_V20: return "S_GAS";
        case S_GPS_V20: return "S_GPS";
        case S_WATER_QUALITY_V20: return "S_WATER_QUALITY";
        default:
            string out = "UNKNOWN[";
            out += (int)type;
            out += "]";
            return out.c_str();
    }
}

const char* getVariableTypeNameV20(enum varTypesV20 type)
{
    switch (type)
    {
        case V_TEMP_V20: return "V_TEMP";
        case V_HUM_V20: return "V_HUM";
        case V_STATUS_V20: return "V_STATUS";
        case V_PERCENTAGE_V20: return "V_PERCENTAGE";
        case V_PRESSURE_V20: return "V_PRESSURE";
        case V_FORECAST_V20: return "V_FORECAST";
        case V_RAIN_V20: return "V_RAIN";
        case V_RAINRATE_V20: return "V_RAINRATE";
        case V_WIND_V20: return "V_WIND";
        case V_GUST_V20: return "V_GUST";
        case V_DIRECTION_V20: return "V_DIRECTION";
        case V_UV_V20: return "V_UV";
        case V_WEIGHT_V20: return "V_WEIGHT";
        case V_DISTANCE_V20: return "V_DISTANCE";
        case V_IMPEDANCE_V20: return "V_IMPEDANCE";
        case V_ARMED_V20: return "V_ARMED";
        case V_TRIPPED_V20: return "V_TRIPPED";
        case V_WATT_V20: return "V_WATT";
        case V_KWH_V20: return "V_KWH";
        case V_SCENE_ON_V20: return "V_SCENE_ON";
        case V_SCENE_OFF_V20: return "V_SCENE_OFF";
        case V_HVAC_FLOW_STATE_V20: return "V_HVAC_FLOW_STATE";
        case V_HVAC_SPEED_V20: return "V_HVAC_SPEED";
        case V_LIGHT_LEVEL_V20: return "V_LIGHT_LEVEL";
        case V_VAR1_V20: return "V_VAR1";
        case V_VAR2_V20: return "V_VAR2";
        case V_VAR3_V20: return "V_VAR3";
        case V_VAR4_V20: return "V_VAR4";
        case V_VAR5_V20: return "V_VAR5";
        case V_UP_V20: return "V_UP";
        case V_DOWN_V20: return "V_DOWN";
        case V_STOP_V20: return "V_STOP";
        case V_IR_SEND_V20: return "V_IR_SEND";
        case V_IR_RECEIVE_V20: return "V_IR_RECEIVE";
        case V_FLOW_V20: return "V_FLOW";
        case V_VOLUME_V20: return "V_VOLUME";
        case V_LOCK_STATUS_V20: return "V_LOCK_STATUS";
        case V_LEVEL_V20: return "V_LEVEL";
        case V_VOLTAGE_V20: return "V_VOLTAGE";
        case V_CURRENT_V20: return "V_CURRENT";
        case V_RGB_V20: return "V_RGB";
        case V_RGBW_V20: return "V_RGBW";
        case V_ID_V20: return "V_ID";
        case V_UNIT_PREFIX_V20: return "V_UNIT_PREFIX";
        case V_HVAC_SETPOINT_COOL_V20: return "V_HVAC_SETPOINT_COOL";
        case V_HVAC_SETPOINT_HEAT_V20: return "V_HVAC_SETPOINT_HEAT";
        case V_HVAC_FLOW_MODE_V20: return "V_HVAC_FLOW_MODE";
        case V_TEXT_V20: return "V_TEXT";
        case V_CUSTOM_V20: return "V_CUSTOM";
        case V_POSITION_V20: return "V_POSITION";
        case V_IR_RECORD_V20: return "V_IR_RECORD";
        case V_PH_V20: return "V_PH";
        case V_ORP_V20: return "V_ORP";
        case V_EC_V20: return "V_EC";
        case V_VAR_V20: return "V_VAR";
        case V_VA_V20: return "V_VA";
        case V_POWER_FACTOR_V20: return "V_POWER_FACTOR";
        default:
            string out = "UNKNOWN[";
            out += (int)type;
            out += "]";
            return out.c_str();
    }
}

const char* getInternalTypeNameV20(enum internalTypesV20 type)
{
    switch (type)
    {
        case I_BATTERY_LEVEL_V20: return "I_BATTERY_LEVEL";
        case I_TIME_V20: return "I_TIME";
        case I_VERSION_V20: return "I_VERSION";
        case I_ID_REQUEST_V20: return "I_ID_REQUEST";
        case I_ID_RESPONSE_V20: return "I_ID_RESPONSE";
        case I_INCLUSION_MODE_V20: return "I_INCLUSION_MODE";
        case I_CONFIG_V20: return "I_CONFIG";
        case I_FIND_PARENT_V20: return "I_FIND_PARENT";
        case I_FIND_PARENT_RESPONSE_V20: return "I_FIND_PARENT_RESPONSE";
        case I_LOG_MESSAGE_V20: return "I_LOG_MESSAGE";
        case I_CHILDREN_V20: return "I_CHILDREN";
        case I_SKETCH_NAME_V20: return "I_SKETCH_NAME";
        case I_SKETCH_VERSION_V20: return "I_SKETCH_VERSION";
        case I_REBOOT_V20: return "I_REBOOT";
        case I_GATEWAY_READY_V20: return "I_GATEWAY_READY";
        case I_REQUEST_SIGNING_V20: return "I_REQUEST_SIGNING";
        case I_GET_NONCE_V20: return "I_GET_NONCE";
        case I_GET_NONCE_RESPONSE_V20: return "I_GET_NONCE_RESPONSE";
        case I_HEARTBEAT_V20: return "I_HEARTBEAT";
        case I_PRESENTATION_V20: return "I_PRESENTATION";
        case I_DISCOVER_V20: return "I_DISCOVER";
        case I_DISCOVER_RESPONSE_V20: return "I_DISCOVER_RESPONSE";
        case I_HEARTBEAT_RESPONSE_V20: return "I_HEARTBEAT_RESPONSE";
        case I_LOCKED_V20: return "I_LOCKED";
        case I_PING_V20: return "I_PING";
        case I_PONG_V20: return "I_PONG";
        case I_REGISTRATION_REQUEST_V20: return "I_REGISTRATION_REQUEST";
        case I_REGISTRATION_RESPONSE_V20: return "I_REGISTRATION_RESPONSE";
        case I_DEBUG_V20: return "I_DEBUG";
        default:
            string out = "UNKNOWN[";
            out += (int)type;
            out += "]";
            return out.c_str();
    }
}
