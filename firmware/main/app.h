/**
 * @file  app.h
 * @brief Application: wires drivers to the logic modules.
 */

#ifndef APP_H
#define APP_H

/**
 * Initialise all drivers in boot order and start the tasks.
 * See docs/firmware/architecture.md, "Boot sequence".
 */
void app_start(void);

#endif /* APP_H */
