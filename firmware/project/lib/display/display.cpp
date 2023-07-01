#include "display.h"


Display::Display(gpio_num_t pin_display_p) {
    
    //At boot GPIO_NUM_15 is assigned to JTAG and should be assigned to
    //GPIO
    pin_display_en = pin_display_p;
    
    gpio_reset_pin(pin_display_en);
    gpio_set_direction(pin_display_en, GPIO_MODE_OUTPUT);
    gpio_set_level(pin_display_en, 1);
    gpio_hold_dis(pin_display_en);
}


Display::~Display() {

}

void Display::begin() {

    begin_time = millis();
    display = Adafruit_SSD1306(
            SCREEN_WIDTH, 
            SCREEN_HEIGHT, 
            &Wire, 
            -1, 
            100000UL, 
            100000UL
            );
    
    gpio_set_direction(pin_display_en, GPIO_MODE_OUTPUT);
    gpio_set_level(pin_display_en, 0);
    
    delay(300);
    
    if(!display.begin(SSD1306_SWITCHCAPVCC, 0x3C, false, false)) {
        Serial.println(F("SSD1306 allocation failed"));
        return;
    }
    display.clearDisplay();
    display.setCursor(0, 0);
}


void Display::write(char * text, uint8_t line) {
    
    display.setTextSize(1);
    display.setTextColor(WHITE);
    display.setCursor(0, line * 10);
    

    // Display static text
    display.print(text);
    display.display(); 
    last_write_time = millis();
}



void Display::end() {
    gpio_set_level(pin_display_en, 1);
    gpio_hold_en(pin_display_en);
}
