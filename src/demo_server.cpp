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

// Custom data type
TypeDef neutrondef(TypeCode::Struct, "structure",
{
    Member(TypeCode::Struct, "timeStamp", "time_t",
    {
        Member(TypeCode::UInt64, "secondsPastEpoch"),
        Member(TypeCode::UInt32, "nanoseconds"),
        Member(TypeCode::UInt32, "userTag"),
    }),
    Member(TypeCode::Struct, "proton_charge", "epics:nt/NTScalar:1.0",
    {
        Member(TypeCode::Float64, "value"),
    }),
    Member(TypeCode::Struct, "time_of_flight", "epics:nt/NTScalarArray:1.0",
    {
        Member(TypeCode::UInt32A, "value"),
    }),
    Member(TypeCode::Struct, "pixel", "epics:nt/NTScalarArray:1.0",
    {
        Member(TypeCode::UInt32A, "value"),
    }),
});


static void help(const char *name)
{
    cout << "USAGE: " << name << " [options]" << endl;
    cout << "  -h        : Help" << endl;
    cout << "  -d seconds: Delay between packages (default 0.01)" << endl;
    cout << "  -e count  : Max event count per packet (default 10)" << endl;
}

int main(int argc, char* argv[])
{
    double delay = 0.01;
    size_t event_count = 10;

    int opt;
    while ((opt = getopt(argc, argv, "d:e:h")) != -1)
    {
        switch (opt)
        {
            case 'd':
                delay = atof(optarg);
                break;
            case 'e':
                event_count = (size_t)atol(optarg);
                break;
            case 'h':
                help(argv[0]);
                return 0;
            default:
                help(argv[0]);
                return -1;
        }
    }

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
    unsigned long id = 0;
    epicsTimeStamp now;
    while (! done.wait(delay))
    {
      ++id;
      epicsTimeGetCurrent(&now);
      auto val = initial.cloneEmpty();
      // val["value"] = id;
      // Vary a fake 'charge' based on the ID
      val["proton_charge.value"] = (1 + id % 10)*1e8;
      val["timeStamp.secondsPastEpoch"] = now.secPastEpoch;
      val["timeStamp.nanoseconds"] = now.nsec;

      size_t count = event_count;
      shared_array<unsigned int> tof(count);
      for (int i=0; i<count; ++i)
          tof[i] = id;
      val["time_of_flight.value"] = tof.freeze().castTo<const void>();

      shared_array<unsigned int> pxl(count);
      unsigned int value = id*10;
      for (size_t i=0; i<count; ++i)
          pxl[i] = value;
      val["pixel.value"] = pxl.freeze().castTo<const void>();

      pv.post(std::move(val));
    }

    server.stop();

    std::cout<<"Bye!\n";

    return 0;
}
