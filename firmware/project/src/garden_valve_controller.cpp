#include <Arduino.h>
#include <LittleFS.h>
#include <WiFi.h>
#include <sntp.h>
#include <ctime>
#include <HTTPClient.h>
#include <Wire.h>
#include <esp_wifi.h>
#include "soc/soc.h"
#include "soc/rtc_cntl_reg.h"
#include <esp32fota.h>


#include "garden_valve_controller.h"
#include "helpers.h"
#include "rtc.h"
#include "weekly_calendar.h"
#include "solenoid_driver.h"
#include "display.h"


#define mainAUTO_RELOAD_TIMER_PERIOD portTICK_PERIOD_MS * 1000

TimerHandle_t xAutoReloadTimer;
BaseType_t xTimer2Started;
       
//Globals
const int VALVE_DELAY = BOOT_DELAY + CAP_CHARGE_TIME + SOLENOID_PULSE_TIME;
int use_display;
uint32_t display_on_time;
uint8_t config, k_wtr_start, k_wtr_stop;
esp_sleep_wakeup_cause_t wakeup_reason;


SemaphoreHandle_t sem, time_sync;

uint8_t buffer[SCHEDULE_MAXLEN];
size_t buflen;

time_t now;
struct tm tm_now;
bool need_sync;
uint32_t conn_timeout;


SolenoidDriver slds[SOLENOID_DRV_NR] {
    SolenoidDriver(
        PIN_STEP_UP_EN,
        PIN_H_BRIDGE_EN_1,
        PIN_H_BRIDGE_BIN_1,
        PIN_H_BRIDGE_BIN_2
        ),
    SolenoidDriver(
        PIN_STEP_UP_EN,
        PIN_H_BRIDGE_EN_1,
        PIN_H_BRIDGE_AIN_1,
        PIN_H_BRIDGE_AIN_2
        ),
    SolenoidDriver(
        PIN_STEP_UP_EN,
        PIN_H_BRIDGE_EN_2,
        PIN_H_BRIDGE_BIN_1,
        PIN_H_BRIDGE_BIN_2
        ),
    SolenoidDriver(
        PIN_STEP_UP_EN,
        PIN_H_BRIDGE_EN_2,
        PIN_H_BRIDGE_AIN_1,
        PIN_H_BRIDGE_AIN_2
        ),
    SolenoidDriver(
        PIN_STEP_UP_EN,
        PIN_H_BRIDGE_EN_3,
        PIN_H_BRIDGE_BIN_1,
        PIN_H_BRIDGE_BIN_2
        ),
    SolenoidDriver(
        PIN_STEP_UP_EN,
        PIN_H_BRIDGE_EN_3,
        PIN_H_BRIDGE_AIN_1,
        PIN_H_BRIDGE_AIN_2
        )
    };


RTC rtc = RTC();
WeeklyCalendar wk = WeeklyCalendar();
Display display = Display(PIN_DISPLAY_EN);

char tmp_url[40];
String url("");
char chip_id[13];
esp32FOTA esp32FOTA("esp32-fota-http", CVERSION, false);



//Callback called when NTP acquires time
void time_is_set(struct timeval * tv) {

  time_t now;

  now = time(nullptr);
  rtc_set_time(now);

  Serial.print("Writing time to rtc: ");
  Serial.println(now);

  now = rtc_get_time();
  Serial.print("Verify: ");
  Serial.println(now);

  xSemaphoreGive(time_sync);
}


void system_set_time(time_t curr_time) {
    timeval tv = {curr_time, 0};
    settimeofday(&tv, nullptr);
    return;
}


void rtc_set_time(time_t time_p) {

    struct tm tmp;

    gmtime_r(&time_p, &tmp);

    wk.print_time_tm("setting to rtc: ", &tmp);
    rtc.setDateTime(&tmp);
    //rtc.writeRam(ADDR_CNT, 0);
}


time_t rtc_get_time() {

    time_t res, offset;
    struct tm tmp, epoch;

    rtc.getDateTime(&tmp);

    //printTm("get from rtc: ", &tmp);

    memset(&epoch, 0, sizeof(struct tm));
    epoch.tm_mday = 2;
    epoch.tm_year = 70;
    offset = mktime(&epoch) - 60*60*24;

    // Now we are ready to convert tm to time_t in UTC.
    // as mktime adds timezone, subtracting offset(=timezone) gives us the right result
    res = mktime(&tmp) - offset;

    return(res);
}


