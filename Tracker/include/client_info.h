#ifndef CLIENT_INFO_H
#define CLIENT_INFO_H

#include <string>

using std::string;

class ClientInfo{
    public:
        ClientInfo(const string& clientId, const string& ip, int messagePort); 
        ~ClientInfo();

        string clientid() const{
            return clientId_;
        }

        string ip() const{
            return ip_;
        }

        int messageport() const{
            return messagePort_;
        }

    private:
        string clientId_;
        string ip_;
        int messagePort_;
};

#endif
