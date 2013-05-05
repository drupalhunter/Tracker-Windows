#include "tracker_db_manager.h"
#include <utility>
#include <Poco/Util/Application.h>
#include <Poco/Exception.h>
#include <Poco/Data/SessionFactory.h>
#include <Poco/Data/Statement.h>
#include <Poco/Data/RecordSet.h>

using std::make_pair;
using Poco::format;
using Poco::Util::Application;
using Poco::Exception;
using Poco::Data::SessionFactory;
using Poco::Data::Statement;
using Poco::Data::Limit;

std::string connectionString = "user=root;password=123456;db=test;compress=true;auto-reconnect=true;host=127.0.0.1;port=3306";

TrackerDBManager::TrackerDBManager()
    :logger_(Application::instance().logger()),
    session_(SessionFactory::instance().create("MySQL", connectionString)),
    clientInfoTableName_("CLIENTINFO"),
    fileOwnerTableName_("FILEOWNERINFO"){

        //Poco::Data::MySQL::Connector::registerConnector();
        retcode_t ret = this->init_tables();
        if( ret != ERROR_OK ){
            poco_error_f1(logger_, "in TrackerDBManager, Init tables error, retcode : %d.", ret);
        }
}

TrackerDBManager::~TrackerDBManager(){
        Poco::Data::MySQL::Connector::unregisterConnector();
}

retcode_t TrackerDBManager::insert_or_update_login_info(const string& clientid, const string& ip, int port, const string loginTime){
    FastMutex::ScopedLock lock(mutex_);
    using namespace Poco::Data;
    string sql;
    int init_upload_total = 0;
    int init_download_total = 0;
    format(sql, "INSERT INTO %s (CLIENTID, IP, MESSAGEPORT, UPLOADTOTAL, DOWNLOADTOTAL, CREATETIME, LASTLOGINTIME, ISONLINE) "
            "VALUES(?,?,?,?,?,?,?,1) "
            "ON DUPLICATE KEY UPDATE IP=VALUES(IP), MESSAGEPORT=VALUES(MESSAGEPORT), LASTLOGINTIME=VALUES(LASTLOGINTIME), ISONLINE=1", clientInfoTableName_);
    poco_debug_f4(logger_, "call TrackerDBManager::insert_or_update_login_info, clientid : %s, ip :%s, port : %d, loginTime : %s",
            clientid, ip, port, loginTime);
    session_ << sql, use(clientid), use(ip), use(port), use(init_upload_total), 
             use(init_download_total), use(loginTime), use(loginTime), now;
    return ERROR_OK;
}

