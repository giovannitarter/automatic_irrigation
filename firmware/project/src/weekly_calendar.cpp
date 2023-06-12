#include <time.h>
#include <cstdio>
#include <stdlib.h>
#include <stdarg.h>
#include <sntp.h>
#include <Arduino.h>

#include "pb_decode.h"

#include "weekly_calendar.h"
#include "schedule.pb.h"

#include "garden_valve_controller.h"


bool dec_callback(pb_istream_t *istream, const pb_field_iter_t *field, void **arg) {

    WeeklyCalendar * wk = (WeeklyCalendar *) *arg;
    ScheduleEntry ent;

    if (field->tag == Schedule_events_tag) {

        if (!pb_decode(istream, ScheduleEntry_fields, &ent))
            return false;

        wk->add_event(&ent);
    }
    return true;
}


WeeklyCalendar::WeeklyCalendar() {
}


int WeeklyCalendar::get_timezone_offset(time_t time) {
    
    int offset, yoffset;
    struct tm gt, lt;

    gmtime_r(&time, &gt);
    localtime_r(&time, &lt);

    //go on...
    //much simpler with a working mktime
    //still broken when localtime is on one year while 
    //localtime is on the next or previous
    offset = (
            (lt.tm_hour - gt.tm_hour) * 60 +
            (lt.tm_min - gt.tm_min)
            );

    yoffset = lt.tm_yday - gt.tm_yday;
    if (yoffset < 0) {
        offset += 60 * 24;
    }
    else {
        offset += yoffset * 60 * 24;
    }

    offset = offset * 60;
    return offset;
}


//int WeeklyCalendar::get_timezone_offset(time_t time) {
//    
//    int offset;
//    struct tm tmp;
//    time_t ltime;
//
//    localtime_r(&time, &tmp);
//    
//    #define TZ "CET-1CEST,M3.5.0/2,M10.5.0/3"
//    
//    configTime(0, 0, "pool.ntp.org");
//    setenv("TZ", "UTC", 3);
//    tzset();
//    
//    ltime = mktime(&tmp);
//    
//    configTime(0, 0, "pool.ntp.org");
//    setenv("TZ", TZ, 3);
//    tzset();
//
//    offset = time - ltime;
//    
//    return offset;
//}


uint8_t WeeklyCalendar::add_event(ScheduleEntry * ent) {

    time_t time, period, offset, tmp;
    int tz_offset_now, tz_offset_next;

    time = ent->start_hou * SECS_PER_HOU + ent->start_min * SECS_PER_MIN;

    period = _get_period(ent->wday);
    offset = _get_offset(ent->wday, time);

    tz_offset_now = get_timezone_offset(_ctime);
    offset = offset - tz_offset_now;

    //last event
    tmp = _last_occurrence(offset, _ctime, period);

    //next event
    tmp += period;

    tz_offset_next = get_timezone_offset(tmp);

    //printf("tz_offset_now: %d\n", tz_offset_now);
    //printf("tz_offset_next: %d\n", tz_offset_next);

    tmp = tmp - (tz_offset_next - tz_offset_now);

    //print_time_tm("next_occurrence lt: ", &lt);
    //print_time_t("next_occurrence lt: ", tmp, 0);

    _events[_ev_next].time = tmp;
    _events[_ev_next].action = ent->op;
    _ev_next = (_ev_next + 1) % MAX_EVENTS;
    _ev_nr++;

    return 0;
};


int compar(const void * a, const void * b) {
    return (((event_t *)a)->time - ((event_t *)b)->time);
}