void set_time_boot(int config) {

    //RTC stores time in UTC

    time_t rtc_time, now;
    struct tm now_tm;
    uint8_t time_valid;
    uint32_t timeout;

    time_valid = rtc.time_valid();
    Serial.printf("time_valid: %d\n", time_valid);

    setenv("TZ", TZ, 3);
    tzset();

    //uint8_t cnt;
    //rtc.readRam(ADDR_CNT, &cnt);
    //Serial.printf("cnt: %d\n\r", cnt);
    //rtc.writeRam(ADDR_CNT, cnt + 1);

    rtc_time = rtc_get_time();

    Serial.print("rtc_time: ");
    Serial.println((unsigned int)rtc_time);

    need_sync = rtc_time < TIME_MIN 
        || config 
        || ! time_valid
        || rtc_time == 1928419089;

    Serial.print("need_sync: ");
    Serial.println(need_sync);

    if (need_sync) {

        Serial.println("Error on RTC, sync from the internet");

        now = time(nullptr);
        wk.print_time_t("localtime:", now, 0);
        Serial.println();
        
        //vTaskDelay(4000 * portTICK_PERIOD_MS);
        //delay(4000);

        conn_timeout = millis() + 10000;
        esp_wifi_start();
        WiFi.mode(WIFI_STA);
        WiFi.begin(STASSID, STAPSK);


        //timeout = millis() + 10000;
        //while (WiFi.status() != WL_CONNECTED) {
        //  delay(500);
        //  Serial.print(".");
        //  if (millis() > timeout) {
        //    Serial.println("Connection timeout");
        //    break;
        //  }
        //}

    }
    else {
        Serial.println("RTC is ok, setting time to system clock");
        system_set_time(rtc_time);
    }
        
}


uint8_t decode_action(uint8_t msg_action) {

    uint8_t action;

    switch (msg_action) {
        
        case Operation_OP_OPEN:
           action = OP_OPEN;
           break;
        
        case Operation_OP_CLOSE:
           action = OP_CLOSE;
           break;
        
        case Operation_OP_SKIP:
           action = OP_SKIP;
           break;

        default:
           action = OP_NONE;
            break;
    }

    return action;
}



void callback(){
  //placeholder callback function
}


void download_config() {
    WiFiClient client;
    HTTPClient http;
    
    char addr[50];
    snprintf(addr, 50, "http://%s:%s/getschedule", ESPSERVER, ESPPORT);
    Serial.printf("addr: %s\n", addr);
    
    http.begin(client, addr);
    
    int httpCode = http.GET();
    if (httpCode == HTTP_CODE_OK) {
        //Serial.println(http.getSize());
      
        LittleFS.begin(true);
        File f = LittleFS.open("/schedule.txt", "w");
        uint8_t buff[20] = { 0 };
        int len = http.getSize();

        while (http.connected() && (len > 0 || len == -1)) {
            // read up to 128 byte
            int c = client.readBytes(buff, std::min((size_t)len, sizeof(buff)));

            // write it to Serial
            f.write(buff, c);

            if (len > 0) { 
                len -= c; 
            }
        }
        f.close();
        LittleFS.end();
    }
    else {
        Serial.printf("Http error: %d\n", httpCode);
    }
    http.end();
}


void WiFiEvent(WiFiEvent_t event) {

    Serial.printf("[WiFi-event] event: %d\n", event);

    if (event == ARDUINO_EVENT_WIFI_STA_GOT_IP) {
    //if (event == ARDUINO_EVENT_WIFI_STA_CONNECTED) {
        
        Serial.println("");
        Serial.println("WiFi connected");
        Serial.println("IP address: ");
        Serial.println(WiFi.localIP());
        
        bool updatedNeeded = esp32FOTA.execHTTPcheck();
        if (updatedNeeded)
        {
            Serial.println("FOTA Update avalable, updating!");
            esp32FOTA.execOTA();
            return;
        }

        sntp_setoperatingmode(SNTP_OPMODE_POLL);
        sntp_setservername(0, NTPSERVER);
        sntp_set_sync_mode(SNTP_SYNC_MODE_IMMED);
        sntp_set_time_sync_notification_cb(time_is_set);
        sntp_init();
  
        xSemaphoreTake(time_sync, portTICK_PERIOD_MS * 6000);

        download_config();
        xSemaphoreGive(sem);
    }
}
        


