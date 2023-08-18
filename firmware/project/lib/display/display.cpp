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
    
    delay(20);
    
    if(!display.begin(SSD1306_SWITCHCAPVCC, 0x3C, false, false)) {
        Serial.println(F("SSD1306 allocation failed"));
        return;
    }
    display.clearDisplay();
    display.setCursor(0, 0);

    disp_queue = xQueueCreate(10, sizeof(TextMessage));

    /* Create the task, storing the handle. */
    xReturned = xTaskCreate(
                    &displayTask,       /* Function that implements the task. */
                    "DisplayTask",   /* Text name for the task. */
                    2048,      /* Stack size in words, not bytes. */
                    this,    /* Parameter passed into the task. */
                    tskIDLE_PRIORITY,/* Priority at which the task is created. */
                    &xHandle
                    );      /* Used to pass out the created task's handle. */
}


 void Display::write(char * text, uint8_t line) {

     TextMessage tx;
     snprintf((char *)tx.text, 20, "%-20s", text);
     //memcpy(tx.text, text, 20);
     tx.line = line;

     xQueueSend(disp_queue, &tx, (TickType_t)0);
}



void Display::end() {

    
    int rem = begin_time + MIN_DISPLAY - millis();
    if (rem > 0) {
        vTaskDelay(rem / portTICK_PERIOD_MS);
    }
    
    gpio_set_level(pin_display_en, 1);
    gpio_hold_en(pin_display_en);
}
 

//do not add static here, to declare a static method is enough to
//add the static keyword in the header
//Adding static here means keep the function only in this file
void Display::displayTask(void * pvParameters) {

    TextMessage rx;
    Display * obj = static_cast<Display *>(pvParameters);

    for( ;; ) {
        if (xQueueReceive(obj->disp_queue, &rx, (TickType_t)portMAX_DELAY) == pdTRUE) {
            //Serial.printf("rx at time: %d\n", millis());
            
            obj->display.setTextSize(1);
            obj->display.setTextColor(WHITE, BLACK);
            obj->display.setCursor(0, rx.line * 10);
            
            // Display static text
            obj->display.print((char *)&(rx.text));
            obj->display.display(); 
            obj->last_write_time = millis();
        }
    }
    //vTaskDelay(1000 / portTICK_PERIOD_MS);
    //Serial.printf("ctime task: %d\n", millis());
}