uint8_t WeeklyCalendar::init(
        time_t ctime,
        uint8_t * buffer,
        size_t len,
        uint32_t evt_precision
        )
{

    uint8_t res = 0;

    _ev_nr = 0;
    _evt_precision = evt_precision;
    _ctime = ctime - _evt_precision;
    _next_exec = 0;

    _write_log("read %d bytes\n\r", len);

    _ev_next = 0;
    for(int i=0; i<MAX_EVENTS; i++) {
        _events[i].time = UINT_MAX;
        _events[i].action = Operation_OP_NONE;
    }

    _write_log("ref time: %d\n\r", (unsigned int)ctime);

    Schedule msg;
    pb_istream_t istream;
    uint8_t status;

    msg.events.funcs.decode = &dec_callback;
    msg.events.arg = (void *) this;
    istream = pb_istream_from_buffer(buffer, len);
    status = pb_decode(&istream, Schedule_fields, &msg);

    qsort(_events, _ev_nr, sizeof(event_t), &compar);

    for(int i=0; i<_ev_nr; i++) {
        _write_log("%d -> action: %X ", i, _events[i].action);
        print_time_t("time: ", _events[i].time, 0);
    }
    return 0;
}


uint8_t WeeklyCalendar::next_event(
        time_t now,
        uint8_t * op,
        uint32_t * sleeptime
        )
{

    uint8_t res = 0;
    time_t next;

    *op = Operation_OP_NONE;

    next = _events[_next_exec].time;
    if (next < _ctime + 2 * _evt_precision)
    {
        *op = _events[_next_exec].action;
        *sleeptime = 0;

        _write_log("Performing action %d ", *op);
        print_time_t("scheduled at: ", _events[_next_exec].time, 0);

        _next_exec++;
    }
    else {
        now = time(nullptr);
        *sleeptime = (unsigned int)next - (unsigned int)now;

        //_write_log("%d\n", next);
        //_write_log("%d\n", now);
        //_write_log("%d\n", (unsigned int)*sleeptime);

    }


    return res;
}


time_t WeeklyCalendar::get_next_event_time() {
    return(_events[_next_exec].time);
}


time_t WeeklyCalendar::_last_occurrence(time_t offset, time_t time, time_t period) {

    time_t res;

    //first we compute the occurence since thursday 00:00
    res = offset;

    //last_occurrence
    res = time - ((time - res) % period);

    return res;
}


time_t WeeklyCalendar::_get_offset(uint8_t wday, time_t time) {

    //start_time in secs

    //wse is considered as happening in the first week after epoch
    //Returns offset in seconds from epoch (which is a Thusrday)
    //i.e.:
    //Monday: (THR + FRI + SAT + SUN) * SECONDS_PER_DAY

    time_t offset;
    offset = 0;

    if (wday != WeekDay_EVR) {
        offset += (wday + THU_TO_SUN) * SECS_PER_DAY;
    }

    offset += time;

    return offset;
}


time_t WeeklyCalendar::_get_period(uint8_t wday) {

    time_t period;

    if (wday != WeekDay_EVR) {
        period = SECS_PER_WEEK;
    }
    else {
        period = SECS_PER_DAY;
    }

    return period;
}


void WeeklyCalendar::print_time_tm(const char * text, struct tm * prt_time) {

    char buffer[26];
    strftime(buffer, 26, "%Y-%m-%d %H:%M:%S", prt_time);
    _write_log("%s%s\n\r", text, buffer);
}

void WeeklyCalendar::time_t_to_str(char * text, time_t t, uint8_t utc) {

    struct tm tmp;

    if (utc) {
        gmtime_r(&t, &tmp);
    }
    else {
        localtime_r(&t, &tmp);
    }

    strftime(text, 26, "%Y-%m-%d %H:%M:%S", &tmp);
}


void WeeklyCalendar::print_time_t(const char * text, time_t t, uint8_t utc) {

    char buffer[26];
    struct tm tmp;

    if (utc) {
        gmtime_r(&t, &tmp);
    }
    else {
        localtime_r(&t, &tmp);
    }

    strftime(buffer, 26, "%Y-%m-%d %H:%M:%S", &tmp);
    _write_log("%s%s\n\r", text, buffer);
}


void WeeklyCalendar::_write_log(const char *format, ...)
{
    va_list args;
    va_start(args, format);
    vprintf(format, args);
    va_end(args);
}