void prvAutoReloadTimerCallback(TimerHandle_t xTimer) {

    //uint32_t ctime = time(nullptr);
    Serial.printf("ctime: %d\n", millis());
    if (use_display) {
        char text[26];
        now = time(nullptr);
        wk.time_t_to_str(text, now, 0);
        display.write(text, 0);
    }
}


void hibernate() {

    Serial.printf("\nhibernate\n"); 
    Serial.flush();
    Serial.end();
    
    gpio_set_level(PIN_LED_ACT, 0);
 
    gpio_set_level(PIN_LATCH, 0);
    
    delay(300);
}


void setup() {
    
    
    use_display = 0;
    need_sync = 0;
    wakeup_reason = esp_sleep_get_wakeup_cause();

    
    for (uint8_t i=0; i<SOLENOID_DRV_NR; i++) {
        slds[i].begin();
    }

    gpio_set_direction(PIN_LED_ACT, GPIO_MODE_OUTPUT);
    gpio_set_level(PIN_LED_ACT, 1);
    gpio_hold_dis(PIN_LED_ACT);
    
    gpio_set_direction(PIN_LATCH, GPIO_MODE_OUTPUT);
    gpio_set_level(PIN_LATCH, 1);
    gpio_hold_dis(PIN_LATCH);
    
    gpio_reset_pin(PIN_TOUCH_1);
    gpio_reset_pin(PIN_TOUCH_2);
    gpio_reset_pin(PIN_TOUCH_3);
    
    gpio_pullup_dis(PIN_TOUCH_1);
    gpio_pullup_dis(PIN_TOUCH_2);
    gpio_pullup_dis(PIN_TOUCH_3);
    
    gpio_pulldown_en(PIN_TOUCH_1);
    gpio_pulldown_en(PIN_TOUCH_2);
    gpio_pulldown_en(PIN_TOUCH_3);
    
    gpio_set_direction(PIN_TOUCH_1, GPIO_MODE_INPUT);
    gpio_set_direction(PIN_TOUCH_2, GPIO_MODE_INPUT);
    gpio_set_direction(PIN_TOUCH_3, GPIO_MODE_INPUT);

    gpio_hold_dis(PIN_TOUCH_1);
    gpio_hold_dis(PIN_TOUCH_2);
    gpio_hold_dis(PIN_TOUCH_3);
    
    rtc.init();

    use_display = 1;

    snprintf(tmp_url, 40, "http://%s:%d/fota/manifest", ESPSERVER, ESPPORT);
    url.concat(String(tmp_url));
    esp32FOTA.setManifestURL(url);


    delay(100);

    Serial.begin(115200);
    Serial.println("\n\nBOOT");
    print_wakeup_reason(wakeup_reason);

    Serial.printf("config: %d\n\r", config);
    Serial.printf("use_display: %d\n\r", use_display);
    Serial.printf("STASSID : %s\n\r", STASSID);
    Serial.printf("STAPSK  : %s\n\r", STAPSK);
    Serial.printf("ESPSRV  : %s\n\r", ESPSERVER);
    Serial.printf("ESPPORT : %s\n\r", ESPPORT);
    
    //Serial.printf("touch 1: %d\n", aa);
    Serial.printf("touch 1: %d\n", gpio_get_level(PIN_TOUCH_1));
    Serial.printf("touch 2: %d\n", gpio_get_level(PIN_TOUCH_2));
    Serial.printf("touch 3: %d\n", gpio_get_level(PIN_TOUCH_3));
    
    config      = gpio_get_level(PIN_TOUCH_1);
    k_wtr_start = gpio_get_level(PIN_TOUCH_2);
    k_wtr_stop  = gpio_get_level(PIN_TOUCH_3);
    uint32_t res;
    
    res = Wire.begin(-1, -1, 100000UL);
    //Serial.printf("Wire res: %d\n", res);
    
    display.begin();

    sem = xSemaphoreCreateBinary();
    
    if ( ! sem) {
        Serial.println("Error creating mutex");
    }
    
    time_sync = xSemaphoreCreateBinary();
    if ( ! time_sync) {
        Serial.println("Error creating mutex");
    }
    
    WiFi.onEvent(WiFiEvent);

    char text[20];
    snprintf(text, 20, "config: %d", config);
    display.write(text, 1);

    set_time_boot(config);
    
    time_t test = time(nullptr);
    if (test < TIME_MIN) {
        Serial.println("ERROR, cannot sync\n");
        delay(2000);
        slds[0].close_valve();
    }

    
    xAutoReloadTimer = xTimerCreate(

        /* Text name for the software timer - not used by FreeRTOS. */
        "AutoReload",
        
        /* The software timer's period in ticks. */
        //mainAUTO_RELOAD_TIMER_PERIOD,
        portTICK_PERIOD_MS * 1000,
        
        /* Setting uxAutoRealod to pdTRUE creates an auto-reload timer. */
        pdTRUE,
        
        /* This example does not use the timer id. */
        0,

        /* The callback function to be used by the software timer being created. */
        prvAutoReloadTimerCallback 
    );


    /* Check the software timers were created. */
    if ( xAutoReloadTimer != NULL ) {

        /* Start the software timers, using a block time of 0 (no block time). 
         * The scheduler has not been started yet so any block time specified 
         * here would be ignored anyway. 
        */
        xTimer2Started = xTimerStart( xAutoReloadTimer, 0 );
    }
    
    prvAutoReloadTimerCallback(NULL);

    //scheduler is already started

    //Serial.println("end setup\n");
}


