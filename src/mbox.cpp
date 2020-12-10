#include <iostream>
#include <math.h>

#include <epicsTime.h>
#include <epicsThread.h>
#include <epicsEvent.h>

#include <pvxs/sharedpv.h>
#include <pvxs/server.h>
#include <pvxs/nt.h>
#include <pvxs/log.h>

#include "epics_time_stamp.h"

using namespace pvxs;
using namespace std;

// Custom data type
TypeDef mboxdef(TypeCode::Struct, "mbox",
{
    Member(TypeCode::Struct, "timeStamp", "time_t",
    {
        Member(TypeCode::UInt64, "secondsPastEpoch"),
        Member(TypeCode::UInt32, "nanoseconds"),
    }),
    Member(TypeCode::Struct, "bpms",
    {
        Member(TypeCode::Float64A, "x"),
        Member(TypeCode::Float64A, "y"),
    }),
    Member(TypeCode::Struct, "steerers",
    {
        Member(TypeCode::Float64A, "x"),
        Member(TypeCode::Float64A, "y"),
    }),
    Member(TypeCode::Struct, "bookKeeping",
    {
        Member(TypeCode::UInt64, "count"),
        Member(TypeCode::Struct, "last", {
                Member(TypeCode::Struct, "timeStamp", "time_t", {
                        Member(TypeCode::UInt64, "secondsPastEpoch"),
                        Member(TypeCode::UInt32, "nanoseconds"),
                    }),
                Member(TypeCode::UInt64, "atCount"),
        }),
        Member(TypeCode::Struct, "lastSet", {
                Member(TypeCode::Struct, "timeStamp", "time_t", {
                        Member(TypeCode::UInt64, "secondsPastEpoch"),
                        Member(TypeCode::UInt32, "nanoseconds"),
                    }),
                Member(TypeCode::UInt64, "atCount"),
        }),
        Member(TypeCode::Struct, "nextAfter", "time_t", {
                Member(TypeCode::UInt64, "secondsPastEpoch"),
                Member(TypeCode::UInt32, "nanoseconds"),
        }),
    }),
});



int main(int argc, char* argv[])
{

    // For full detail,
    // export PVXS_LOG="*=DEBUG"
    logger_config_env();

    std::string pv_name = "Pierre:mbox";

    epicsTimeStamp now;
    Value initial = mboxdef.create();

    epicsTimeGetCurrent(&now);
    initial["bookKeeping.last.timeStamp.secondsPastEpoch"] = now.secPastEpoch;
    initial["bookKeeping.last.timeStamp.nanoseconds"] = now.nsec;
    initial["bookKeeping.last.atCount"] = 0;
    initial["bookKeeping.lastSet.timeStamp.secondsPastEpoch"] = 0;
    initial["bookKeeping.lastSet.timeStamp.nanoseconds"] = 0;
    initial["bookKeeping.lastSet.atCount"] = 0;
    initial["bookKeeping.nextAfter.secondsPastEpoch"] = 0;
    initial["bookKeeping.nextAfter.nanoseconds"] = 0;
    initial["bookKeeping.count"] = 0;

    server::SharedPV pv(server::SharedPV::buildMailbox());
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

    unsigned long cnt = 0;
    bool printed=false;

    for(cnt=0; ! done.wait(delay); ++cnt)
    {
        epicsTimeGetCurrent(&now);
        auto actual = initial.cloneEmpty();

        size_t count = 128;
        shared_array<double> bpms_x(count), bpms_y(count),
            steerers_x(2), steerers_y(2);

        {
            for (size_t i=0; i<count; ++i)
            {
                double frac = i/double(count);

                bpms_x[i] = cos((frac + cnt) * 355/113*3);
                bpms_y[i] = sin((frac + cnt) * 355/113*3);
                //steerers_x[i] = 0.0;
                //steerers_y[i] = 0.0;
            }
        }

        actual["bpms.x"] = bpms_x.freeze().castTo<const void>();
        actual["bpms.y"] = bpms_y.freeze().castTo<const void>();
        actual["bookKeeping.count"] = cnt;
        actual["bookKeeping.last.timeStamp.secondsPastEpoch"] = now.secPastEpoch;
        actual["bookKeeping.last.timeStamp.nanoseconds"] = now.nsec;
        actual["bookKeeping.last.atCount"] = cnt;

        pv.post(std::move(actual));
        pv.fetch(actual);

        epicsTimeStamp run_after;
        run_after.secPastEpoch  = actual["bookKeeping.nextAfter.secondsPastEpoch"].as<uint64_t>();
        run_after.nsec  = actual["bookKeeping.nextAfter.nanoseconds"].as<uint32_t>();

        epicsTimeGetCurrent(&now);
        if(already_passed(now, run_after)){
            actual["bookKeeping.lastSet.timeStamp.secondsPastEpoch"] = now.secPastEpoch;
            actual["bookKeeping.lastSet.timeStamp.nanoseconds"] = now.nsec;
            actual["bookKeeping.lastSet.atCount"] = cnt;
            actual["bookKeeping.nextAfter.secondsPastEpoch"] = 0;
            actual["bookKeeping.nextAfter.nanoseconds"] = 0;

            epicsTimeStamp tdiff = diff(now, run_after);
            std::cout << "Set data after "
                      << tdiff.secPastEpoch << " sec "
                      << tdiff.nsec/1e6 << " msec " << std::endl
                      << "Now at "
                      << now.secPastEpoch << " sec "
                      << now.nsec << " nsec"
                      << endl;
            pv.post(std::move(actual));
        }
        if(!printed){
            cout << "fetch returned " << actual << endl;
            printed = true;
        }
        //cerr << ".";
    }

    server.stop();

    std::cout<<"Bye!\n";

    return 0;
}
