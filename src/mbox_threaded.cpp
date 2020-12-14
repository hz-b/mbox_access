/**
 * Experimenting with pvxs for the servers
 *
 * This code tries to see if OnPut can be used together with polling
 * the server at the same time
 *
 * It seems that the server can be used simulatniously in two different
 * threads
 */

#include <iostream>
#include <thread>
#include <math.h>
#include <signal.h>

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

/*
 *  I split the variables set by the user and the variables read
 *  by the user.
 *
 * Intended use case: set data / read data from a fast orbit
 * feedback
 */
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

TypeDef mboxsetdef(TypeCode::Struct, "settings", {
    Member(TypeCode::Struct, "steerers",
    {
        Member(TypeCode::Float64A, "x"),
        Member(TypeCode::Float64A, "y"),
    }),
    Member(TypeCode::Struct, "nextAfter", "time_t", {
            Member(TypeCode::UInt64, "secondsPastEpoch"),
            Member(TypeCode::UInt32, "nanoseconds"),
   }),
});

volatile sig_atomic_t new_data = false;

void run_server(server::Server *server)
{
    server->run();
}

int main(int argc, char* argv[])
{
    double delay = .5;

    // For full detail,
    // export PVXS_LOG="*=DEBUG"
    logger_config_env();

    std::string pv_name = "Pierre:mbox";

    epicsTimeStamp now;
    Value rbk = mboxdef.create();
    Value set = mboxsetdef.create();

    epicsTimeGetCurrent(&now);
    /* Is there a way to find out which attribute is the offending one? */
    cerr << "Setting book keeping" << endl;
    rbk["bookKeeping.last.timeStamp.secondsPastEpoch"] = now.secPastEpoch;
    rbk["bookKeeping.last.timeStamp.nanoseconds"] = now.nsec;
    rbk["bookKeeping.last.atCount"] = 0;
    rbk["bookKeeping.lastSet.timeStamp.secondsPastEpoch"] = 0;
    rbk["bookKeeping.lastSet.timeStamp.nanoseconds"] = 0;
    rbk["bookKeeping.lastSet.atCount"] = 0;
    rbk["bookKeeping.nextAfter.secondsPastEpoch"] = 0;
    rbk["bookKeeping.nextAfter.nanoseconds"] = 0;
    rbk["bookKeeping.count"] = 0;

    cerr << "Setting set" << set <<  endl;
    set["nextAfter.secondsPastEpoch"] = 0;
    set["nextAfter.nanoseconds"] = 0;
    cerr << "Initial  set done" << endl;

    server::SharedPV pv_rbk(server::SharedPV::buildReadonly());
    server::SharedPV pv_set(server::SharedPV::buildMailbox());
    pv_rbk.open(rbk);
    pv_set.open(set);

    std::string pv_name_rbk = pv_name + ":rbk";
    std::string pv_name_set = pv_name + ":set";

    server::Server server = server::Config::from_env()
                                   .build()
                                   .addPV(pv_name_rbk, pv_rbk)
                                   .addPV(pv_name_set, pv_set);

    std::cout << "Effective config\n"<<server.config();
    std::cout << "Check 'pvxget " << pv_name_rbk
              << " or " << pv_name_set << std::endl;
    std::cout << "Ctrl-C to end...\n";

    epicsEvent done;
    /* Used by server->run */
    // SigInt handle( [&done] ()  { done.trigger(); });

    unsigned long cnt = 0;
    bool printed=false;

    // see if callbacks work ...
    pv_set.onPut([](server::SharedPV& pv_seti,
                std::unique_ptr<server::ExecOp>&& op,
                Value&& top){

                     // (optional) Provide a timestamp if the client has not (common)
                     Value st_x = top["steerers.x"];
                     Value st_y = top["steerers.y"];
                     Value ts = top["nextAfter"];

                     new_data = true;
                     // A bit visual info on what is new
                     if(top.isMarked(true, true)) {
                         std::cerr << "setp marked" << std::endl;
                     }

                     if(st_x.isMarked(false, true)) {
                         std::cerr << "st_x marked" << std::endl;
                     }
                     if(st_y.isMarked(false, true)) {
                         std::cerr << "st_y marked" << std::endl;
                     }

                     if(ts.isMarked(false, true)) {
                         std::cerr << "ts marked" << std::endl;
                     }

                     // (optional) update the SharedPV cache and send
                     // a update to any subscribers
                     pv_seti.post(top);

                     // Required.  Inform client that PUT operation is complete.
                     op->reply();
                 });

    // for put completion
    std::thread background(run_server, &server);
    auto actual = rbk.cloneEmpty();
    auto act_set = set.cloneEmpty();

    for(cnt=0; ! done.wait(delay); ++cnt)
    {
        epicsTimeGetCurrent(&now);

        size_t count = 128;
        shared_array<double> bpms_x(count), bpms_y(count),
            steerers_x(2), steerers_y(2);

        for (size_t i=0; i<count; ++i)
        {
            double frac = i/double(count);

            bpms_x[i] = cos((frac + cnt) * 355/113*3);
            bpms_y[i] = sin((frac + cnt) * 355/113*3);
            //steerers_x[i] = 0.0;
            //steerers_y[i] = 0.0;
        }

        actual["bpms.x"] = bpms_x.freeze().castTo<const void>();
        actual["bpms.y"] = bpms_y.freeze().castTo<const void>();
        actual["bookKeeping.count"] = cnt;
        actual["bookKeeping.last.timeStamp.secondsPastEpoch"] = now.secPastEpoch;
        actual["bookKeeping.last.timeStamp.nanoseconds"] = now.nsec;
        actual["bookKeeping.last.atCount"] = cnt;

        pv_rbk.post(std::move(actual));
        //pv.fetch(actual);
        bool fetch_and_mark = ((cnt == 0) || new_data);
        if(fetch_and_mark){
            pv_set.fetch(act_set);
        }

        epicsTimeStamp run_after;
        bool flag = act_set.isMarked(false, true);

        cerr << "New data arrived? Marked; In act_set " << flag
             << " by sig_atomic_t: "  << new_data
             << " by fetch and mark "  << fetch_and_mark  << endl;

        if(fetch_and_mark){
            act_set.unmark(true, true);
            flag = act_set.isMarked(false, true);
            cerr << "Unmarked. New data in next: " << flag << "?"
                 << "new data"  << new_data << "?"
                 <<  " fetch and mark "  << fetch_and_mark  << endl;
            new_data = false;
        }

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
            std::cerr << "posting rbk 2 " << std::endl;
            pv_rbk.post(std::move(actual));

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
