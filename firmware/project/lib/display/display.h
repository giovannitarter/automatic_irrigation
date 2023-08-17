#ifndef __AUTO_IRR_DISPLAY_H
#define __AUTO_IRR_DISPLAY_H

#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>


#define SCREEN_WIDTH 128 // OLED display width, in pixels
#define SCREEN_HEIGHT 32 // OLED display height, in pixels

#define MIN_DISPLAY 5000 //msec



struct TextMessage {
    uint8_t line;
    uint8_t text[20];
};
typedef struct TextMessage TextMessage;


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

       BaseType_t xReturned;
       TaskHandle_t xHandle = NULL;

       static void displayTask(void * pvParameters);

        QueueHandle_t disp_queue;

};
       

#endif
