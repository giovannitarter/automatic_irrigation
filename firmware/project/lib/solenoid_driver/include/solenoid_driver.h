#ifndef __GVC_SOLENOID_DRIVER_H
#define __GVC_SOLENOID_DRIVER_H

#define CAP_CHARGE_TIME 700
#define SOLENOID_PULSE_TIME 50

class SolenoidDriver {

    public:
        
        SolenoidDriver(
                gpio_num_t pin_step_up, 
                gpio_num_t pin_stby,
                gpio_num_t pin_h1,
                gpio_num_t pin_h2
                );
        
        ~SolenoidDriver();

        void begin();
        void end();

        void open_valve();
        void close_valve();

    
    private:
        gpio_num_t pin_su;
        gpio_num_t pin_stby;
        gpio_num_t pin_h1;
        gpio_num_t pin_h2;

};

#endif