retcode_t TrackerDBManager::update_logout_info(const string& clientid, Int64 downloadTotal, Int64 uploadTotal){
    FastMutex::ScopedLock lock(mutex_);
    using namespace Poco::Data;
    string sql;
    format(sql, "UPDATE %s SET ISONLINE = 0, UPLOADTOTAL = UPLOADTOTAL + ?, DOWNLOADTOTAL = DOWNLOADTOTAL + ? WHERE CLIENTID=?", clientInfoTableName_);
    session_ << sql, use(downloadTotal), use(uploadTotal), use(clientid), now;
    return ERROR_OK;
}
/*
retcode_t TrackerDBManager::search_client_info(const string& cientid, ClientInfo* pInfo){
    FastMutex::ScopedLock lock(mutex_);
    return ERROR_OK;
}
retcode_t TrackerDBManager::search_file_owner_info(const string& fileid, FileOwnerInfoPtrCollection* c){
    FastMutex::ScopedLock lock(mutex_);
    return ERROR_OK;
}
*/
retcode_t TrackerDBManager::search_near_percentage_client(const string& fileid, int percentage, int needCount,
        const ClientIdCollection& clientids, FileOwnerInfoCollection* c){
    if( needCount <= 0 ){
        poco_notice_f1(logger_, "search_near_percentage_client with invalid needCount=%d.", needCount);
        return ERROR_OK;
    }

    using namespace Poco::Data;
    FastMutex::ScopedLock lock(mutex_);
    string sql;
    //%[0]s -> clientInfoTableName_, %[1]s -> fileOwnerTableName_
    //%[2]s -> fileid, %[3]d -> percentage, %[4]s -> SearchType
    //return 4 cols, ie clientid, ip, messageport, percentage
    format( sql, 
    "SELECT * FROM "
    "("
    "(SELECT CLIENTID,IP,MESSAGEPORT,PERCENTAGE FROM %[0]s INNER JOIN %[1]s "
    "WHERE %[1]s.FILEID='%[2]s' AND %[0]s.CLIENTID=%[1]s.CLIENTID AND %[0]s.ISONLINE=1 AND %[1]s.PERCENTAGE <= %[3]d ORDER BY PERCENTAGE DESC) AS GREATER_RECORDS " 
    "UNION ALL "
    "(SELECT CLIENTID,IP,MESSAGEPORT,PERCENTAGE FROM %[0]s INNER JOIN %[1]s "
    "WHERE %[1]s.FILEID='%[2]s' AND %[0]s.CLIENTID=%[1]s.CLIENTID AND %[0]s.ISONLINE=1 AND %[1]s.PERCENTAGE > %[3]d ORDER BY PERCENTAGE ASC ) AS LESS_EQUAL_RECORDS"
    ") AS RECORDS WHERE PERCENTAGE != 0 ", clientInfoTableName_, fileOwnerTableName_, fileid, percentage);

    Statement select(session_);

    ClientIdCollection::const_iterator iter = clientids.begin();
    ClientIdCollection::const_iterator end = clientids.end();
    while( iter != end ){
        sql.append( format("AND %s.CLIENTID != '%s'", fileOwnerTableName_, *iter) );
        ++iter;
    }
    sql.append( format(" LIMIT %d", needCount) );

    poco_notice_f1(logger_, "in search_percentage_raw, next execute :\n%s", sql);
    select << sql;
    select.execute();
    RecordSet rs(select);

    bool more = rs.moveFirst();
    while(more){
        more = rs.moveNext();
        string clientid = rs[0].convert<string>();
        string ip = rs[1].convert<string>();
        int port = rs[2].convert<int>();
        int percentage = rs[3].convert<int>();
        ClientPtr pClient( new ClientInfo(clientid, ip, port) );
        FileOwnerInfoPtr pFile( new FileOwnerInfo(clientid, fileid, percentage) );
        c->push_back( make_pair(pClient, pFile) );
    }

    return ERROR_OK;
}

retcode_t TrackerDBManager::search_greater_percentage(const string& fileid, int percentage, int needCount,
    const ClientIdCollection& clientids, FileOwnerInfoCollection* c){
    return this->search_percentage_raw( GREATER, fileid, percentage, needCount, clientids, c);
}

retcode_t TrackerDBManager::search_less_equal_percentage(const string& fileid, int percentage, int needCount, const ClientIdCollection& clientids, FileOwnerInfoCollection* c){
    return this->search_percentage_raw( LESS_EQUAL, fileid, percentage, needCount, clientids, c);
}

retcode_t TrackerDBManager::search_percentage_raw(SearchType type, const string& fileid, int percentage, int needCount,
        const ClientIdCollection& clientids, FileOwnerInfoCollection* c){
    if( needCount <= 0 ){
        poco_notice_f1(logger_, "search_percentage_raw with invalid needCount=%d.", needCount);
        return ERROR_OK;
    }
    string typeString;
    if( type == GREATER ){
        typeString = ">";
    }else if(type == LESS_EQUAL){
        typeString = "<=";
    }else{
        return ERROR_UNKNOWN;
    }

    using namespace Poco::Data;
    FastMutex::ScopedLock lock(mutex_);
    string sql;
    //%[0]s -> clientInfoTableName_, %[1]s -> fileOwnerTableName_
    //%[2]s -> fileid, %[3]d -> percentage, %[4]s -> SearchType
    //return 4 cols, ie clientid, ip, messageport, percentage
    format(sql, "SELECT %[0]s.CLIENTID,IP,MESSAGEPORT,PERCENTAGE FROM %[0]s INNER JOIN %[1]s "
            "WHERE %[1]s.FILEID='%[2]s' AND %[0]s.CLIENTID=%[1]s.CLIENTID AND %[0]s.ISONLINE=1 AND %[1]s.PERCENTAGE %[4]s %[3]d ", clientInfoTableName_, fileOwnerTableName_, fileid, percentage, typeString);

    Statement select(session_);

    ClientIdCollection::const_iterator iter = clientids.begin();
    ClientIdCollection::const_iterator end = clientids.end();
    while( iter != end ){
        sql.append( format("AND %s.CLIENTID != '%s'", fileOwnerTableName_, *iter) );
        ++iter;
    }

    poco_notice_f1(logger_, "in search_percentage_raw, next execute :\n%s", sql);
    select << sql, Limit(needCount, false, false);
    select.execute();
    RecordSet rs(select);

    bool more = rs.moveFirst();
    while(more){
        more = rs.moveNext();
        string clientid = rs[0].convert<string>();
        string ip = rs[1].convert<string>();
        int port = rs[2].convert<int>();
        int percentage = rs[3].convert<int>();
        ClientPtr pClient( new ClientInfo(clientid, ip, port) );
        FileOwnerInfoPtr pFile( new FileOwnerInfo(clientid, fileid, percentage) );
        c->push_back( make_pair(pClient, pFile) );
    }

    return ERROR_OK;
}

