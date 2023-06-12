#ifndef __GVC_WEEKLY_CALENDAR
#define __GVC_WEEKLY_CALENDAR

#include <ctime>
#include "schedule.pb.h"

#define STRUCT_VERSION 0x01
#define EVERYDAY 7
#define MAX_ENTRIES 10
#define SECS_PER_WEEK 604800
#define SECS_PER_DAY 86400
#define SECS_PER_MIN 60
#define SECS_PER_HOU 3600
#define THU_TO_SUN 3


#define MAX_EVENTS 10


typedef struct event {
    time_t time;
    uint8_t action;
} event_t;


class WeeklyCalendar {

    public:
        WeeklyCalendar();
        void parse_schedule();

        uint8_t init(
                time_t ctime, 
                uint8_t * buffer, 
                size_t len,
                uint32_t evt_precision
                );
        
        uint8_t next_event(
                time_t now,
                uint8_t *op,
                uint32_t * sleeptime
                );
        time_t get_next_event_time();

        uint8_t add_event(ScheduleEntry * ent);
        int get_timezone_offset(time_t time);

        void print_time_tm(const char * text, struct tm * prt_time);
        void print_time_t(const char * text, time_t t, uint8_t utc);
        void time_t_to_str(char * text, time_t t, uint8_t utc);

        time_t _ctime;

    private:

        uint8_t _ev_next, _ev_nr;
        event_t _events[MAX_EVENTS];
        uint8_t _next_exec;
        uint32_t _evt_precision;

        time_t _last_occurrence(time_t offset, time_t time, time_t period);
        time_t _get_period(uint8_t wday);
        time_t _get_offset(uint8_t wday, time_t time);

        void _write_log(const char *format, ...);

};



#endif
