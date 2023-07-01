#include <Arduino.h>
#include <stdint.h>

#include "solenoid_driver.h"

SolenoidDriver::SolenoidDriver(
    gpio_num_t pin_step_up_p, 
    gpio_num_t pin_stby_p,
    gpio_num_t pin_h1_p,
    gpio_num_t pin_h2_p
    ) 
{
    pin_su = pin_step_up_p;
    pin_stby = pin_stby_p;
    pin_h1 = pin_h1_p;
    pin_h2 = pin_h2_p;
}

SolenoidDriver::~SolenoidDriver() {
}

void SolenoidDriver::end() {
    
    gpio_set_level(pin_su, 0);
    gpio_set_level(pin_stby, 0);
    gpio_set_level(pin_h1, 0);
    gpio_set_level(pin_h2, 0);
    
    gpio_hold_en(pin_su);
    gpio_hold_en(pin_stby);
    gpio_hold_en(pin_h1);
    gpio_hold_en(pin_h2);
};


void SolenoidDriver::begin(
) 
{
    gpio_set_direction(pin_su, GPIO_MODE_OUTPUT);
    gpio_set_direction(pin_stby, GPIO_MODE_OUTPUT);
    gpio_set_direction(pin_h1, GPIO_MODE_OUTPUT);
    gpio_set_direction(pin_h2, GPIO_MODE_OUTPUT);
    
    gpio_set_level(pin_su, 0);
    gpio_set_level(pin_stby, 0);
    gpio_set_level(pin_h1, 0);
    gpio_set_level(pin_h2, 0);

    gpio_hold_dis(pin_su);
    gpio_hold_dis(pin_stby);
    gpio_hold_dis(pin_h1);
    gpio_hold_dis(pin_h2);
}


void SolenoidDriver::close_valve() {

    gpio_set_level(pin_su, 1);

    //Serial.println("Charging...");
    delay(CAP_CHARGE_TIME);
    //Serial.println("Charged!");

    gpio_set_level(pin_h1, 1);
    gpio_set_level(pin_h2, 0);

    //Serial.println("Activation");
    gpio_set_level(pin_stby, 1);
    delay(SOLENOID_PULSE_TIME);
    //Serial.println("Stop");

    gpio_set_level(pin_stby, 0);
    gpio_set_level(pin_su, 0);
    gpio_set_level(pin_h1, 0);
    gpio_set_level(pin_h2, 0);

}


void SolenoidDriver::open_valve() {
    
    gpio_set_level(pin_su, 1);

    //Serial.println("Charging...");
    delay(CAP_CHARGE_TIME);
    //Serial.println("Charged!");

    gpio_set_level(pin_h1, 0);
    gpio_set_level(pin_h2, 1);

    //Serial.println("Activation");
    gpio_set_level(pin_stby, 1);
    delay(SOLENOID_PULSE_TIME);
    //Serial.println("Stop");

    gpio_set_level(pin_stby, 0);
    gpio_set_level(pin_su, 0);
    gpio_set_level(pin_h1, 0);
    gpio_set_level(pin_h2, 0);
}
