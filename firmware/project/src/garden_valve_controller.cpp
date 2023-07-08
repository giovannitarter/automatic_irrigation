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
    snprintf(addr, 50, "http://%s:%d/getschedule", ESPSERVER, ESPPORT);
    
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
        Serial.println("Http error");
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


void setup() {
    
    use_display = 0;
    need_sync = 0;
    wakeup_reason = esp_sleep_get_wakeup_cause();

    //WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 0);
    
    for (uint8_t i=0; i<SOLENOID_DRV_NR; i++) {
        slds[i].begin();
    }

    gpio_set_direction(PIN_LED_ACT, GPIO_MODE_OUTPUT);
    gpio_set_level(PIN_LED_ACT, 1);
    gpio_hold_dis(PIN_LED_ACT);
    
    gpio_deep_sleep_hold_dis();    
    
    rtc.init();

    if (wakeup_reason == ESP_SLEEP_WAKEUP_TOUCHPAD) {
        
        use_display = 1;
        
        if (touchRead(T4) < TOUCH_THRESHOLD) {
            config = 1;
        }
        
        if (touchRead(T5) < TOUCH_THRESHOLD) {
            k_wtr_start = 1;
        }
        
        if (touchRead(T6) < TOUCH_THRESHOLD) {
            k_wtr_start = 1;
        }
    }
    else if (wakeup_reason == ESP_SLEEP_WAKEUP_UNDEFINED) {
        use_display = 1; 
    }
    else {
        config = digitalRead(PIN_TOUCH_1);
        use_display = 1;
    }

    snprintf(tmp_url, 40, "http://%s:%d/fota/manifest", ESPSERVER, ESPPORT);
    url.concat(String(tmp_url));
    esp32FOTA.setManifestURL(url);


    delay(100);

    Serial.begin(115200);
    Serial.println("\n\nBOOT");
    print_wakeup_reason(wakeup_reason);

    Serial.printf("config: %d\n\r", config);
    Serial.printf("use_display: %d\n\r", use_display);
    Serial.printf("STASSID: %s\n\r", STASSID);
    Serial.printf("STAPSK: %s\n\r", STAPSK);
    
    uint32_t res;
    
    res = Wire.begin(-1, -1, 100000UL);
    Serial.printf("Wire res: %d\n", res);
    
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

    set_time_boot(config);
    
    time_t test = time(nullptr);
    if (test < TIME_MIN) {
        Serial.println("ERROR, cannot sync\n");
        slds[0].close_valve();
    }

    //Serial.println("end setup\n");
}


void loop() {
     
    if (need_sync) {
        Serial.printf("Need sync\n");
        xSemaphoreTake(sem, portTICK_PERIOD_MS * 10000);
        //if (xSemaphoreTake(sem, portTICK_PERIOD_MS * 10000) == pdFALSE) {
            //Serial.printf("Not synched yet, waiting %d\r", millis());
            //return;
        //}
    }

    esp_wifi_stop();
    
    digitalWrite(PIN_LED_ACT, LOW);
    
    if (use_display) {
        char text[26];
        now = time(nullptr);
        wk.time_t_to_str(text, now, 0);
        display.write(text, 0);
    }

    now = time(nullptr);
    Serial.print("UTC time from system:  ");
    Serial.println((unsigned int)now);

    Serial.print("UTC time from rtc:     ");
    now = rtc_get_time();
    Serial.println((unsigned int)now);

    wk.print_time_t("localtime from system: ", now, 0);

    
    //try settinf an alarm
    struct tm tmp;
    gmtime_r(&now, &tmp);
    tmp.tm_sec = 0;
    tmp.tm_min += 1;
    rtc.setAlarm(&tmp);
    wk.print_time_tm("alarm set for: ", &tmp);


    uint32_t sleeptime;
    sleeptime = 30;
    uint8_t op;

    Serial.println("");

    sleeptime = 0;
    op = OP_NONE;
    buflen = SCHEDULE_MAXLEN;
    
    buflen = read_schedule((char *)buffer, buflen);

    wk.init(now, buffer, buflen, EVT_TOLERANCE); 
    if (use_display) {
        char text[26];
        time_t next_evt_time = wk.get_next_event_time();
        wk.time_t_to_str(text, next_evt_time, 0);
        display.write(text, 2);
    }

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
        while(millis() < display_on_time + MIN_DISPLAY);
        delay(100);
    }
    
    
    for (uint8_t i=0; i<SOLENOID_DRV_NR; i++) {
        slds[i].end();
    }
    
    display.end();
    
    gpio_set_level(PIN_LED_ACT, 0);
    gpio_hold_en(PIN_LED_ACT);

    gpio_deep_sleep_hold_en();
    
 
    time_t req_time = time(nullptr) - now;
    if (req_time < 0) {
        req_time = 0;
    }   
    Serial.printf("All action performed in %d sec\n\r", (uint32_t) req_time);

    if (sleeptime > SLEEP_MAX) {
        sleeptime = SLEEP_MAX;
    }
    else if (sleeptime < SLEEP_MIN) {
        sleeptime = SLEEP_MIN;
    }
    else {
        sleeptime -= req_time;
        sleeptime -= BOOT_DELAY;
    }

    Serial.printf("Will sleep for: %d\n\r", sleeptime);
    sleeptime = sleeptime * 1e6;

    esp_sleep_enable_touchpad_wakeup();
    
    touchAttachInterrupt(T4, callback, TOUCH_THRESHOLD);
    touchAttachInterrupt(T5, callback, TOUCH_THRESHOLD);
    touchAttachInterrupt(T6, callback, TOUCH_THRESHOLD);

    ESP.deepSleep(sleeptime);
}


