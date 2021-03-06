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


using namespace pvxs;
using namespace std;

/*
 *  I split the variables set by the user and the variables read
 *  by the user.
 *
 * Intended use case: set data / read data from a fast orbit
 * feedback
 */
TypeDef mboxdef(TypeCode::Struct, "rbk",
{
    Member(TypeCode::Struct, "timeStamp", "time_t",
    {
        Member(TypeCode::UInt64, "secondsPastEpoch"),
        Member(TypeCode::UInt32, "nanoseconds"),
    }),
});

TypeDef mboxsetdef(TypeCode::Struct, "set", {
    Member(TypeCode::Struct, "steerers",
    {
        Member(TypeCode::Float64, "vala"),
        Member(TypeCode::Float64, "valb"),
    }),
});

static void usage(const char* progname)
{
    std::cerr << "Usage " << progname << " pv_prefix" << std::endl;
    std::exit(1);
}

int main(int argc, char* argv[])
{

    // For full detail,
    // export PVXS_LOG="*=DEBUG"
    logger_config_env();

    if(argc != 2){
        usage(argv[0]);
    }

    std::string pv_name_prefix = argv[1];
    double delay = .5;

    Value rbk = mboxdef.create();
    Value set = mboxsetdef.create();

    epicsTimeStamp now;

    epicsTimeGetCurrent(&now);

    /* Is there a way to find out which attribute is the offending one? */
    cerr << "Setting rbk" << endl;
    rbk["timeStamp.secondsPastEpoch"] = now.secPastEpoch;
    rbk["timeStamp.nanoseconds"] = now.nsec;
    cerr << "Setting set" << set <<  endl;
    set["steerers.vala"] = -1;
    set["steerers.valb"] = -2;
    cerr << "Initial  set done" << endl;

    server::SharedPV pv_rbk(server::SharedPV::buildReadonly());
    server::SharedPV pv_set(server::SharedPV::buildMailbox());
    pv_rbk.open(rbk);
    pv_set.open(set);

    std::string pv_name_rbk = pv_name_prefix + ":rbk";
    std::string pv_name_set = pv_name_prefix + ":set";

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

    auto act_rbk = rbk.cloneEmpty();
    auto act_set = set.cloneEmpty();

    for(cnt=0; ! done.wait(delay); ++cnt)
    {
        epicsTimeGetCurrent(&now);
        // just to change the values
        act_rbk["timeStamp.secondsPastEpoch"] = now.secPastEpoch;
        act_rbk["timeStamp.nanoseconds"] = now.nsec;

        // and post it
        pv_rbk.post(std::move(act_rbk));

        pv_set.fetch(act_set);
        act_set["steerers.vala"] = 0;
        act_set["steerers.valb"] = 0;


        bool flag = act_set.isMarked(false, true);
        bool flag_a = act_set["steerers.vala"].isMarked(false, true);
        bool flag_b = act_set["steerers.valb"].isMarked(false, true);

        cerr << "New data arrived?   Marked: act_set " << flag
             << " value a: "  << flag_a
             << " value b: "  << flag_b  << endl;

        if (flag || flag_a || flag_b){
            act_set.unmark(true, true);
            bool flag = act_set.isMarked(false, true);
            bool flag_a = act_set["steerers.vala"].isMarked(false, true);
            bool flag_b = act_set["steerers.valb"].isMarked(false, true);

            cerr << "Did unmark work?:       act_set " << flag
                 << " value a: "  << flag_a
                 << " value b: "  << flag_b  << endl << endl;
        }
        //cerr << ".";
    }

    server.stop();

    std::cout<<"Bye!\n";

    return 0;
}
