#include <iostream>
#include <epicsEvent.h>
#include <epicsThread.h>
#include <pvxs/client.h>
#include <pvxs/log.h>


using namespace std;
using namespace pvxs;


int main(int argc, char* argv[])
{
    // For full detail,
    // export PVXS_LOG="*=DEBUG"
    logger_config_env();

    std::string field;
    std::string request;
    
    if (argc == 2)
    {
        field = argv[1];
        request = "field(" + field + ")";
    }

    cout << "Hi!" << endl;

    client::Context ctxt = client::Config::from_env().build();
    epicsEvent done;
    auto op = ctxt.get("neutrons")
                  .pvRequest(request)
                  .result([&field, &done](client::Result&& result)
                          {
                            if (field.empty())
                                cout << result() << endl;
                            else
                                cout << result()[field] << endl;
                            done.signal();
                          })
                  .exec();

    ctxt.hurryUp();

    //  epicsThreadSleep(5.0);
    done.wait(5.0);
    cout << "Bye!" << endl;
    
    return 0;
}