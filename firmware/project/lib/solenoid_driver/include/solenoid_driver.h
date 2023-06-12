#ifndef __GVC_SOLENOID_DRIVER_H
#define __GVC_SOLENOID_DRIVER_H

#define CAP_CHARGE_TIME 1000
#define SOLENOID_PULSE_TIME 50

class SolenoidDriver {

    public:
        
        SolenoidDriver();

        void init(
                uint8_t pin_step_up, 
                uint8_t pin_stby,
                uint8_t pin_h1,
                uint8_t pin_h2
                );

        void open_valve();
        void close_valve();

    
    private:
        uint8_t pin_su;
        uint8_t pin_stby;
        uint8_t pin_h1;
        uint8_t pin_h2;

};

#endif
