#include <iostream>
#include <epicsEvent.h>
#include <epicsThread.h>
#include <pvxs/client.h>
#include <pvxs/log.h>


using namespace std;
using namespace pvxs;

epicsEvent done;

int main()
{
    logger_config_env();

    std::string request;

    cout << "Hi!" << endl;

    client::Context ctxt = client::Config::from_env().build();
    auto op = ctxt.get("demo")
                  //.pvRequest(request)
                  .result([](client::Result&& result)
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