#ifndef MODULE_FREERTOS_H_
#define MODULE_FREERTOS_H_

#ifdef __cplusplus
extern "C" {
#endif

/* Creates the application tasks and starts the FreeRTOS scheduler.
 * This function only returns when task creation or scheduler startup fails. */
void ModuleFreeRtos_Start(void);

#ifdef __cplusplus
}
#endif

#endif /* MODULE_FREERTOS_H_ */
