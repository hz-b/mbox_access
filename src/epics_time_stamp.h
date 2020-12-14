#ifndef _MBOX_EPICS_TIME_STAMP_H_
#define _MBOX_EPICS_TIME_STAMP_H_
#include <cassert>
#include <epicsTime.h>

static inline bool already_passed(epicsTimeStamp &now, epicsTimeStamp &ref, bool ignore_empty=true)
{

    if(ignore_empty){
        if(ref.secPastEpoch == 0){
            /* in that case ignore the nano seconds ... 1970 is long gone */
            // std::cerr << "ref stamp not set" << endl;
            return false;
        }
    }
    if(now.secPastEpoch > ref.secPastEpoch){
        return true;
    }
    else if(now.secPastEpoch == ref.secPastEpoch && now.nsec > ref.nsec){
        return true;
    }
    return false;

}

static inline epicsTimeStamp diff(epicsTimeStamp now, epicsTimeStamp ref, bool ignore_empty=true)
{
    epicsTimeStamp result;
    int64_t sec,  nsec, nsec_per_second = 1000 * 1000 * 1000;

    sec =  now.secPastEpoch - ref.secPastEpoch;
    assert(sec >= 0);
    if(sec < 0){
        throw;
    }
    nsec = now.nsec - ref.nsec;
    if (nsec > 0){
        while(nsec >= nsec_per_second){
            sec++;
            nsec -= nsec_per_second;
        }
    }else if(nsec < 0){
        while(nsec < 0){
            nsec += nsec_per_second;
            sec--;
        }
    }
    assert(sec >= 0);
    assert(nsec >= 0);
    result.secPastEpoch =  sec;
    result.nsec = nsec;
    return result;

}
#endif /* _MBOX_EPICS_TIME_STAMP_H_ */
