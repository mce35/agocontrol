#include <stdexcept>
#include <sstream>
#include "agoproto.h"

using namespace qpid::types;

static const qpid::types::Variant::Map EMPTY_DATA;
static const std::string EMPTY_STRING;
qpid::types::Variant::Map agocontrol::responseResult(const std::string& identifier)
{
    return responseResult(identifier, EMPTY_STRING, EMPTY_DATA);
}

qpid::types::Variant::Map agocontrol::responseResult(const std::string& identifier, const std::string& message)
{
    return responseResult(identifier, message, EMPTY_DATA);
}

qpid::types::Variant::Map agocontrol::responseResult(const std::string& identifier, const qpid::types::Variant::Map& data)
{
    return responseResult(identifier, EMPTY_STRING, data);
}

qpid::types::Variant::Map agocontrol::responseResult(const std::string& identifier, const std::string& message, const qpid::types::Variant::Map& data)
{
    qpid::types::Variant::Map result;

    if (identifier.empty())
        throw std::invalid_argument("Response without identifier not permitted");

    result["identifier"] = identifier;

    if(!message.empty())
        result["message"] = message;

    if(!data.empty())
        result["data"] = data;

    qpid::types::Variant::Map response;
    response["result"] = result;
    response["_newresponse"] = true; // TODO: remove thits after everything is using new response style
    return response;
}

qpid::types::Variant::Map agocontrol::responseError(const std::string& identifier, const std::string& message)
{
    return responseError(identifier, message, EMPTY_DATA);
}

qpid::types::Variant::Map agocontrol::responseError(const std::string& identifier, const std::string& message, const qpid::types::Variant::Map& data)
{
    qpid::types::Variant::Map error;

    if (identifier.empty())
        throw std::invalid_argument("Response without identifier not permitted");

    if (message.empty())
        throw std::invalid_argument("Error response without message not permitted");

    error["identifier"] = identifier;
    error["message"] = message;

    if(!data.empty())
        error["data"] = data;

    qpid::types::Variant::Map response;
    response["error"] = error;
    response["_newresponse"] = true; // TODO: remove thits after everything is using new response style
    return response;
}


/* Shortcuts to respond with basic FAILED error */

qpid::types::Variant::Map agocontrol::responseFailed(const std::string& message)
{
    return responseError(RESPONSE_ERR_FAILED, message, EMPTY_DATA);
}

qpid::types::Variant::Map agocontrol::responseFailed(const std::string& message, const qpid::types::Variant::Map& data)
{
    return responseError(RESPONSE_ERR_FAILED, message, data);
}

/* Shortcuts to respond with basic SUCCESS result */
qpid::types::Variant::Map agocontrol::responseSuccess()
{
    return responseResult(RESPONSE_SUCCESS, EMPTY_STRING, EMPTY_DATA);
}

qpid::types::Variant::Map agocontrol::responseSuccess(const std::string& message)
{
    return responseResult(RESPONSE_SUCCESS, message, EMPTY_DATA);
}

qpid::types::Variant::Map agocontrol::responseSuccess(const qpid::types::Variant::Map& data)
{
    return responseResult(RESPONSE_SUCCESS, EMPTY_STRING, data);
}

qpid::types::Variant::Map agocontrol::responseSuccess(const std::string& message, const qpid::types::Variant::Map& data)
{
    return responseResult(RESPONSE_SUCCESS, message, data);
}


/* Helper to check incoming messages */
void agocontrol::checkMsgParameter(/*const */qpid::types::Variant::Map& content, const std::string& key) {
    if(!content.count(key.c_str()) || content[key.c_str()].isVoid()) {
        std::stringstream err;
        err << "Parameter " << key << " is required";
        throw AgoCommandException(RESPONSE_ERR_PARAMETER_MISSING, err.str());
    }
}
void agocontrol::checkMsgParameter(/*const */qpid::types::Variant::Map& content, const std::string& key,
        qpid::types::VariantType type, bool allowEmpty) {
    checkMsgParameter(content, key);

    const char *ckey = key.c_str();

    qpid::types::VariantType msgType = content[ckey].getType();
    try {
        // For INT types, try to make an actual conversion to the target
        // type. If it is not possible, an InvalidConversion will be thrown.
        // We could re-implement the QPID probe code ourselfs, but this is easier..
        switch(type) {
            case VAR_UINT8:
                content[ckey].asUint8();
                return;
            case VAR_UINT16:
                content[ckey].asUint16();
                return;
            case VAR_UINT32:
                content[ckey].asUint32();
                return;
            case VAR_UINT64:
                content[ckey].asUint64();
                return;
            case VAR_INT8:
                content[ckey].asInt8();
                return;
            case VAR_INT16:
                content[ckey].asInt16();
                return;
            case VAR_INT32:
                content[ckey].asInt32();
                return;
            case VAR_INT64:
                content[ckey].asInt64();
                return;
            case VAR_BOOL:
                content[ckey].asBool();
                return;
            default:
                // Not an int type
                if(msgType != type) {
                    std::stringstream err;
                    err << "Parameter " << key << " has invalid type";
                    throw AgoCommandException(RESPONSE_ERR_PARAMETER_INVALID, err.str());
                }
        }
    }catch(const qpid::types::InvalidConversion& ex) {
        std::stringstream err;
        err << "Parameter " << key << " has invalid integer type: " << ex.what();
        throw AgoCommandException(RESPONSE_ERR_PARAMETER_INVALID, err.str());
    }

    if(msgType == VAR_STRING && !allowEmpty && content[ckey].getString().empty()) {
        std::stringstream err;
        err << "Parameter " << key << " must not be empty";
        throw AgoCommandException(RESPONSE_ERR_PARAMETER_INVALID, err.str());
    }
}

void agocontrol::AgoResponse::init(const qpid::messaging::Message& message) {
    if (message.getContentSize() > 3) {
        decode(message, response);
    }else{
        qpid::types::Variant::Map err;
        err["message"] = "invalid.response";
        response["error"] = err;
    }

    validate();
}

void agocontrol::AgoResponse::init(const qpid::types::Variant::Map& response_) {
    response = response_;
    validate();
}

void agocontrol::AgoResponse::validate() {
    if(isError() && isOk())
        throw std::invalid_argument("error and result are mutually exclusive");

    if(isOk()) {
        if(response["result"].getType() != VAR_MAP)
            throw std::invalid_argument("result must be map");

        root = response["result"].asMap();
    }
    else if(isError()) {
        if(response["error"].getType() != VAR_MAP)
            throw std::invalid_argument("error must be map");

        root = response["error"].asMap();
    }
    else {
        throw std::invalid_argument("error or result must be set");
    }

    if(!root.count("identifier"))
        throw std::invalid_argument("identifier must be set");

    if(isError()) {
        if(!root.count("message"))
            throw std::invalid_argument("error.message must be set");
    }

    if(root.count("data") && root["data"].getType() != VAR_MAP)
        throw std::invalid_argument("data must be a map");
}


bool agocontrol::AgoResponse::isError() const {
    return response.count("error") == 1;
}

bool agocontrol::AgoResponse::isOk() const {
    return response.count("result") == 1;
}

std::string agocontrol::AgoResponse::getIdentifier() {
    return root["identifier"].asString();
}

std::string agocontrol::AgoResponse::getMessage() {
    if(root.count("message"))
        return root["message"].asString();
    else
        return std::string();
}


qpid::types::Variant::Map agocontrol::AgoResponse::getData() {
    if(root.count("data"))
        return root["data"].asMap();
    else
        return qpid::types::Variant::Map();
}

