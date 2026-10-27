#include "UtcTime.h"
#include <cmath>
#include <cstdio>

namespace RaceSyncLaps {
namespace {
bool leap(int y) { return y%4==0 && (y%100!=0 || y%400==0); }
}
uint64_t utcMilliseconds(int y,int m,int d,int h,int minute,int s,double fraction) {
    if (y<2024 || y>2099 || m<1 || m>12 || h<0 || h>23 || minute<0 || minute>59 ||
        s<0 || s>59 || !std::isfinite(fraction) || fraction<=-1 || fraction>=1) return 0;
    const int lengths[]={31,28,31,30,31,30,31,31,30,31,30,31};
    if(d<1 || d>lengths[m-1]+(m==2 && leap(y))) return 0;
    int days=0;
    for(int year=1970;year<y;++year) days+=leap(year)?366:365;
    for(int month=1;month<m;++month) days+=lengths[month-1]+(month==2 && leap(y));
    days+=d-1;
    return static_cast<uint64_t>(static_cast<int64_t>(days)*86400000 +
        ((h*60LL+minute)*60+s)*1000 + std::llround(fraction*1000));
}
bool formatUtcTimestamp(uint64_t ms, char* text, size_t capacity) {
    uint64_t days=ms/86400000;uint32_t tod=ms%86400000;
    int year=1970,month=1;
    while(days>=static_cast<uint64_t>(leap(year)?366:365)) {days-=leap(year)?366:365;++year;}
    const int lengths[]={31,28,31,30,31,30,31,31,30,31,30,31};
    while(days>=static_cast<uint64_t>(lengths[month-1]+(month==2&&leap(year)))) {
        days-=lengths[month-1]+(month==2&&leap(year));++month;
    }
    const int written=snprintf(text,capacity,"%04d-%02d-%02uT%02u:%02u:%02u.%03uZ",year,month,
        static_cast<unsigned>(days+1),tod/3600000,(tod/60000)%60,(tod/1000)%60,tod%1000);
    return written>=0 && static_cast<size_t>(written)<capacity;
}
}
