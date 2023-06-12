#include <Arduino.h>
#include <LittleFS.h>
#include <WiFi.h>
#include <sntp.h>
#include <ctime>
#include <HTTPClient.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>


#include "garden_valve_controller.h"
#include "rtc.h"
#include "weekly_calendar.h"
#include "solenoid_driver.h"

#define TIME_MIN 1640995200

#define ADDR_CNT 0x01

#define ADDR_CHECK 0x02
#define VAL_CHECK 0b0101010101

#define ADDR_NEXTOP 0x03


//Globals
int dir;
const int VALVE_DELAY = BOOT_DELAY + CAP_CHARGE_TIME + SOLENOID_PULSE_TIME;
int use_display;
int config;
esp_sleep_wakeup_cause_t wakeup_reason;

#define TZ "CET-1CEST,M3.5.0/2,M10.5.0/3"


RTC rtc = RTC();
WeeklyCalendar wk = WeeklyCalendar();
SolenoidDriver soldrv = SolenoidDriver();

#define SCHEDULE_MAXLEN 256
uint8_t buffer[SCHEDULE_MAXLEN];
size_t buflen;


time_t now;
struct tm tm_now;
bool time_sync;
bool need_sync;


#define SCREEN_WIDTH 128 // OLED display width, in pixels
#define SCREEN_HEIGHT 32 // OLED display height, in pixels

// Declaration for an SSD1306 display connected to I2C (SDA, SCL pins)
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);


void display_init() {

    pinMode(PIN_DISPLAY_EN, OUTPUT);
    digitalWrite(PIN_DISPLAY_EN, LOW);
    delay(1000);

    if(!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) { // Address 0x3D for 128x32
        Serial.println(F("SSD1306 allocation failed"));
        return;
    }
    display.clearDisplay();

    display.setTextSize(1);
    display.setTextColor(WHITE);
    display.setCursor(0, 10);
    
    char text[26];
    now = time(nullptr);
    wk.time_t_to_str(text, now, 0);

    // Display static text
    display.print(text);
    display.display(); 
}


