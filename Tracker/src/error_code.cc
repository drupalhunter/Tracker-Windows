#include "error_code.h"
/*
const char* errmsg(retcode_t ret){
    const char* p = "Everything is OK.";
    if( ret == ERROR_OK ){
        return p;
    }else{
        switch(ret){
            case ERROR_UNKNOWN:
                p = "Unknown Error!";
                break;
            case ERROR_PACK_SEND_HEADER_LENGTH:
                p = "Error while send netpack header.";
                break;
            default:
                p = "Unknown retcode_t value.";
                break;
        }
    }
    return p;
}
*/
