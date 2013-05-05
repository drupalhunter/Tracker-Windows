#include "tracker_connection_handler.h"
#include "netpack.h"
#include "tracker.h"
#include "payload_type.h"
#include <Poco/Util/Application.h>
#include <Poco/Logger.h>
#include <google/protobuf/descriptor.h>
#include <Poco/Exception.h>
#include <iostream>
using namespace std;

using Poco::Util::Application;
using Poco::Exception;

TrackerConnectionHandler::TrackerConnectionHandler(const StreamSocket& sock, SocketReactor& reactor):
    sock_(sock),
    reactor_(reactor),
    logger_( Application::instance().logger() ),
    app_( dynamic_cast<Tracker&>(Application::instance()) )
{
    poco_information_f1(logger_, "Connection from %s", sock_.peerAddress().toString() );

    reactor_.addEventHandler(sock_, 
                        NObserver<TrackerConnectionHandler, ReadableNotification>(*this, &TrackerConnectionHandler::onReadable));

    reactor_.addEventHandler(sock_, 
                        NObserver<TrackerConnectionHandler, ShutdownNotification>(*this, &TrackerConnectionHandler::onShutdown));
}

TrackerConnectionHandler::~TrackerConnectionHandler()
{
    poco_information_f1(logger_, "Disconnect from %s", sock_.peerAddress().toString() );

    reactor_.removeEventHandler(sock_, 
                        NObserver<TrackerConnectionHandler, ReadableNotification>(*this, &TrackerConnectionHandler::onReadable));

    reactor_.removeEventHandler(sock_, 
                        NObserver<TrackerConnectionHandler, ShutdownNotification>(*this, &TrackerConnectionHandler::onShutdown));
}

void TrackerConnectionHandler::onReadable(const AutoPtr<ReadableNotification>& pNotification)
{

    NetPack pack;
    NetPack retMsg;

    int ret = pack.receiveFrom( sock_ );

    if( ret ){
        if( ret == ERROR_NET_GRACEFUL_SHUTDOWN ){
            poco_notice_f1(logger_, "graceful shutdown by remote peer : %s.", sock_.peerAddress().toString() );
        }else{
            poco_warning_f2(logger_, "pack.receiveFrom error! ret : %d, remote addr : %s.", 
                    ret, sock_.peerAddress().toString());
        }
            goto err;
    }

    poco_notice_f1(logger_, "header : \n%s", pack.debug_string() );
    //poco_notice_f1(logger_, "Message DebugString : \n%s", pack.message()->DebugString() );

    this->Process(pack, &retMsg);

    ret = retMsg.sendBy( sock_ );
    if( ret != ERROR_OK ){
        poco_warning_f2( logger_, "pack.sendBy error ! ret : %d, remote addr : %s.", 
                ret, sock_.peerAddress().toString() );
        goto err;
    }else{
        poco_notice_f2( logger_, "process 1 request from %s, payload type : %d", 
                sock_.peerAddress().toString(), pack.payloadtype() );
    }
    return;

err:
    app_.RemoveOnlineUser(this->clientid);
    delete this;
    return;
}

void TrackerConnectionHandler::onShutdown(const AutoPtr<ShutdownNotification>& pNotification)
{
    poco_information_f1(logger_, "shutdown connection from %s.", sock_.peerAddress().toString());
    delete this;
}

void TrackerConnectionHandler::Process(const NetPack& in, NetPack* out){
    SharedPtr<Message> m = in.message();
    if( m.isNull() ){
        poco_warning_f1(logger_, "Cannot get message from netpack in. debug_string : \n%s", in.debug_string());
        return;
    }
    MessageReply mr;
    mr.set_returncode(ERROR_UNKNOWN);
    QueryPeerReply qpr;
    qpr.set_returncode(ERROR_UNKNOWN);
    try{

        switch( in.payloadtype() ){
            case PAYLOAD_LOGIN: 
                this->HandleLogin(m, &mr);
                 break;
            case PAYLOAD_LOGOUT: 
                this->HandleLogOut(m, &mr);
                 break;
            case PAYLOAD_REQUEST_PEER:
                 this->HandleRequestPeer(m, &qpr);
                 break;
            case PAYLOAD_REPORT_PROGRESS:
                this->HandleReportProgress(m, &mr);
                 break;
            case PAYLOAD_PUBLISH_RESOURCE:
                this->HandlePublishResource(m, &mr);
                 break;
            default:
                 poco_warning_f2(logger_, "Unknown PayloadType : %d , remote addr : %s.", 
                         in.payloadtype(), sock_.peerAddress().toString() );
                 break;
        }
    }catch(Exception& e){
        poco_warning_f3(logger_, "Got exception while Process Request, client ip : %s, netpack debug string : \n%sException content:\n%s", 
                sock_.peerAddress().toString(), in.debug_string(), e.message());
    }

    if( in.payloadtype() != PAYLOAD_REQUEST_PEER ){
        out->set_message( PAYLOAD_MESSAGE_REPLY, mr );
    }else{
        out->set_message( PAYLOAD_REQUEST_PEER_REPLAY, qpr);
    }
}

retcode_t TrackerConnectionHandler::HandleLogin(SharedPtr<Message> in, MessageReply* out){
    SharedPtr<Login> loginProto = in.cast<Login>();
    retcode_t ret = ERROR_UNKNOWN;
    out->set_returncode(ERROR_UNKNOWN);

    if( loginProto.isNull() ){
        return ERROR_PROTO_TYPE_ERROR;
    }

    string ip = loginProto->has_loginip() ? loginProto->loginip() : sock_.peerAddress().host().toString();
    this->clientid = loginProto->clientid();
    poco_notice_f3(logger_, "client login, id : %s, ip : %s, port : %d", loginProto->clientid(), ip, loginProto->messageport());
    Tracker::ClientPtr client( new ClientInfo(loginProto->clientid(), ip, loginProto->messageport()) );
    ret = app_.AddOnlineUser(client);

    if( ret != ERROR_OK ){
        poco_warning_f3(logger_,"Cannot add online user, client id : %s, client ip : %s, retCode : %d.", 
                loginProto->clientid(), ip, ret);
        return ret;
    }

    out->set_returncode(ERROR_OK);
    return ERROR_OK;
}