void print_wakeup_reason(){
  
    esp_sleep_wakeup_cause_t wakeup_reason;

    wakeup_reason = esp_sleep_get_wakeup_cause();

    switch(wakeup_reason){
      
      case ESP_SLEEP_WAKEUP_EXT0: 
          Serial.println("Wakeup caused by external signal using RTC_IO"); 
          break;
      
      case ESP_SLEEP_WAKEUP_EXT1: 
          Serial.println("Wakeup caused by external signal using RTC_CNTL"); 
          break;
      
      case ESP_SLEEP_WAKEUP_TIMER: 
          Serial.println("Wakeup caused by timer"); 
          break;
      
      case ESP_SLEEP_WAKEUP_TOUCHPAD: 
          Serial.println("Wakeup caused by touchpad"); 
          break;
      
      case ESP_SLEEP_WAKEUP_ULP: 
          Serial.println("Wakeup caused by ULP program"); 
          break;
      
      default: 
          Serial.printf("Wakeup was not caused by deep sleep: %d\n",wakeup_reason); 
          break;
    }
    Serial.printf("\r");
}


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

  time_sync = 1;
  need_sync = 0;
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

    setenv("TZ", TZ, 3);
    tzset();

    //uint8_t cnt;
    //rtc.readRam(ADDR_CNT, &cnt);
    //Serial.printf("cnt: %d\n\r", cnt);
    //rtc.writeRam(ADDR_CNT, cnt + 1);

    rtc_time = rtc_get_time();

    Serial.print("rtc_time: ");
    Serial.println((unsigned int)rtc_time);

    need_sync = rtc_time < TIME_MIN || config;

    Serial.print("need_sync: ");
    Serial.println(need_sync);

    if (need_sync) {

        time_sync = false;

        Serial.println("Error on RTC, sync from the internet");

        now = time(nullptr);
        wk.print_time_t("localtime:", now, 0);
        Serial.println();

        WiFi.mode(WIFI_STA);
        WiFi.begin(STASSID, STAPSK);

        uint32_t timeout = millis() + 10000;
        while (WiFi.status() != WL_CONNECTED) {
          delay(500);
          Serial.print(".");
          if (millis() > timeout) {
            Serial.println("Connection timeout");
            break;
          }
        }

        Serial.println("");
        Serial.println("WiFi connected");
        Serial.println("IP address: ");
        Serial.println(WiFi.localIP());

        setenv("TZ", TZ, 3);
        tzset();

        sntp_setoperatingmode(SNTP_OPMODE_POLL);
        sntp_setservername(0, "pool.ntp.org");
        sntp_set_sync_mode(SNTP_SYNC_MODE_IMMED);
        sntp_set_time_sync_notification_cb(time_is_set);
        sntp_init();

        WiFiClient client;
        HTTPClient http;
        http.begin(client, "http://192.168.2.25:25000/getschedule");
        
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
        http.end();
    }
    else {
        time_sync = true;
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


size_t read_schedule(char * buffer, size_t buflen) {

    size_t res = 0;

    Serial.println("read schedule");

    LittleFS.begin(true);
    if (LittleFS.exists("/schedule.txt")) {

        File f = LittleFS.open("/schedule.txt", "r");
        res = f.readBytes(buffer, buflen);
        f.close();
    }
    LittleFS.end();

    return res;
}

void callback(){
  //placeholder callback function
}


void setup() {
    
    dir = 0;
    use_display = 0;
    need_sync = 0;
    wakeup_reason = esp_sleep_get_wakeup_cause();

    gpio_deep_sleep_hold_dis();
    
    pinMode(PIN_STEP_UP_EN, OUTPUT);
    pinMode(PIN_H_BRIDGE_EN_1, OUTPUT);
    pinMode(PIN_H_BRIDGE_EN_2, OUTPUT);
    pinMode(PIN_H_BRIDGE_EN_2, OUTPUT);
    pinMode(PIN_H_BRIDGE_AIN_1, OUTPUT);
    pinMode(PIN_H_BRIDGE_AIN_2, OUTPUT);
    pinMode(PIN_H_BRIDGE_BIN_1, OUTPUT);
    pinMode(PIN_H_BRIDGE_BIN_2, OUTPUT);

    pinMode(PIN_LED_ACT, OUTPUT);
    pinMode(PIN_TOUCH_1, INPUT);
    pinMode(PIN_DISPLAY_EN, OUTPUT);
    
    digitalWrite(PIN_STEP_UP_EN, LOW);
    digitalWrite(PIN_H_BRIDGE_EN_1, LOW);
    digitalWrite(PIN_H_BRIDGE_EN_2, LOW);
    digitalWrite(PIN_H_BRIDGE_EN_3, LOW);
    digitalWrite(PIN_DISPLAY_EN, HIGH);
    digitalWrite(PIN_STEP_UP_EN, LOW);

    rtc.init();
    soldrv.init(PIN_STEP_UP_EN, PIN_H_BRIDGE_EN_1, PIN_H_BRIDGE_AIN_1, PIN_H_BRIDGE_AIN_2);

    delay(BOOT_DELAY * 9e2);
    
    digitalWrite(PIN_LED_ACT, HIGH);
    delay(BOOT_DELAY * 1e2);
    digitalWrite(PIN_LED_ACT, LOW);
    
    if (wakeup_reason == ESP_SLEEP_WAKEUP_TOUCHPAD) {
        config = 1;
        use_display = 1;
    }
    else if (wakeup_reason == ESP_SLEEP_WAKEUP_UNDEFINED) {
        use_display = 1; 
    }
    else {
        config = digitalRead(PIN_TOUCH_1);
    }

    Serial.begin(115200);
    Serial.println("\n\nBOOT");
    print_wakeup_reason();

    Wire.begin();

    Serial.printf("config: %d\n\r", config);
    Serial.printf("use_display: %d\n\r", use_display);
    Serial.printf("STASSID: %s\n\r", STASSID);
    Serial.printf("STAPSK: %s\n\r", STAPSK);

    set_time_boot(config);
    
    if (use_display) {
        Serial.println("Display init\n");
        display_init();
        Serial.println("Display init end\n");
    }

    //time_t test = time(nullptr);
    //if (test < TIME_MIN) {
    //    Serial.println("ERROR, cannot sync\n");
    //    soldrv.close_valve();
    //}

    Serial.println("end setup\n");
    
}

void loop() {

    if (need_sync) {
        if (time_sync == 0) {
            return;
        }
    }

    soldrv.open_valve();
    soldrv.close_valve();

    now = time(nullptr);
    Serial.print("UTC time from system:  ");
    Serial.println((unsigned int)now);

    Serial.print("UTC time from rtc:     ");
    now = rtc_get_time();
    Serial.println((unsigned int)now);

    wk.print_time_t("localtime from system: ", now, 0);

    uint32_t sleeptime;
    uint8_t op;

    Serial.println("");

    sleeptime = 0;
    op = OP_NONE;
    buflen = SCHEDULE_MAXLEN;
    
    buflen = read_schedule((char *)buffer, buflen);

    //while(1) {
    //    
    //    Serial.println("Time to open");
    //    soldrv.open_valve();
    //    delay(5000);
    //    
    //    Serial.println("Time to close");
    //    soldrv.close_valve();
    //    delay(5000);
 
    //}
    //return;


    wk.init(now, buffer, buflen, EVT_TOLERANCE);
    
    if (use_display) {
    
        display.setTextSize(1);
        display.setTextColor(WHITE);
        display.setCursor(0, 20);
    
        char text[26];
        time_t next_evt_time = wk.get_next_event_time();
        wk.time_t_to_str(text, next_evt_time, 0);

        // Display static text
        display.print(text);
        display.display(); 
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
                soldrv.open_valve();
                break;

            case OP_CLOSE:
                Serial.println("Time to close");
                soldrv.close_valve();
                break;

            default:
                break;

        }
    }
    
    if (use_display) {
        while(millis() < MIN_DISPLAY * 1e3) 
        delay(100);
    }
    
    digitalWrite(PIN_STEP_UP_EN, LOW);
    digitalWrite(PIN_DISPLAY_EN, HIGH);
    digitalWrite(PIN_H_BRIDGE_EN_1, LOW);
    digitalWrite(PIN_H_BRIDGE_EN_2, LOW);
    digitalWrite(PIN_H_BRIDGE_EN_3, LOW);
    
    gpio_hold_en(GPIO_NUM_4);
    gpio_hold_en(GPIO_NUM_15);
    gpio_hold_en(GPIO_NUM_16);
    gpio_hold_en(GPIO_NUM_17);
    gpio_hold_en(GPIO_NUM_18);
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



    ESP.deepSleep(sleeptime);
}


