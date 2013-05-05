#ifndef TRACKER_DB_MANAGER
#define TRACKER_DB_MANAGER

#include "error_code.h"
#include "client_info.h"
#include "file_owner_info.h"

#include <vector>
#include <string>
#include <Poco/Types.h>
#include <Poco/SharedPtr.h>
#include <Poco/Data/Session.h>
#include <Poco/Data/MySQL/Connector.h>
#include <Poco/Logger.h>
#include <Poco/Mutex.h>

using std::vector;
using std::string;
using Poco::Int64;
using Poco::SharedPtr;
using Poco::Data::Session;
using Poco::Logger;
using Poco::FastMutex;

class TrackerDBManager{
    public:
        typedef SharedPtr<ClientInfo> ClientPtr;
        typedef SharedPtr<FileOwnerInfo> FileOwnerInfoPtr;
        typedef vector< std::pair<ClientPtr, FileOwnerInfoPtr> > FileOwnerInfoCollection;
        typedef vector<string> ClientIdCollection;

        enum SearchType{
            GREATER = 1,
            LESS_EQUAL = 2
        };

        TrackerDBManager();
        ~TrackerDBManager();

        retcode_t insert_or_update_login_info(const string& clientid, const string& ip, int port, 
                const string loginTime);
        retcode_t update_logout_info(const string& clientid, Int64 downloadTotal, Int64 uploadTotal);
        /*
        retcode_t search_client_info(const string& cientid, ClientInfo* pInfo);
        retcode_t search_file_owner_info(const string& fileid, FileOwnerInfoPtrCollection* c);
        */
        //search func do not remove the items in FileOwnerInfoPtrCollection!
        //
        retcode_t search_near_percentage_client(const string& fileid, int percentage, int needCount,
                const ClientIdCollection& clientids, FileOwnerInfoCollection* c);

        retcode_t search_greater_percentage(const string& fileid, int percentage, int needCount,
                const ClientIdCollection& clientids, FileOwnerInfoCollection* c);

        retcode_t search_less_equal_percentage(const string& fileid, int percentage, int needCount, 
                const ClientIdCollection& clientids, FileOwnerInfoCollection* c);

        retcode_t insert_file_owner_info(const FileOwnerInfo& info);
        retcode_t update_file_owner_info(const FileOwnerInfo& info);
        retcode_t remove_file_owner_info(const FileOwnerInfo& info);

    private:
        retcode_t search_percentage_raw(SearchType type, const string& fileid, int percentage, int needCount,
                const ClientIdCollection& clientids, FileOwnerInfoCollection* c);
        retcode_t init_tables();

        retcode_t init_client_info_table();
        retcode_t init_file_owner_info_table();

        Logger& logger_;
        Session session_;

        string clientInfoTableName_;
        string fileOwnerTableName_;

        FastMutex mutex_;
};

#endif
