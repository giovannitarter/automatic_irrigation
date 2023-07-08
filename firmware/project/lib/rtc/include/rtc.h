#ifndef _DS_RTC_H
#define _DS_RTC_H

#include <stdint.h>
#include <Wire.h>


#define RTC_BYTES 0x12
#define RTC_ADDR 0b1101000
#define READ_TIMEOUT 200

#define REG_SEC               0x00
#define REG_MIN               0x01
#define REG_HOUR              0x02
#define REG_WDAY              0x03
#define REG_MDAY              0x04
#define REG_MON               0x05
#define REG_YEAR              0x06

#define REG_CONTROL           0x0E
#define REG_STATUS            0x0F


uint8_t _bcd2dec(uint8_t bcd);
uint8_t _dec2bcd(uint8_t dec);

class RTC
{
    public:

        RTC();
        void init();
        uint8_t getDateTime(struct tm * dt);
        uint8_t setDateTime(struct tm * dt);
        uint8_t setAlarm(struct tm * dt);
        uint8_t time_valid();
        uint8_t dump_memory(uint8_t addr, uint8_t size);

    private:
        uint8_t _getByte(uint8_t addr, uint8_t * val);

};

#endif

