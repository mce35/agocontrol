#include "MySensors.h"

const char* getMsgTypeName(enum msgType type) {
	switch (type) {
		case PRESENTATION: return "PRESENTATION";
		case SET_VARIABLE: return "SET_VARIABLE";
		case REQUEST_VARIABLE: return "REQUEST_VARIABLE";
		case VARIABLE_ACK: return "VARIABLE_ACK";
		case INTERNAL: return "INTERNAL";
	}
}

const char* getDeviceTypeName(enum deviceTypes type) {
	switch (type) {
		case S_DOOR: return "S_DOOR";
		case S_MOTION: return "S_MOTION";
		case S_SMOKE: return "S_SMOKE";
		case S_LIGHT: return "S_LIGHT";
		case S_DIMMER: return "S_DIMMER";
		case S_COVER: return "S_COVER";
		case S_TEMP: return "S_TEMP";
		case S_HUM: return "S_HUM";
		case S_BARO: return "S_BARO";
		case S_WIND: return "S_WIND";
		case S_RAIN: return "S_RAIN";
		case S_UV: return "S_UV";
		case S_WEIGHT: return "S_WEIGHT";
		case S_POWER: return "S_POWER";
		case S_HEATER: return "S_HEATER";
		case S_DISTANCE: return "S_DISTANCE";
		case S_LIGHT_LEVEL: return "S_LIGHT_LEVEL";
		case S_ARDUINO_NODE: return "S_ARDUINO_NODE";
		case S_ARDUINO_RELAY: return "S_ARDUINO_RELAY";
		case S_LOCK: return "S_LOCK";
		case S_IR: return "S_IR";
		case S_WATER: return "S_WATER";
	}
}


const char* getVariableTypeName(enum varTypes type) {
	switch (type) {
		case V_TEMP: return "V_TEMP";
		case V_HUM: return "V_HUM";
		case V_LIGHT: return "V_LIGHT";
		case V_DIMMER: return "V_DIMMER";
		case V_PRESSURE: return "V_PRESSURE";
		case V_FORECAST: return "V_FORECAST";
		case V_RAIN: return "V_RAIN";
		case V_RAINRATE: return "V_RAINRATE";
		case V_WIND: return "V_WIND";
		case V_GUST: return "V_GUST";
		case V_DIRECTION: return "V_DIRECTION";
		case V_UV: return "V_UV";
		case V_WEIGHT: return "V_WEIGHT";
		case V_DISTANCE: return "V_DISTANCE";
		case V_IMPEDANCE: return "V_IMPEDANCE";
		case V_ARMED: return "V_ARMED";
		case V_TRIPPED: return "V_TRIPPED";
		case V_WATT: return "V_WATT";
		case V_KWH: return "V_KWH";
		case V_SCENE_ON: return "V_SCENE_ON";
		case V_SCENE_OFF: return "V_SCENE_OFF";
		case V_HEATER: return "V_HEATER";
		case V_HEATER_SW: return "V_HEATER_SW";
		case V_LIGHT_LEVEL: return "V_LIGHT_LEVEL";
		case V_VAR1: return "V_VAR1";
		case V_VAR2: return "V_VAR2";
		case V_VAR3: return "V_VAR3";
		case V_VAR4: return "V_VAR4";
		case V_VAR5: return "V_VAR5";
		case V_UP: return "V_UP";
		case V_DOWN: return "V_DOWN";
		case V_STOP: return "V_STOP";
		case V_IR_SEND: return "V_IR_SEND";
		case V_IR_RECEIVE: return "V_IR_RECEIVE";
		case V_FLOW: return "V_FLOW";
		case V_VOLUME: return "V_VOLUME";
		case V_LOCK_STATUS: return "V_LOCK_STATUS";
	}
}

const char* getInternalTypeName(enum internalTypes type) {
	switch (type) {
		case I_BATTERY_LEVEL: return "I_BATTERY_LEVEL";
		case I_TIME: return "I_TIME";
		case I_VERSION: return "I_VERSION";
		case I_REQUEST_ID: return "I_REQUEST_ID";
		case I_INCLUSION_MODE: return "I_INCLUSION_MODE";
		case I_RELAY_NODE: return "I_RELAY_NODE";
		case I_PING: return "I_PING";
		case I_PING_ACK: return "I_PING_ACK";
		case I_LOG_MESSAGE: return "I_LOG_MESSAGE";
		case I_CHILDREN: return "I_CHILDREN";
		case I_UNIT: return "I_UNIT";
		case I_SKETCH_NAME: return "I_SKETCH_NAME";
		case I_SKETCH_VERSION: return "I_SKETCH_VERSION";
	}
}