retcode_t TrackerDBManager::insert_file_owner_info(const FileOwnerInfo& info){
    FastMutex::ScopedLock lock(mutex_);
    using namespace Poco::Data;
    string sql;
    format(sql, "INSERT INTO %s(FILEID, CLIENTID, PERCENTAGE) VALUES(?,?,?)", fileOwnerTableName_);
    session_ << sql, use(info.fileid()), use(info.clientid()), use(info.percentage()), now;
    return ERROR_OK;
}

retcode_t TrackerDBManager::update_file_owner_info(const FileOwnerInfo& info){
    FastMutex::ScopedLock lock(mutex_);
    using namespace Poco::Data;
    string sql;
    format(sql, "INSERT INTO %s (FILEID, CLIENTID, PERCENTAGE) VALUES( '%s', '%s', %d) ON DUPLICATE KEY UPDATE PERCENTAGE=VALUES(PERCENTAGE)", fileOwnerTableName_, info.fileid(), info.clientid(), info.percentage());
    session_ << sql, now;
    return ERROR_OK;
}

retcode_t TrackerDBManager::remove_file_owner_info(const FileOwnerInfo& info){
    FastMutex::ScopedLock lock(mutex_);
    using namespace Poco::Data;
    string sql;
    format(sql, "DELETE FROM %s WHERE FILEID=? AND CLIENTID=?", fileOwnerTableName_);
    session_ << sql, use(info.fileid()), use(info.clientid()), now;
    return ERROR_OK;
}

retcode_t TrackerDBManager::init_tables(){
    retcode_t ret = this->init_client_info_table();
    if( ret != ERROR_OK ){
        poco_error_f1(logger_, "in TrackerDBManager, Init client info table error, retcode : %d.", ret);
        return ret;
    }

    ret = this->init_file_owner_info_table();
    if( ret != ERROR_OK ){
        poco_error_f1(logger_, "in TrackerDBManager, Init file owner info table error, retcode : %d.", ret);
        return ret;
    }
    return ERROR_OK;
}

retcode_t TrackerDBManager::init_client_info_table(){
    using namespace Poco::Data;
    string clientInfoTableName("CLIENTINFO");
    try{
        session_ << "CREATE TABLE IF NOT EXISTS " << clientInfoTableName << "("
                   "CLIENTID VARCHAR(40) PRIMARY KEY,"
                   "IP VARCHAR(20),"
                   "MESSAGEPORT INTEGER,"
                   "UPLOADTOTAL BIGINT,"
                   "DOWNLOADTOTAL BIGINT,"
                   "CREATETIME VARCHAR(40),"
                   "LASTLOGINTIME VARCHAR(40),"
                   "ISONLINE INTEGER"
                   ");", now;
    }catch(Exception& e){
        poco_error_f1(logger_, "init client info table error, msg : %s", e.message());
        return ERROR_DB_INIT_TABLE_ERROR;
    }
    return ERROR_OK;
}

retcode_t TrackerDBManager::init_file_owner_info_table(){
    using namespace Poco::Data;
    string fileOwnerTableName("FILEOWNERINFO");
    try{
        session_ << "CREATE TABLE IF NOT EXISTS " << fileOwnerTableName << "("
                   //"id INTEGER PRIMARY KEY AUTO_INCREMENT,"
                   "FILEID VARCHAR(40),"
                   "CLIENTID VARCHAR(40),"
                   "PERCENTAGE INTEGER,"
                   "PRIMARY KEY(FILEID, CLIENTID)"
                   ");", now;
    }catch(Exception& e){
        poco_error_f1(logger_, "init file owner info table error, msg : %s", e.message());
        return ERROR_DB_INIT_TABLE_ERROR;
    }
    return ERROR_OK;
}
