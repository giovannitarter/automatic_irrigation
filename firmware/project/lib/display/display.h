#ifndef __AUTO_IRR_DISPLAY_H
#define __AUTO_IRR_DISPLAY_H

#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>


#define SCREEN_WIDTH 128 // OLED display width, in pixels
#define SCREEN_HEIGHT 32 // OLED display height, in pixels


class Display 
{

    public:

        Display(gpio_num_t pin_display_p);
        ~Display();

        void begin();
        void end();
        void write(char * text, uint8_t line);


    private:

       Adafruit_SSD1306 display;

       gpio_num_t pin_display_en;
       
       uint32_t begin_time;
       uint32_t last_write_time;

        
    


};


#endif
