#ifndef NET_MANAGER_H
#define NET_MANAGER_H

#include "error_code.h"

#include <string>

#include <Poco/HashMap.h>
#include <Poco/Net/StreamSocket.h>
#include <Poco/SharedPtr.h>
#include <Poco/Logger.h>

using std::string;
using Poco::HashMap;
using Poco::Net::StreamSocket;
using Poco::SharedPtr;
using Poco::Logger;

class ClientInfo;
class NetPack;

class NetManager{
    public:
        NetManager();
        ~NetManager();

        retcode_t Send(const ClientInfo& peerInfo, const NetPack& content);
        retcode_t SendData(const ClientInfo& peerInfo, const string& content);
        retcode_t Recv(const ClientInfo& peerInfo, NetPack* content);
        retcode_t Connect(const ClientInfo& peerInfo);
        
        retcode_t Bind(const ClientInfo& localInfo);

    private:
        typedef SharedPtr<StreamSocket> SockPtr;
        typedef string HANDLE;
        typedef HashMap<HANDLE, SockPtr> SockCollection;
        HANDLE GetSockHandler(const ClientInfo& info);

        SockCollection socks_;
        Logger& logger_;
};
#endif
