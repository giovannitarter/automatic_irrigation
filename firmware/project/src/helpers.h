#ifndef __AUTO_IRR_HELPERS__
#define __AUTO_IRR_HELPERS__


void print_wakeup_reason(esp_sleep_wakeup_cause_t wakeup_reason);
size_t read_schedule(char * buffer, size_t buflen);


#endif
