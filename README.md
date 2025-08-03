# ZMK ESB Transport Module

This module adds Enhanced ShockBurst (ESB) transport capability to ZMK, enabling low-latency wireless communication for gaming keyboards.

## Architecture

```
ZMK (PRIM) → UART → BLESB → ESB → DONGLE → USB → Computer
```

ESB and BLE are **mutually exclusive** - BLESB hardware SPDT switch determines which wireless mode is active.

## Features

- **ESB Transport**: Third transport option alongside USB and BLE
- **Mutual Exclusivity**: ESB and BLE cannot be active simultaneously
- **Connection State Tracking**: Like BLE, tracks connection to DONGLE
- **Event-Driven**: Raises connection state events for endpoint system
- **HID Report Forwarding**: Sends standard ZMK HID reports via UART to BLESB
- **Low Latency**: Sub-millisecond ESB transmission for gaming performance
- **Shared UART**: Uses same UART hardware as HCI-over-UART (mode switching)
- **Hardware Mode Detection**: Detects BLESB presence and mode via RTS/CTS pins

## Transport Priority

**Default Transport Priority** (when multiple available):
1. **ESB** - High-performance wireless (if enabled)
2. **BLE** - Standard wireless (if enabled, ESB not available)  
3. **USB** - Wired connection (fallback)

**Runtime Selection:**
- USB ↔ Wireless toggle (wireless type determined by BLESB hardware)
- Only one wireless type available at a time (ESB OR BLE, never both)

## Protocol

The module frames ZMK HID reports with a 2-byte header for transmission to BLESB:

```c
struct hid_packet_header {
    uint8_t type;      // 1=keyboard, 2=consumer, 3=mouse
    uint8_t length;    // Length of HID report data
} __packed;
```

**Packet Format**: `[type:1][length:1][HID_data:variable]`

**Total Size**: Header (2 bytes) + HID data ≤ 32 bytes (ESB constraint)

## Dependencies and Build Integration

### Module Dependencies
This module is **external to core ZMK** and must be included via `west.yml`:

```yaml
# config/west.yml
manifest:
  projects:
    - name: zmk-feature-esb-transport
      remote: your-remote
      revision: main
      path: modules/zmk-feature-esb-transport
```

### Conditional Compilation
All references to ESB functionality in core ZMK must be conditional:

```c
// Correct - conditional include
#if IS_ENABLED(CONFIG_ZMK_ESB)
#include <zmk/esb_hid.h>
#endif

// Correct - conditional usage
#if IS_ENABLED(CONFIG_ZMK_ESB)
    return zmk_esb_active_profile_is_connected();
#else
    return false;
#endif
```

**Why?** The ESB headers exist only when the module is included. Without conditional compilation, builds would fail when the module is not present.

## Configuration

### Enable ESB Transport

```kconfig
CONFIG_ZMK_ESB=y
CONFIG_UART_INTERRUPT_DRIVEN=y
```

### Device Tree

Uses the same UART as HCI transport:

```dts
chosen {
    zephyr,bt-uart = &lpuart3;  // ESB shares this UART
};
```

### Mode Detection (TODO)

BLESB signals its mode via UART flow control pins:
- **ESB Mode**: RTS=low, CTS=high  
- **BLE Mode**: RTS=high, CTS=low

## Usage

The module automatically integrates with ZMK's endpoint system. When BLESB is detected in ESB mode, keyboard reports are forwarded via ESB transport.

### API Functions

```c
// Send reports (same interface as usb_hid.h)
int zmk_esb_hid_send_keyboard_report(void);
int zmk_esb_hid_send_consumer_report(void);
int zmk_esb_hid_send_mouse_report(void);

// Transport readiness and connection state (like BLE pattern)
bool zmk_esb_hid_is_ready(void);                    // Hardware/software ready
bool zmk_esb_active_profile_is_connected(void);     // Active connection to DONGLE
```

## Connection State Management

ESB follows the same pattern as BLE for connection state:

**Connection Events:**
- `zmk_esb_conn_state_changed` - Raised when DONGLE connection state changes
- Integrated with ZMK endpoint system via event subscriptions

**Connection Detection:**
- Tracks successful ESB transmissions via BLESB
- Timeout-based disconnection detection
- Future: ESB ACK/NACK monitoring for real-time connection status

## Integration

This module integrates with ZMK core via the endpoints system. Core ZMK modifications needed:

