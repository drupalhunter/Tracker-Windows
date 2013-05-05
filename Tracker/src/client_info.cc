#include "client_info.h"

ClientInfo::ClientInfo(const string& clientId, const string& ip, int messagePort = 9025)
    :clientId_(clientId),
    ip_(ip),
    messagePort_(messagePort)
{
}

ClientInfo::~ClientInfo(){
}
