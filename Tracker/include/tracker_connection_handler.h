#ifndef CONNECTION_HANDLER_H
#define CONNECTION_HANDLER_H

#include "tracker.pb.h"
#include "error_code.h"
#include <string>
#include <Poco/Logger.h>
#include <Poco/Net/SocketReactor.h>
#include <Poco/Net/SocketNotification.h>
#include <Poco/Net/StreamSocket.h>
#include <Poco/SharedPtr.h>
#include <Poco/NObserver.h>
#include <Poco/Data/Session.h>
#include <google/protobuf/message.h>

using std::string;
using Poco::Logger;
using Poco::Net::SocketReactor;
using Poco::Net::ReadableNotification;
using Poco::Net::ShutdownNotification;
using Poco::Net::StreamSocket;
using Poco::SharedPtr;
using Poco::NObserver;
using Poco::AutoPtr;
using google::protobuf::Message;

class Tracker;
class NetPack;

using namespace TrackerProto;

class TrackerConnectionHandler
{
    public:
        TrackerConnectionHandler(const StreamSocket& sock, SocketReactor& reactor);
        ~TrackerConnectionHandler();

        void onReadable(const AutoPtr<ReadableNotification>& pNotification);
        void onShutdown(const AutoPtr<ShutdownNotification>& pNotification);


    private:
        const static size_t BUF_LEN = 1 << 10;

        void Process(const NetPack& in, NetPack* out);
        retcode_t HandleLogin(SharedPtr<Message> in, MessageReply* out);
        retcode_t HandleLogOut(SharedPtr<Message> in, MessageReply* out);
        retcode_t HandleRequestPeer(SharedPtr<Message> in, QueryPeerReply* out);
        retcode_t HandleReportProgress(SharedPtr<Message> in, MessageReply* out);
        retcode_t HandlePublishResource(SharedPtr<Message> in, MessageReply* out);

        //proto itself is changed in the func ParseProto, so it must be a ref
        retcode_t ParseProto(const string& name, Message* &proto);
        /*
        */

        string clientid;
        StreamSocket sock_;
        SocketReactor& reactor_;
        Logger& logger_;
        Tracker& app_;
};
#endif
