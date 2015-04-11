#ifndef AGOPROTO_H
#define AGOPROTO_H

#include <string>
#include <qpid/messaging/Message.h>

#include "response_codes.h"

namespace agocontrol {
    /**
     * These methods shall be used to build valid command handler response maps
     */
    qpid::types::Variant::Map responseResult(const std::string& identifier);
    qpid::types::Variant::Map responseResult(const std::string& identifier, const std::string& message);
    qpid::types::Variant::Map responseResult(const std::string& identifier, const std::string& message, const qpid::types::Variant::Map& data);
    qpid::types::Variant::Map responseResult(const std::string& identifier, const qpid::types::Variant::Map& data);

    qpid::types::Variant::Map responseError(const std::string& identifier, const std::string& message, const qpid::types::Variant::Map& data);
    qpid::types::Variant::Map responseError(const std::string& identifier, const std::string& message);

    // Shortcut to send responseError(RESPONSE_ERR_FAILED, message)
    qpid::types::Variant::Map responseFailed(const std::string& message);
    qpid::types::Variant::Map responseFailed(const std::string& message, const qpid::types::Variant::Map& data);

#define responseUnknownCommand() \
    responseError(RESPONSE_ERR_UNKNOWN_COMMAND, "Command not supported");

#define responseNoDeviceCommands() \
    responseError(RESPONSE_ERR_NO_DEVICE_COMMANDS, "Device does not have any commands")

    // Shortcut to send responseResult(RESPONSE_SUCCESS, ...)
    qpid::types::Variant::Map responseSuccess();
    qpid::types::Variant::Map responseSuccess(const std::string& message);
    qpid::types::Variant::Map responseSuccess(const qpid::types::Variant::Map& data);
    qpid::types::Variant::Map responseSuccess(const std::string& message, const qpid::types::Variant::Map& data);


    /**
     * An exception which can be thrown from a CommandHandler in order to signal failure.
     * This will be catched by the code calling the CommandHandler, and will then be
     * translated into a proper response.
     */
    class AgoCommandException: public std::exception {
    public:
        AgoCommandException(const std::string& identifier_, const std::string& message_)
            : identifier(identifier_)
            , message(message_)
        {
        }
        ~AgoCommandException() _NOEXCEPT {};

        qpid::types::Variant::Map toResponse() const {
            return responseError(identifier, message);
        }

        const char* what() const _NOEXCEPT {
            return identifier.c_str();
        }

    private:
        const std::string identifier;
        const std::string message;
    };

    /**
     * Precondition functions to verify requests, and possibly throw AgoCommandExceptions
     */
    
    // Ensure that the message map has an 'key' entry, with a non-void value
    void checkMsgParameter(/*const */qpid::types::Variant::Map& content, const std::string& key);

    // Ensure that the message map has an 'key' entry, with a non-void value
    // of the specified type. If allowEmpty is set to false (default), and type is string,
    // we check for empty string too. For other types, allowEmpty is ignored.
    void checkMsgParameter(/*const */qpid::types::Variant::Map& content, const std::string& key,
            qpid::types::VariantType type,
            bool allowEmpty=false);



    /**
     * When sending requests FROM an app, AgoClient will deserialize them into a 
     * AgoResponse object which can be interacted with easilly
     */
    class AgoConnection;
    class AgoResponse {
        friend class AgoConnection;
    protected:
        qpid::types::Variant::Map response;
        qpid::types::Variant::Map root;
        AgoResponse(){};
        void init(const qpid::messaging::Message& message);
        void init(const qpid::types::Variant::Map& response);
        void validate();
    public:

        // Return true if we have an "error" element
        bool isError() const;

        // Return true if we have a "result" element
        bool isOk() const;

        // Get the "identifier" field from either type of response
        std::string getIdentifier() /*const*/;

        // Get the "message" field from either type of response
        std::string getMessage();

        // Get either "result.data" or "error.data"
        // Note that this creates a copy; Variant::map does not allow get without copy
        // before C++-11
        /*const*/ qpid::types::Variant::Map/*&*/ getData() /*const*/;
    };

}/* namespace agocontrol */

#endif
