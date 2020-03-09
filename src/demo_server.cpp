#include <iostream>

#include <epicsTime.h>
#include <epicsThread.h>
#include <epicsEvent.h>

#include <pvxs/sharedpv.h>
#include <pvxs/server.h>
#include <pvxs/nt.h>
#include <pvxs/log.h>

using namespace pvxs;
using namespace std;

int main(int argc, char* argv[])
{
    // To show detail,  `export PVXS_LOG=*=DEBUG`
    logger_config_env();

    std::string pv_name = "test";

    // Use pre-canned definition of scalar with meta-data
    Value initial = nt::NTScalar{TypeCode::Float64}.create();
    initial["value"] = 42.0;
    initial["alarm.severity"] = 0;
    initial["alarm.status"] = 0;
    initial["alarm.message"] = "";

    server::SharedPV pv(server::SharedPV::buildReadonly());
    pv.open(initial);

    server::Server server = server::Config::from_env()
                            .build()
                            .addPV(pv_name, pv);

    std::cout << "Effective config\n"<<server.config();
    std::cout << "Check 'pvxget " << pv_name << "'\n";
    std::cout << "Ctrl-C to end...\n";

    server.start();

    epicsEvent done;
    SigInt handle( [&done] ()  { done.trigger(); });

    int count = 42;
    while (! done.wait(1.0))
    {
      auto val = initial.cloneEmpty();
      val["value"] = ++count;
      pv.post(std::move(val));
    }

    server.stop();

    std::cout<<"Bye!\n";

    return 0;
}
