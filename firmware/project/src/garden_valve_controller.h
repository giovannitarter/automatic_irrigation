#ifndef __GVC_GDEFINES__
#define __GVC_GDEFINES__


#ifndef STASSID
#define STASSID ""
#define STAPSK  ""
#endif

#define PIN_DISPLAY_EN 15
#define PIN_LED_ACT 23

#define PIN_TOUCH_1 13
#define PIN_TOUCH_2 12
#define PIN_TOUCH_3 14

#define PIN_STEP_UP_EN 4

#define PIN_H_BRIDGE_EN_1 18
#define PIN_H_BRIDGE_EN_2 17
#define PIN_H_BRIDGE_EN_3 16

#define PIN_H_BRIDGE_AIN_1 25
#define PIN_H_BRIDGE_AIN_2 26
#define PIN_H_BRIDGE_BIN_1 32
#define PIN_H_BRIDGE_BIN_2 33

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


