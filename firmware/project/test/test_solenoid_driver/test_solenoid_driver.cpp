#include <Arduino.h>
#include <ctime>

#include <unity.h>

#define STASSID ""
#define STAPSK ""

#include "garden_valve_controller.h"
#include "solenoid_driver.h"


void setUp(void) {
    // set stuff up here
}

void tearDown(void) {
    // clean stuff up here
}

void test_open(void) {
    
    SolenoidDriver sld = SolenoidDriver(
        PIN_STEP_UP_EN,
        PIN_H_BRIDGE_EN_1,
        PIN_H_BRIDGE_BIN_1,
        PIN_H_BRIDGE_BIN_2
        );

    sld.begin();

    sld.open_valve();
   
    delay(1000);
    
    sld.close_valve();

}


void setup()
{
    UNITY_BEGIN();

    RUN_TEST(test_open);
    
    UNITY_END(); // stop unit testing
}

void loop()
{
}
