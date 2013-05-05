#ifndef NETPACK_H
#define NETPACK_H
#include <string>

#include "netpack_header.pb.h"
#include "error_code.h"
#include <Poco/Net/StreamSocket.h>
#include <Poco/Logger.h>
#include <Poco/Format.h>
#include <Poco/Types.h>
#include <Poco/Buffer.h>
#include <Poco/SharedPtr.h>
#include <google/protobuf/message.h>

using std::string;
using Poco::Net::StreamSocket;
using Poco::Logger;
using Poco::format;
using Poco::Int32;
using Poco::Buffer;
using Poco::SharedPtr;
using google::protobuf::Message;

#pragma comment(lib,"WS2_32.LIB")

class NetPack{
    public:
        typedef StreamSocket SockType;
        NetPack();
        NetPack(int payloadType, const Message& msg);

        retcode_t sendBy(SockType& sock) const;
        retcode_t receiveFrom(SockType& sock);

        SharedPtr<Message> message() const;
        void set_message(int payloadType, const Message& msg);

        void clear(){
            this->payloadType_ = 0;
            this->messageName_.clear();
            this->payload_.clear();
            this->header_.Clear();
        };

        void set_payloadtype(int t){
            this->payloadType_ = t;
        }

        void set_payload(const string& payload){
            this->payload_ = payload;
        }

        int payloadtype() const{
            return this->payloadType_;
        };

        string payload() const {
            return this->payload_;
        };

        string messageName() const{
            return this->messageName_;
        }

        string debug_string() const{
            return format("type : %d\nname : %s\nlength : %d\npayload : %s", this->payloadType_, this->messageName_, 
                    header_.payloadlength(), this->payload_);
        }

    private:
        retcode_t sendHeaderLength(SockType& sock) const;
        retcode_t receiveHeaderLength(SockType& sock, Int32* headerLength);

        int payloadType_;
        string payload_;
        string messageName_;

        NetPackHeader header_;
        Logger& logger_;
        Buffer<char> headerLengthBuf_;
};

#endif
