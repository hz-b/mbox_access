#include <iostream>
#include <epicsEvent.h>
#include <epicsThread.h>
#include <pvxs/client.h>
#include <pvxs/log.h>


using namespace std;
using namespace pvxs;


int main()
{
    // For full detail,
    // export PVXS_LOG="*=DEBUG"
    logger_config_env();

    std::string request = "field(pixel)";

    cout << "Hi!" << endl;

    client::Context ctxt = client::Config::from_env().build();
    epicsEvent done;
    auto op = ctxt.get("neutrons")
                  .pvRequest(request)
                  .result([&done](client::Result&& result)
                          {
                            cout << result() << endl;
                            done.signal();
                          })
                  .exec();

    ctxt.hurryUp();

    //  epicsThreadSleep(5.0);
    done.wait(5.0);
    cout << "Bye!" << endl;
    
    return 0;
}