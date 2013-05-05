#include "tracker.h"
#include "netpack.h"
#include "tracker_connection_handler.h"
#include "tracker_db_manager.h"
#include "file_owner_info.h"

#include <iostream>
#include "Poco/Logger.h"
#include "Poco/Net/SocketReactor.h"
#include "Poco/Net/SocketAcceptor.h"
#include "Poco/Net/StreamSocket.h"
#include "Poco/Net/ServerSocket.h"
#include "Poco/Exception.h"
#include "Poco/Thread.h"
#include "Poco/Util/ServerApplication.h"
#include "Poco/Util/Option.h"
#include "Poco/Util/OptionSet.h"
#include "Poco/Util/HelpFormatter.h"
#include "Poco/Data/Session.h"
#include <Poco/DateTimeFormat.h>
#include <Poco/DateTimeFormatter.h>
#include <Poco/LocalDateTime.h>
#include <Poco/Exception.h>
#include <Poco/Data/MySQL/Connector.h>

using Poco::Logger;
using Poco::Net::SocketReactor;
using Poco::Net::SocketAcceptor;
using Poco::Net::ReadableNotification;
using Poco::Net::ShutdownNotification;
using Poco::Net::ServerSocket;
using Poco::Net::StreamSocket;
using Poco::NObserver;
using Poco::AutoPtr;
using Poco::Thread;
using Poco::Util::ServerApplication;
using Poco::Util::Application;
using Poco::Util::Option;
using Poco::Util::OptionSet;
using Poco::Util::HelpFormatter;
using Poco::Data::Session;
using Poco::LocalDateTime;
using Poco::Exception;

Tracker::Tracker()
{
}
	
Tracker::~Tracker()
{
}

void Tracker::initialize(Application& self)
{
    loadConfiguration(); // load default configuration files, if present
    ServerApplication::initialize(self);

    /*
    Logger* pLogger = Logger::has("ConsoleLogger");
    if( pLogger == NULL ){
        poco_error(logger(), "No ConsoleLogger found!");
    }else{
        setLogger( *pLogger );
        poco_notice(logger(), "ConsoleLogger set succeed.");
    }
    */
    Poco::Data::MySQL::Connector::registerConnector();
    dbManager_.assign(new TrackerDBManager);
    if( dbManager_.isNull() ){
        poco_error(logger(), "Cannot create TrackerDBManager!");
    }
    timeFormat_ = Poco::DateTimeFormat::HTTP_FORMAT;
}
		
void Tracker::uninitialize()
{
    ServerApplication::uninitialize();
}

int Tracker::main(const std::vector<std::string>& args)
{
        if( !isInteractive() ){
            setLogger( Logger::get("FileLogger") );
        }else{
            setLogger( Logger::get("ConsoleLogger") );
        }

    try{
        unsigned short port = (unsigned short) config().getInt("Tracker.port", 9977);

        ServerSocket svs(port);
        svs.setReusePort(false);

        SocketReactor reactor;
        SocketAcceptor<TrackerConnectionHandler> acceptor(svs, reactor);


        Thread thread;
        thread.start(reactor);

        waitForTerminationRequest();

        reactor.stop();
        thread.join();

    }catch(Exception& e){
        poco_error_f1(logger(), "Got exception in Tracker::main : %s", e.message());
        return Application::EXIT_TEMPFAIL;
    }
    return Application::EXIT_OK;
}

retcode_t Tracker::AddOnlineUser(ClientPtr peer){
        string loginTime( this->get_current_time() );
        retcode_t ret = dbManager_->insert_or_update_login_info(peer->clientid(), peer->ip(), peer->messageport(), loginTime);
        return ret;
}

retcode_t Tracker::RemoveOnlineUser(const string& clientid){
    Int64 uploadTotal = 0;
    Int64 downloadTotal = 0;
    retcode_t ret = dbManager_->update_logout_info(clientid, uploadTotal, downloadTotal);
    return ret;
}

retcode_t Tracker::RequestClients(const string& fileid, int percentage, int needCount,
        const ClientIdCollection& ownedClientIdList, ClientFileInfoCollection* clients){
    clients->clear();
    if( fileid.empty() || percentage < 0 || percentage > 100 || needCount < 0 || clients == NULL){
        poco_warning(logger(), "invalid arg for Tracker::RequestClients.");
        return ERROR_UNKNOWN;
    }
    /*
    return dbManager_->search_near_percentage_client(fileid, percentage, needCount, ownedClientIdList, clients);
    */
    retcode_t ret = dbManager_->search_greater_percentage(fileid, percentage, needCount, ownedClientIdList, clients);
    if( ret != ERROR_OK ){
        return ret;
    }

    if( clients->size() < needCount && percentage >0 ){
        ret = dbManager_->search_less_equal_percentage(fileid, percentage, needCount - clients->size(), ownedClientIdList, clients);
        if( ret != ERROR_OK ){
            return ret;
        }
    }

    poco_debug_f2(logger(), "request clients for %s return %u clients", fileid, clients->size());
    return ERROR_OK;
}

retcode_t Tracker::ReportProgress(const string& clientid, const string& fileid, int percentage){
    FileOwnerInfo info(clientid, fileid, percentage);
    return dbManager_->update_file_owner_info(info);
}

retcode_t Tracker::PublishResource(const string& clientid, const string& fileid){
    FileOwnerInfo info(clientid, fileid, 100);
    return dbManager_->insert_file_owner_info(info);
}

string Tracker::get_current_time() const{
    LocalDateTime now;
    return Poco::DateTimeFormatter::format(now, timeFormat_);
}
