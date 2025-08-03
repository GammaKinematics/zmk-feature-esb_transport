# Core ZMK Changes Required for ESB Transport

This document outlines the minimal core ZMK modifications needed to integrate the ESB transport module.

## Critical: Conditional Compilation

⚠️ **Important**: All ESB functionality must be conditionally compiled because the ESB module is external to core ZMK.

**Pattern**: Wrap ALL ESB code in `#if IS_ENABLED(CONFIG_ZMK_ESB)` guards.

## Files to Modify

### 1. `app/include/zmk/endpoints_types.h`

**Add ESB transport enum:**
```c
enum zmk_transport {
    ZMK_TRANSPORT_USB,
    ZMK_TRANSPORT_BLE,
    ZMK_TRANSPORT_ESB,    // ← ADD THIS LINE
};
```

**Add ESB transport data structure:**
```c
/**
 * Configuration to select an endpoint on ZMK_TRANSPORT_ESB.
 */
struct zmk_transport_esb_data {};    // ← ADD THIS STRUCT
```

**Add ESB to endpoint instance union:**
```c
struct zmk_endpoint_instance {
    enum zmk_transport transport;
    union {
        struct zmk_transport_usb_data usb; // ZMK_TRANSPORT_USB
        struct zmk_transport_ble_data ble; // ZMK_TRANSPORT_BLE
        struct zmk_transport_esb_data esb; // ZMK_TRANSPORT_ESB  ← ADD THIS LINE
    };
};
```

### 2. `app/include/zmk/endpoints.h`

**Add ESB endpoint count:**
```c
#ifdef CONFIG_ZMK_ESB
#define ZMK_ENDPOINT_ESB_COUNT 1
#else
#define ZMK_ENDPOINT_ESB_COUNT 0
#endif
```

**Update total endpoint count:**
```c
#define ZMK_ENDPOINT_COUNT (ZMK_ENDPOINT_USB_COUNT + ZMK_ENDPOINT_BLE_COUNT + ZMK_ENDPOINT_ESB_COUNT)
```

### 3. `app/src/endpoints.c`

**Add conditional ESB include:**
```c
#include <zmk/ble.h>
#include <zmk/endpoints.h>
#include <zmk/hid.h>
#include <dt-bindings/zmk/hid_usage_pages.h>
#include <zmk/usb_hid.h>
#include <zmk/hog.h>
#if IS_ENABLED(CONFIG_ZMK_ESB)
#include <zmk/esb_hid.h>    // ← ADD THIS CONDITIONAL INCLUDE
#endif
#include <zmk/event_manager.h>
#include <zmk/events/ble_active_profile_changed.h>
#include <zmk/events/usb_conn_state_changed.h>
#include <zmk/events/endpoint_changed.h>
```

**Update DEFAULT_TRANSPORT priority:**
```c
#define DEFAULT_TRANSPORT                                                                          \
    COND_CODE_1(IS_ENABLED(CONFIG_ZMK_ESB), (ZMK_TRANSPORT_ESB),                                 \
    COND_CODE_1(IS_ENABLED(CONFIG_ZMK_BLE), (ZMK_TRANSPORT_BLE), (ZMK_TRANSPORT_USB)))
```

**Add ESB ready function:**
```c
static bool is_esb_ready(void) {
#if IS_ENABLED(CONFIG_ZMK_ESB)
    return zmk_esb_active_profile_is_connected();
#else
    return false;
#endif
}
```

**Update instance equality function:**
```c
bool zmk_endpoint_instance_eq(struct zmk_endpoint_instance a, struct zmk_endpoint_instance b) {
    if (a.transport != b.transport) {
        return false;
    }

    switch (a.transport) {
    case ZMK_TRANSPORT_USB:
        return true;
    case ZMK_TRANSPORT_BLE:
        return a.ble.profile_index == b.ble.profile_index;
    case ZMK_TRANSPORT_ESB:    // ← ADD THIS CASE
        return true;
    }

    LOG_ERR("Invalid transport %d", a.transport);
    return false;
}
```

**Update string conversion function:**
```c
int zmk_endpoint_instance_to_str(struct zmk_endpoint_instance endpoint, char *str, size_t len) {
    switch (endpoint.transport) {
    case ZMK_TRANSPORT_USB:
        return snprintf(str, len, "USB");
    case ZMK_TRANSPORT_BLE:
        return snprintf(str, len, "BLE:%d", endpoint.ble.profile_index);
    case ZMK_TRANSPORT_ESB:    // ← ADD THIS CASE
        return snprintf(str, len, "ESB");
    default:
        return snprintf(str, len, "Invalid");
    }
}
```

**Add ESB index offset and update index function:**
```c
#define INSTANCE_INDEX_OFFSET_USB 0
#define INSTANCE_INDEX_OFFSET_BLE ZMK_ENDPOINT_USB_COUNT
#define INSTANCE_INDEX_OFFSET_ESB (ZMK_ENDPOINT_USB_COUNT + ZMK_ENDPOINT_BLE_COUNT)  // ← ADD THIS

int zmk_endpoint_instance_to_index(struct zmk_endpoint_instance endpoint) {
    switch (endpoint.transport) {
    case ZMK_TRANSPORT_USB:
        return INSTANCE_INDEX_OFFSET_USB;
    case ZMK_TRANSPORT_BLE:
        return INSTANCE_INDEX_OFFSET_BLE + endpoint.ble.profile_index;
    case ZMK_TRANSPORT_ESB:    // ← ADD THIS CASE
        return INSTANCE_INDEX_OFFSET_ESB;
    }

    LOG_ERR("Invalid transport %d", endpoint.transport);
    return 0;
}
```

