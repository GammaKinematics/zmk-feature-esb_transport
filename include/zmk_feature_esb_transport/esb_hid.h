#pragma once

#include <stdint.h>

#if IS_ENABLED(CONFIG_ZMK_ESB)

/**
 * Send keyboard HID report via ESB transport
 * @return 0 on success, negative error code on failure
 */
int zmk_esb_hid_send_keyboard_report(void);

/**
 * Send consumer HID report via ESB transport
 * @return 0 on success, negative error code on failure
 */
int zmk_esb_hid_send_consumer_report(void);

#if IS_ENABLED(CONFIG_ZMK_POINTING)
/**
 * Send mouse HID report via ESB transport
 * @return 0 on success, negative error code on failure
 */
int zmk_esb_hid_send_mouse_report(void);
#endif // IS_ENABLED(CONFIG_ZMK_POINTING)

/**
 * Check if ESB transport is ready to send reports
 * @return true if ready, false otherwise
 */
bool zmk_esb_hid_is_ready(void);

/**
 * Check if ESB profile is connected to DONGLE
 * @return true if connected, false otherwise  
 */
bool zmk_esb_active_profile_is_connected(void);

#else

// Stub implementations when ESB disabled
static inline int zmk_esb_hid_send_keyboard_report(void) { return -ENOTSUP; }
static inline int zmk_esb_hid_send_consumer_report(void) { return -ENOTSUP; }
#if IS_ENABLED(CONFIG_ZMK_POINTING)
static inline int zmk_esb_hid_send_mouse_report(void) { return -ENOTSUP; }
#endif
static inline bool zmk_esb_hid_is_ready(void) { return false; }
static inline bool zmk_esb_active_profile_is_connected(void) { return false; }

#endif /* CONFIG_ZMK_ESB */