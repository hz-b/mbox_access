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

TypeDef neutrondef(TypeCode::Struct, "simple_t", {
                Member(TypeCode::Float64, "value"),
                Member(TypeCode::Struct, "timeStamp", "time_t", {
                    Member(TypeCode::UInt64, "secondsPastEpoch"),
                    Member(TypeCode::UInt32, "nanoseconds"),
                }),
            });

int main(int argc, char* argv[])
{
    // To show detail,  `export PVXS_LOG=*=DEBUG`
    logger_config_env();

    std::string pv_name = "neutrons";

    Value initial = neutrondef.create();
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
    epicsTimeStamp now;
    while (! done.wait(1))
    {
      epicsTimeGetCurrent(&now);
      auto val = initial.cloneEmpty();
      val["value"] = ++count;
      val["timeStamp.secondsPastEpoch"] = now.secPastEpoch;
      val["timeStamp.nanoseconds"] = now.nsec;
      pv.post(std::move(val));
    }

    server.stop();

    std::cout<<"Bye!\n";

    return 0;
}
