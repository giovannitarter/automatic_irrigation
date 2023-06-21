#ifndef __GVC_GDEFINES__
#define __GVC_GDEFINES__


#ifndef STASSID
#define STASSID ""
#define STAPSK  ""
#endif

#define PIN_DISPLAY_EN GPIO_NUM_15
#define PIN_LED_ACT GPIO_NUM_23

#define PIN_TOUCH_1 GPIO_NUM_13
#define PIN_TOUCH_2 GPIO_NUM_12
#define PIN_TOUCH_3 GPIO_NUM_14

#define PIN_STEP_UP_EN GPIO_NUM_4

#define PIN_H_BRIDGE_EN_1 GPIO_NUM_18
#define PIN_H_BRIDGE_EN_2 GPIO_NUM_17
#define PIN_H_BRIDGE_EN_3 GPIO_NUM_16

#define PIN_H_BRIDGE_AIN_1 GPIO_NUM_25
#define PIN_H_BRIDGE_AIN_2 GPIO_NUM_26
#define PIN_H_BRIDGE_BIN_1 GPIO_NUM_32
#define PIN_H_BRIDGE_BIN_2 GPIO_NUM_33

#define BOOT_DELAY 1 //sec

#define EVT_TOLERANCE 10 //sec
#define SLEEP_MAX 60 * 60 * 2 //sec
#define SLEEP_MIN 5  //sec

#define MIN_DISPLAY 5000 //msec

#define OP_NONE  0x00
#define OP_SKIP  0x10
#define OP_OPEN  0x11
#define OP_CLOSE 0x12
    
#define TOUCH_THRESHOLD 80

void print_wakeup_reason(esp_sleep_wakeup_cause_t wakeup_reason);

uint8_t decode_action(uint8_t msg_action);
size_t read_schedule(char * buffer, size_t buflen);
void rtc_set_time(time_t time_p);
time_t rtc_get_time();



#endif


