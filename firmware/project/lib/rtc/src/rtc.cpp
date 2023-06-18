#include <Arduino.h>
#include <ctime>

#include "rtc.h"


RTC::RTC()
{
}


void RTC::init()
{
}


uint8_t RTC::getDateTime(struct tm * dt)
{


    uint8_t res = 0;
    uint32_t timeout;
    uint16_t tmp[RTC_BYTES];

    //Serial.println("getDateTime");

    Wire.beginTransmission(RTC_ADDR);
    Wire.write(0x00);
    res = Wire.endTransmission(1);
    //Serial.printf("res: %d\n", res);
       
    timeout = millis() + 100;
    Wire.requestFrom(RTC_ADDR, RTC_BYTES, 1);

    while(Wire.available() != RTC_BYTES && millis() < timeout);
    //Serial.printf("available: %d\n", Wire.available());

    for(uint8_t i; i<RTC_BYTES; i++) {
        tmp[i] = Wire.read();
        //Serial.printf("%01X ", tmp[i]);
    }
    //Serial.println("");

    if (tmp[REG_STATUS] & 0b1000000) {
        res = 1;
    }

    dt->tm_sec = _bcd2dec(tmp[REG_SEC] & 0b01111111);
    dt->tm_min = _bcd2dec(tmp[REG_MIN] & 0b01111111);
    dt->tm_hour = _bcd2dec(tmp[REG_HOUR] & 0b00111111);
    dt->tm_mday = _bcd2dec(tmp[REG_MDAY] & 0b00111111);
    dt->tm_mon  = _bcd2dec(tmp[REG_MON] & 0b00011111);
    dt->tm_year = _bcd2dec(tmp[REG_YEAR] & 0b11111111) + 100;
    dt->tm_isdst = 0;
    dt->tm_yday = 0;
    dt->tm_wday = 0;

    return res;
}


uint8_t RTC::setDateTime(struct tm * dt)
{

    uint8_t res = 0;

    Wire.beginTransmission(RTC_ADDR);
    Wire.write(0x00); 
    Wire.write(_dec2bcd(dt->tm_sec  % 60 ));
    Wire.write(_dec2bcd(dt->tm_min  % 60 ));
    Wire.write(_dec2bcd(dt->tm_hour % 24 ));
    Wire.write(_dec2bcd((dt->tm_wday % 7) + 1));
    Wire.write(_dec2bcd(dt->tm_mday % 32 ));
    Wire.write(_dec2bcd(dt->tm_mon  % 13 ));
    Wire.write(_dec2bcd((dt->tm_year - 100) % 100));
    res = Wire.endTransmission();
    
    Wire.beginTransmission(RTC_ADDR);
    Wire.write(REG_CONTROL); 
    Wire.write(0x00);
    Wire.write(0x00);
    res = Wire.endTransmission();

    return res;
}


uint8_t RTC::_getByte(uint8_t addr, uint8_t * val) {
    
    uint8_t res = 0;
    uint32_t timeout;
    
    Wire.beginTransmission(RTC_ADDR);
    Wire.write(addr);
    res = Wire.endTransmission();

    if (res != 0) {
        return res;
    }
       
    timeout = millis() + 100;
    Wire.requestFrom(RTC_ADDR, 1);
    
    while(! Wire.available() && millis() < timeout);

    if (Wire.available()) {
        *val = Wire.read();
    }
    else {
        res = 15;
    }

    return res;
}


uint8_t RTC::time_valid() {
    // returns 1 if time is still valid
    // checking osc stop bit
    
    uint8_t res, val, sec;
    
    res = _getByte(REG_STATUS, &val);
    if (res != 0) {
        Serial.printf("Cannot read byte %d\n", res);
        return 0;
    }
    
    res = _getByte(REG_SEC, &sec);
    if (res != 0) {
        Serial.printf("Cannot read byte %d\n", res);
        return 0;
    }

    if (sec > 0x59) {
        Serial.printf("Invalid sec byte %d\n", sec);
        return 0;
    }

    return ((val & 0b10000000) == 0);
}


uint8_t _bcd2dec(uint8_t bcd) {
    return (bcd >> 4) * 10 + (bcd & 0x0F);
}


uint8_t _dec2bcd(uint8_t dec) {
    return ((dec / 10) << 4) | (((dec % 10) & 0x0F));
}


