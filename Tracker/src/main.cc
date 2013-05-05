#include <tracker.h>

#include <iostream>
#include <Poco/Exception.h>

using std::cerr;
using std::endl;
using Poco::Exception;

int main(int argc, char* argv[]){
    try{
        Tracker t;
        return t.run(argc, argv);
    }catch(Exception& e){
        cerr << e.message() << endl;
    }
}