retcode_t TrackerConnectionHandler::HandleLogOut(SharedPtr<Message> in, MessageReply* out){
    SharedPtr<Logout> logoutProto = in.cast<Logout>();
    retcode_t ret = ERROR_UNKNOWN;
    out->set_returncode(ERROR_UNKNOWN);

    if( logoutProto.isNull() ){
        return ERROR_PROTO_TYPE_ERROR;
    }
    if( logoutProto->clientid() != this->clientid ){
        poco_warning_f2(logger_, "Logout clientid doesn't fit the login client id. client id :%s, client ip :%s",
                this->clientid, sock_.peerAddress().toString() );
        return ERROR_UNKNOWN;
    }
    ret = app_.RemoveOnlineUser(logoutProto->clientid());
    if( ret != ERROR_OK ){
        poco_warning_f3(logger_,"Cannot remove online user, client id : %s, client ip : %s, retCode : %d.", 
                logoutProto->clientid(), sock_.peerAddress().toString(), ret);
        return ret;
    }
    out->set_returncode(ERROR_OK);
    return ERROR_OK;
}

retcode_t TrackerConnectionHandler::HandleRequestPeer(SharedPtr<Message> in, QueryPeerReply* out){
    SharedPtr<QueryPeer> queryProto = in.cast<QueryPeer>();
    retcode_t ret = ERROR_UNKNOWN;

    if( queryProto.isNull() ){
        return ERROR_PROTO_TYPE_ERROR;
    }
    poco_notice_f1(logger_, "fileid=%s", queryProto->fileid());
    poco_notice_f1(logger_, "percentage=%d", queryProto->percentage());
    poco_notice_f1(logger_, "needcount=%d", queryProto->needcount());

    out->set_fileid( queryProto->fileid() );

    Tracker::ClientIdCollection idCollection( queryProto->ownedclients().begin(), queryProto->ownedclients().end() );
    //add this clientid to known client lists
    idCollection.push_back(this->clientid);

    Tracker::ClientFileInfoCollection fileInfoCollection;

    ret = app_.RequestClients(queryProto->fileid(), queryProto->percentage(), queryProto->needcount(), 
            idCollection, &fileInfoCollection); 
    if( ret != ERROR_OK ){
        out->set_count(0);
        poco_warning_f4(logger_,"Cannot request clients, file id : %s, percentage : %d, need count : %d, client ip : %s", 
                queryProto->fileid(), queryProto->percentage(), queryProto->needcount(), sock_.peerAddress().toString());
        return ret;
    }

    poco_debug_f3(logger_, "Get %d clients of file '%s' for %s", 
            (int)fileInfoCollection.size(), out->fileid(), sock_.peerAddress().toString());

    out->set_count( fileInfoCollection.size() );
    for(int i = 0; i != fileInfoCollection.size(); ++i){
        std::pair<Tracker::ClientPtr, Tracker::FileOwnerInfoPtr>& v = fileInfoCollection[i];
        PeerFileInfo* info = out->add_info();
        Peer* p = info->mutable_client();
        p->set_clientid( v.first->clientid() );
        p->set_ip( v.first->ip() );
        p->set_messageport( v.first->messageport() );
        info->set_percentage( v.second->percentage() );
    }

    out->set_returncode(ERROR_OK);
    return ERROR_OK;
}

retcode_t TrackerConnectionHandler::HandleReportProgress(SharedPtr<Message> in, MessageReply* out){
    SharedPtr<ReportProgress> reportProto = in.cast<ReportProgress>();
    retcode_t ret = ERROR_UNKNOWN;
    out->set_returncode( ERROR_UNKNOWN );

    if( reportProto.isNull() ){
        return ERROR_PROTO_TYPE_ERROR;
    }

    ret = app_.ReportProgress(reportProto->clientid(), reportProto->fileid(), reportProto->percentage());
    poco_debug_f3(logger_, "recv report progress from '%s', fileid : '%s', percentage '%d'",
            reportProto->clientid(), reportProto->fileid(), reportProto->percentage() );
    if( ret != ERROR_OK ){
        return ret;
    }

    out->set_returncode(ERROR_OK);
    return ERROR_OK;
}

retcode_t TrackerConnectionHandler::HandlePublishResource(SharedPtr<Message> in, MessageReply* out){
    SharedPtr<PublishResource> publishProto = in.cast<PublishResource>();
    retcode_t ret = ERROR_UNKNOWN;
    out->set_returncode( ERROR_UNKNOWN );

    if( publishProto.isNull() ){
        return ERROR_PROTO_TYPE_ERROR;
    }

    ret = app_.PublishResource( publishProto->clientid(),  publishProto->fileid() );
    if( ret != ERROR_OK ){
        return ret;
    }

    out->set_returncode( ERROR_OK );
    return ERROR_OK;
}

retcode_t TrackerConnectionHandler::ParseProto(const string& name, Message* &proto){
    using namespace google::protobuf;
    proto = NULL;
    const Descriptor* descriptor = DescriptorPool::generated_pool()->FindMessageTypeByName(name);
    if (descriptor)
    {
        const Message* prototype = MessageFactory::generated_factory()->GetPrototype(descriptor);
        if (prototype)
        {
          proto = prototype->New();
        }
    }
    return ERROR_OK;
}
