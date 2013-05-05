#include "net_manager.h"
#include "netpack.h"
#include "client_info.h"
#include <Poco/Logger.h>
#include <Poco/Util/Application.h>

using Poco::Util::Application;

NetManager::NetManager()
    :logger_(Application::instance().logger())
{
}

NetManager::~NetManager(){
}

retcode_t NetManager::Send(const ClientInfo& peerInfo, const NetPack& content){
    HANDLE key = this->GetSockHandler(peerInfo);
    SockCollection::Iterator pos = socks_.find( key );
    if( socks_.end() == pos ){
        //never start a connection, so return ERROR
        return ERROR_NET_NOT_CONNECTED;
    }

    SockPtr sock = socks_[key];
    return content.sendBy(*sock);
}

retcode_t NetManager::SendData(const ClientInfo& peerInfo, const string& content){
    HANDLE key = this->GetSockHandler(peerInfo);
    SockCollection::Iterator pos = socks_.find( key );
    if( socks_.end() == pos ){
        //never start a connection, so return ERROR
        return ERROR_NET_NOT_CONNECTED;
    }

    SockPtr sock = socks_[key];
    if( content.length() != sock->sendBytes(content.data(), content.length()) ){
        return ERROR_NET_SEND_DATA;
    }
    return ERROR_OK;
}

retcode_t NetManager::Recv(const ClientInfo& peerInfo, NetPack* content){
    /*
    HANDLE key = this->GetSockHandler(peerInfo);
    SockCollection::Iterator pos = socks_.find( key );
    if( sock_s.end() == pos ){
        SockPtr sock;
        if( ERROR_OK != make_sock(peerInfo, sock) ){
            return ERROR_NET_CONNECT;
        }
        socks_[key] = sock;
    }

    content->clear();
    return content->receiveFrom( *sock );
    */
    return ERROR_OK;
}

retcode_t NetManager::Connect(const ClientInfo& peerInfo){
    /*
    SockPtr sock( new StreamSocket );
    try{
        SocketAddress addr(peerInfo.Ip(), peerInfo.MessagePort() );
    }
    */
    return ERROR_OK;
}

/*
retcode_t NetManager::Bind(const ClientInfo& localInfo){
    HANDLE key = this->GetSockHandler
}
*/

NetManager::HANDLE NetManager::GetSockHandler(const ClientInfo& info){
    return "";
}
