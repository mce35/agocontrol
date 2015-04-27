/**
 * Defines a set standard response codes which applications should try to keep to.
 */

#ifndef RESPONSE_CODES_H
#define RESPONSE_CODES_H

// Default response result for generic sucess
#define RESPONSE_SUCCESS        "success"

// failed with no further reason
#define RESPONSE_ERR_FAILED                 "error.failed"
#define RESPONSE_ERR_UNKNOWN_COMMAND        "error.unknown.command"
#define RESPONSE_ERR_NO_DEVICE_COMMANDS     "error.no.device.commands"
#define RESPONSE_ERR_PARAMETER_MISSING      "error.parameter.missing"
#define RESPONSE_ERR_PARAMETER_INVALID      "error.parameter.invalid"
#define RESPONSE_ERR_NOT_FOUND              "error.not.found"

// Lowlevel communications error
#define RESPONSE_ERR_NO_REPLY               "error.no.reply"
#define RESPONSE_ERR_INTERNAL               "error.internal"

#endif