### 1. Transport Type Addition
```c
// app/include/zmk/endpoints_types.h
enum zmk_transport {
    ZMK_TRANSPORT_USB,
    ZMK_TRANSPORT_BLE,
    ZMK_TRANSPORT_ESB,    // Add this
};
```

### 2. Endpoint Count Update
```c
// app/include/zmk/endpoints.h  
#ifdef CONFIG_ZMK_ESB
#define ZMK_ENDPOINT_ESB_COUNT 1
#else
#define ZMK_ENDPOINT_ESB_COUNT 0
#endif

#define ZMK_ENDPOINT_COUNT (ZMK_ENDPOINT_USB_COUNT + ZMK_ENDPOINT_BLE_COUNT + ZMK_ENDPOINT_ESB_COUNT)
```

### 3. Endpoints Logic Updates
```c
// app/src/endpoints.c
#include <zmk/esb_hid.h>

// Add ESB cases to:
// - zmk_endpoint_instance_eq()
// - zmk_endpoint_instance_to_str()  
// - zmk_endpoint_instance_to_index()
// - send_keyboard_report()
// - send_consumer_report()
// - send_mouse_report()
// - get_selected_transport() - with mutual exclusivity logic
// - is_esb_ready() function
```

### 4. Event Subscription
```c
// app/src/endpoints.c
#if IS_ENABLED(CONFIG_ZMK_ESB)
ZMK_SUBSCRIPTION(endpoint_listener, zmk_esb_conn_state_changed);
#endif
```

### 5. Default Transport Priority
```c
// app/src/endpoints.c
#define DEFAULT_TRANSPORT                                                                          \
    COND_CODE_1(IS_ENABLED(CONFIG_ZMK_ESB), (ZMK_TRANSPORT_ESB),                                 \
    COND_CODE_1(IS_ENABLED(CONFIG_ZMK_BLE), (ZMK_TRANSPORT_BLE), (ZMK_TRANSPORT_USB)))
```

### 6. Event System Integration

The module provides event files - core ZMK just subscribes:

```c
// app/src/endpoints.c - Core ZMK only subscribes to module events
#if IS_ENABLED(CONFIG_ZMK_ESB)
ZMK_SUBSCRIPTION(endpoint_listener, zmk_esb_conn_state_changed);
#endif
```

**Note**: Event files (`esb_conn_state_changed.h/c`) are **in the module**, not core ZMK.

## Hardware Requirements

- **PRIM MCU**: STM32U073CC with UART pins for BLESB communication
- **BLESB**: nRF52805 with ESB firmware for packet forwarding
- **DONGLE**: nRF52840 with ESB receiver and USB HID capability

## Status

- ✅ **Module Implementation**: Complete ESB HID transport with connection state tracking
- ✅ **Protocol Definition**: Matches DONGLE-ESB packet format perfectly
- ✅ **HID Integration**: Uses standard ZMK HID report functions
- ✅ **Event System**: ESB connection state events for endpoint integration
- ✅ **Mutual Exclusivity**: Proper handling of ESB/BLE exclusivity
- ✅ **Transport Priority**: ESB > BLE > USB default preference
- 🔄 **Mode Detection**: Placeholder implementation (TODO: GPIO RTS/CTS)
- 🔄 **Connection Monitoring**: Basic implementation (TODO: ESB ACK/NACK feedback)
- 🔄 **Core Integration**: Requires ZMK fork modifications (~45 lines, 3 files)

## Architecture Benefits

### Mutual Exclusivity Design
- **Clean separation**: ESB and BLE cannot conflict
- **Hardware-determined**: BLESB SPDT switch controls wireless mode
- **Simplified logic**: Only one wireless transport active at a time
- **Consistent behavior**: Same UART, different protocols

### Connection State Tracking  
- **BLE-like pattern**: Familiar connection state management
- **Event-driven**: Proper integration with ZMK endpoint system
- **Responsive switching**: Automatic endpoint updates on connection changes
- **Performance optimized**: ESB preferred when available

## Next Steps

1. **Core ZMK Integration**: Add `ZMK_TRANSPORT_ESB` to endpoints system
2. **Mode Detection**: Implement actual BLESB mode detection via GPIO
3. **Testing**: Validate end-to-end ESB transport chain
4. **Optimization**: Performance tuning for sub-millisecond latency

## Dependencies

- **ZMK Core**: Fork with ESB transport enum in endpoints_types.h
- **BLESB Firmware**: ESB mode with matching packet format
- **DONGLE Firmware**: ESB receiver with USB HID forwarding
- **Hardware**: UART connections between PRIM and BLESB