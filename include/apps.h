#include <sys/cdefs.h>

#ifndef _APPS_H
#define _APPS_H

#ifdef __cplusplus
#define APPS_NORETURN [[noreturn]]
extern "C" {
#else
#define APPS_NORETURN _Noreturn
#endif

APPS_NORETURN void app_data_client(void* pvParameters);

APPS_NORETURN void app_monitor(void* pvParameters);

APPS_NORETURN void app_system_loop(void* pvParameters);

#ifdef __cplusplus
}
#endif

#endif
