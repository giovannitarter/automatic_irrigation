#include <Arduino.h>
#include <ctime>

#include <unity.h>
#include "rtc.h"

void setUp(void) {
    // set stuff up here
}

void tearDown(void) {
    // clean stuff up here
}

void test_bcd_dec(void) {

    for(uint8_t i = 0; i < 99; i++) {
        TEST_ASSERT_EQUAL(_bcd2dec(_dec2bcd(i)), i);
    } 

}


void setup()
{
    UNITY_BEGIN();

    RUN_TEST(test_bcd_dec);
    
    UNITY_END(); // stop unit testing
}

void loop()
{
}
