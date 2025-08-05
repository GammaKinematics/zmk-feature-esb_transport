#pragma once

#include <stdbool.h>

/**
 * @brief Send keyboard HID report via ESB transport
 * 
 * @return 0 on success, negative error code on failure
 */
int zmk_esb_hid_send_keyboard_report(void);

/**
 * @brief Send consumer HID report via ESB transport
 * 
 * @return 0 on success, negative error code on failure  
 */
int zmk_esb_hid_send_consumer_report(void);

#if IS_ENABLED(CONFIG_ZMK_POINTING)
/**
 * @brief Send mouse HID report via ESB transport
 * 
 * @return 0 on success, negative error code on failure
 */
int zmk_esb_hid_send_mouse_report(void);
#endif

/**
 * @brief Check if ESB HID transport is ready for transmission
 * 
 * @return true if ESB HID is ready, false otherwise
 */
bool zmk_esb_hid_is_ready(void);