**Update transport selection logic (with mutual exclusivity):**
```c
static enum zmk_transport get_selected_transport(void) {
    bool usb_ready = is_usb_ready();
    bool ble_ready = is_ble_ready();
    bool esb_ready = is_esb_ready();
    
    // ESB and BLE are mutually exclusive - only one can be ready
    // Hardware SPDT switch on BLESB determines which wireless mode is available
    
    if (usb_ready) {
        if (ble_ready || esb_ready) {
            // Both USB and wireless available - use preference
            LOG_DBG("USB and wireless ready. Using preferred: %d", preferred_transport);
            return preferred_transport;
        }
        LOG_DBG("Only USB is ready.");
        return ZMK_TRANSPORT_USB;
    }
    
    // No USB, check wireless options (mutually exclusive)
    if (esb_ready) {
        LOG_DBG("ESB wireless ready.");
        return ZMK_TRANSPORT_ESB;
    }
    
    if (ble_ready) {
        LOG_DBG("BLE wireless ready.");
        return ZMK_TRANSPORT_BLE;
    }

    LOG_DBG("No transports ready.");
    return DEFAULT_TRANSPORT;
}
```

**Update transport toggle logic:**
```c
int zmk_endpoints_toggle_transport(void) {
    // Simple USB <-> wireless toggle
    // Wireless type determined by hardware (BLESB SPDT switch)
    enum zmk_transport new_transport;
    
    if (preferred_transport == ZMK_TRANSPORT_USB) {
        // Switch to whatever wireless is available
        new_transport = is_esb_ready() ? ZMK_TRANSPORT_ESB : ZMK_TRANSPORT_BLE;
    } else {
        // Any wireless -> USB
        new_transport = ZMK_TRANSPORT_USB;
    }
    
    return zmk_endpoints_select_transport(new_transport);
}
```

**Add ESB cases to report sending functions:**

In `send_keyboard_report()`:
```c
case ZMK_TRANSPORT_ESB: {
#if IS_ENABLED(CONFIG_ZMK_ESB)
    int err = zmk_esb_hid_send_keyboard_report();
    if (err) {
        LOG_ERR("FAILED TO SEND OVER ESB: %d", err);
    }
    return err;
#else
    LOG_ERR("ESB endpoint is not supported");
    return -ENOTSUP;
#endif
}
```

In `send_consumer_report()`:
```c
case ZMK_TRANSPORT_ESB: {
#if IS_ENABLED(CONFIG_ZMK_ESB)
    int err = zmk_esb_hid_send_consumer_report();
    if (err) {
        LOG_ERR("FAILED TO SEND OVER ESB: %d", err);
    }
    return err;
#else
    LOG_ERR("ESB endpoint is not supported");
    return -ENOTSUP;
#endif
}
```

In `send_mouse_report()` (if CONFIG_ZMK_POINTING enabled):
```c
case ZMK_TRANSPORT_ESB: {
#if IS_ENABLED(CONFIG_ZMK_ESB)
    int err = zmk_esb_hid_send_mouse_report();
    if (err) {
        LOG_ERR("FAILED TO SEND OVER ESB: %d", err);
    }
    return err;
#else
    LOG_ERR("ESB endpoint is not supported");
    return -ENOTSUP;
#endif
}
```

**Add ESB event subscription:**
```c
ZMK_LISTENER(endpoint_listener, endpoint_listener);
#if IS_ENABLED(CONFIG_ZMK_USB)
ZMK_SUBSCRIPTION(endpoint_listener, zmk_usb_conn_state_changed);
#endif
#if IS_ENABLED(CONFIG_ZMK_BLE)
ZMK_SUBSCRIPTION(endpoint_listener, zmk_ble_active_profile_changed);
#endif
#if IS_ENABLED(CONFIG_ZMK_ESB)
ZMK_SUBSCRIPTION(endpoint_listener, zmk_esb_conn_state_changed);    // ← ADD THIS
#endif
```

### 4. Event Subscription Only

Core ZMK doesn't need any new event files - they're in your module! Core ZMK just subscribes to the events your module raises.

**Add ESB event subscription:**
```c
ZMK_LISTENER(endpoint_listener, endpoint_listener);
#if IS_ENABLED(CONFIG_ZMK_USB)
ZMK_SUBSCRIPTION(endpoint_listener, zmk_usb_conn_state_changed);
#endif
#if IS_ENABLED(CONFIG_ZMK_BLE)
ZMK_SUBSCRIPTION(endpoint_listener, zmk_ble_active_profile_changed);
#endif
#if IS_ENABLED(CONFIG_ZMK_ESB)
ZMK_SUBSCRIPTION(endpoint_listener, zmk_esb_conn_state_changed);    // ← ADD THIS
#endif
```

## Summary

**Total Changes:**
- **3 files modified**: endpoints_types.h, endpoints.h, endpoints.c
- **0 files created**: All event files are in your ESB module
- **0 CMakeLists changes**: Module handles its own build
- **~45 lines added** across 3 files only

**Architecture Impact:**
- ESB becomes first-class transport alongside USB/BLE
- Mutual exclusivity properly handled (ESB ↔ BLE hardware-determined)
- Event-driven connection state management
- Performance-optimized transport priority (ESB > BLE > USB)

**Integration:**
- Clean, minimal changes following existing ZMK patterns
- No breaking changes to existing USB/BLE functionality
- Module-based ESB implementation keeps core changes minimal