void loop() {


    //Serial.println("Time to open");
    //slds[0].open_valve();
    //delay(1000);
    //Serial.println("Time to close");
    //slds[0].close_valve();

    //return;
     
    if (need_sync) {
        Serial.printf("Need sync\n");
        xSemaphoreTake(sem, portTICK_PERIOD_MS * 10000);
        //if (xSemaphoreTake(sem, portTICK_PERIOD_MS * 10000) == pdFALSE) {
            //Serial.printf("Not synched yet, waiting %d\r", millis());
            //return;
        //}
    }

    esp_wifi_stop();

    if (k_wtr_start) {
        Serial.println("Time to open");
        display.write("Open           ", 1);
        slds[0].open_valve();
        //hibernate();
    }
    else if (k_wtr_stop) {
        Serial.println("Time to close");
        display.write("Close         ", 1);
        slds[0].close_valve();
        //hibernate();
    }

    now = time(nullptr);
    Serial.print("UTC time from system:  ");
    Serial.println((unsigned int)now);

    Serial.print("UTC time from rtc:     ");
    now = rtc_get_time();
    Serial.println((unsigned int)now);

    wk.print_time_t("localtime from system: ", now, 0);

   
    uint32_t sleeptime;
    sleeptime = 30;
    uint8_t op;

    Serial.println("");

    op = OP_NONE;
    buflen = SCHEDULE_MAXLEN;
    
    buflen = read_schedule((char *)buffer, buflen);

    wk.init(now, buffer, buflen, EVT_TOLERANCE); 
    
    sleeptime = 0;
    while(sleeptime == 0) {

        wk.next_event(now, &op, &sleeptime);
        
        op = decode_action(op);

        switch (op) {

            case OP_SKIP:
                Serial.println("Time to skip");
                break;

            case OP_OPEN:
                Serial.println("Time to open");
                slds[0].open_valve();
                break;

            case OP_CLOSE:
                Serial.println("Time to close");
                slds[0].close_valve();
                break;

            default:
                break;

        }
    }
    
    if (use_display) {
        char text[26];
        time_t next_evt_time = wk.get_next_event_time();
        wk.time_t_to_str(text, next_evt_time, 0);
        display.write(text, 2);
    }
    
    for (uint8_t i=0; i<SOLENOID_DRV_NR; i++) {
        slds[i].end();
    }
    
    display.end();

    now = time(nullptr);
    now = ((now + 60) / 60) * 60;

    struct tm tmp;
    gmtime_r(&now, &tmp);
    rtc.setAlarm(&tmp);
    wk.print_time_tm("alarm set for: ", &tmp);

    hibernate();
}


