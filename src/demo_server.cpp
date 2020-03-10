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

#define NS_TOF_MAX 160000 /** Maximum TOF value for the -r option (realistic data)*/
#define NS_TOF_NORM 10 /** Number of random samples for each TOF to generate a normal distribution*/

#define NS_ID_MIN1 0    /** Min pixel ID for detector 1 */
#define NS_ID_MAX1 1023 /** Max pixel ID for detector 1 */
#define NS_ID_MIN2 2048 /** Min pixel ID for detector 2 */
#define NS_ID_MAX2 3072 /** Max pixel ID for detector 2 */

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
    cout << "  -m : Random event count, using 'count' as maximum" << endl;
    cout << "  -r : Generate normally distributed data which looks semi realistic." << endl;
}

int main(int argc, char* argv[])
{
    double delay = 0.01;
    size_t event_count = 10;
    bool random_count = false;
    bool realistic = false;
    
    int opt;
    while ((opt = getopt(argc, argv, "mrhd:e:")) != -1)
    {
        switch (opt)
        {
        case 'd':
            delay = atof(optarg);
            break;
        case 'e':
            event_count = (size_t)atol(optarg);
            break;
        case 'm':
                random_count = true;
            break;
        case 'r':
                realistic = true;
            break;
        case 'h':
            help(argv[0]);
            return 0;
        default:
            help(argv[0]);
            return -1;
        }
    }

    // For full detail,
    // export PVXS_LOG="*=DEBUG"
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

        size_t count = random_count ? (rand() % event_count) : event_count;
        shared_array<unsigned int> tof(count);
        shared_array<unsigned int> pxl(count);

        if (realistic)
        {
            for (size_t i=0; i<count; ++i)
            {
                unsigned int normal_tof = 0;
                for (size_t j = 0; j < NS_TOF_NORM; ++j)
                    normal_tof += rand() % (NS_TOF_MAX);
                tof[i] = int(normal_tof/NS_TOF_NORM);

                if (i%2 == 0)
                    pxl[i] = (rand() % (NS_ID_MAX1-NS_ID_MIN1)) + NS_ID_MIN1;
                else
                    pxl[i] = (rand() % (NS_ID_MAX2-NS_ID_MIN2)) + NS_ID_MIN2;
            }
        }
        else
        {
            unsigned int value = id*10;
            for (size_t i=0; i<count; ++i)
            {
                tof[i] = id;
                pxl[i] = value;
            }
        }            

        val["time_of_flight.value"] = tof.freeze().castTo<const void>();
        val["pixel.value"] = pxl.freeze().castTo<const void>();

        pv.post(std::move(val));
    }

    server.stop();

    std::cout<<"Bye!\n";

    return 0;
}
