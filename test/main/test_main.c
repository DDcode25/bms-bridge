#include <stdio.h>
#include <stdlib.h>
#include "unity.h"

void run_pylon_frame_tests(void);
void run_pylon_cmd_tests(void);
void run_limits_tests(void);
void run_pack_tests(void);
void run_bmz_can_tests(void);

void setUp(void) {}
void tearDown(void) {}

void app_main(void)
{
    UNITY_BEGIN();
    run_pylon_frame_tests();
    run_pylon_cmd_tests();
    run_limits_tests();
    run_pack_tests();
    run_bmz_can_tests();
    int failures = UNITY_END();
    fflush(stdout);
    exit(failures ? 1 : 0);
}

#ifndef ESP_PLATFORM
/* Сборка обычным gcc (test/host_test.sh), без ESP-IDF. */
int main(void)
{
    app_main();
    return 0;
}
#endif
