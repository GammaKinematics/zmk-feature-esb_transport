#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/uart.h>
#include <zephyr/logging/log.h>

#include <zmk/hid.h>
#include <zmk_feature_esb_transport/esb_hid.h>
#include <zmk_feature_esb_transport/events/esb_conn_state_changed.h>

#if IS_ENABLED(CONFIG_ZMK_ESB)

LOG_MODULE_DECLARE(zmk, CONFIG_ZMK_LOG_LEVEL);

// ESB packet format - matches DONGLE-ESB exactly
struct hid_packet_header {
    uint8_t type;      // HID report type: 1=keyboard, 2=consumer, 3=mouse
    uint8_t length;    // Length of HID report data following this header
} __packed;

// HID packet types - must match DONGLE-ESB definitions
#define HID_PACKET_TYPE_KEYBOARD  1   // Standard keyboard report (8 bytes)
#define HID_PACKET_TYPE_CONSUMER  2   // Media/consumer report (2 bytes)  
#define HID_PACKET_TYPE_MOUSE     3   // Mouse report (6 bytes)

// ESB constraints
#define ESB_MAX_PAYLOAD_SIZE      32
#define MAX_HID_REPORT_SIZE       (ESB_MAX_PAYLOAD_SIZE - sizeof(struct hid_packet_header))

// UART device and state
static const struct device *esb_uart_dev;
static bool esb_transport_ready = false;

// ESB connection state tracking (like BLE)
static bool esb_connection_active = false;

// UART transmission buffer
static uint8_t tx_buffer[ESB_MAX_PAYLOAD_SIZE];
static K_SEM_DEFINE(uart_tx_sem, 1, 1);

/**
 * Update ESB connection state and raise events
 */
static void update_esb_connection_state(bool connected) {
    if (esb_connection_active != connected) {
        esb_connection_active = connected;
        LOG_INF("ESB connection state changed: %s", connected ? "connected" : "disconnected");
        
        // Raise event to notify endpoint system (like BLE does)
        raise_zmk_esb_conn_state_changed(
            (struct zmk_esb_conn_state_changed){.connected = connected}
        );
    }
}

/**
 * Mode detection functions (placeholders for now)
 * TODO: Implement actual BLESB mode detection via RTS/CTS pins
 */
static bool is_blesb_connected(void) {
    // TODO: Check GPIO pin that indicates BLESB presence
    // For now, assume connected if ESB transport is enabled
    return IS_ENABLED(CONFIG_ZMK_ESB);
}

static bool is_blesb_in_esb_mode(void) {
    // TODO: Read RTS/CTS pins to detect BLESB mode
    // BLESB signals: ESB mode = RTS low, CTS high
    //               BLE mode = RTS high, CTS low
    // For now, return true if ESB enabled
    return IS_ENABLED(CONFIG_ZMK_ESB);
}

/**
 * Send framed HID packet via UART to BLESB
 * Packet format: [type:1][length:1][HID_data:variable]
 */
static int zmk_esb_hid_send_packet(uint8_t packet_type, const uint8_t *hid_data, size_t hid_len) {
    if (!zmk_esb_hid_is_ready()) {
        LOG_DBG("ESB transport not ready");
        return -ENODEV;
    }

    if (hid_len > MAX_HID_REPORT_SIZE) {
        LOG_ERR("HID report too large: %zu bytes (max %d)", hid_len, MAX_HID_REPORT_SIZE);
        return -EINVAL;
    }

    // Build framed packet
    struct hid_packet_header header = {
        .type = packet_type,
        .length = (uint8_t)hid_len
    };

    size_t total_len = sizeof(header) + hid_len;
    
    // Copy header and data to transmission buffer
    memcpy(tx_buffer, &header, sizeof(header));
    memcpy(tx_buffer + sizeof(header), hid_data, hid_len);

    // Send packet via UART (blocking with timeout)
    k_sem_take(&uart_tx_sem, K_MSEC(10));
    
    for (size_t i = 0; i < total_len; i++) {
        uart_poll_out(esb_uart_dev, tx_buffer[i]);
    }
    
    k_sem_give(&uart_tx_sem);

    LOG_DBG("ESB packet sent: type=%d, len=%zu, total=%zu", 
            packet_type, hid_len, total_len);
    
    // TODO: Implement actual connection monitoring
    // For now, assume successful transmission = connected
    // In real implementation:
    // - Monitor ESB ACK/NACK from BLESB
    // - Track transmission success/failure rates  
    // - Implement timeout-based connection detection
    if (!esb_connection_active) {
        update_esb_connection_state(true);
    }
    
    return 0;
}

/**
 * Monitor ESB connection state (placeholder implementation)
 * TODO: Implement proper connection detection:
 * - ESB ACK/NACK monitoring via BLESB feedback
 * - Heartbeat packets to DONGLE
 * - Timeout-based disconnection detection
 */
static void monitor_esb_connection(void) {
    // Method 1: Track ESB transmission acknowledgments
    // Method 2: Send periodic heartbeat packets  
    // Method 3: Timeout after failed transmissions
    
    // For now, connection state is updated in send_packet
    // Real implementation would monitor BLESB ESB status
}

/**
 * Send keyboard HID report via ESB transport
 * Uses same HID report format as USB transport
 */
int zmk_esb_hid_send_keyboard_report(void) {
    struct zmk_hid_keyboard_report *report = zmk_hid_get_keyboard_report();
    
    if (!report) {
        LOG_ERR("Failed to get keyboard report");
        return -EINVAL;
    }

    return zmk_esb_hid_send_packet(HID_PACKET_TYPE_KEYBOARD, 
                                   (uint8_t *)&report->body, sizeof(report->body));
}

/**
 * Send consumer HID report via ESB transport
 * Uses same HID report format as USB transport
 */
int zmk_esb_hid_send_consumer_report(void) {
    struct zmk_hid_consumer_report *report = zmk_hid_get_consumer_report();
    
    if (!report) {
        LOG_ERR("Failed to get consumer report");
        return -EINVAL;
    }

    return zmk_esb_hid_send_packet(HID_PACKET_TYPE_CONSUMER, 
                                   (uint8_t *)report, sizeof(*report));
}

#if IS_ENABLED(CONFIG_ZMK_POINTING)
/**
 * Send mouse HID report via ESB transport
 * Uses same HID report format as USB transport
 */
int zmk_esb_hid_send_mouse_report(void) {
    struct zmk_hid_mouse_report *report = zmk_hid_get_mouse_report();
    
    if (!report) {
        LOG_ERR("Failed to get mouse report");
        return -EINVAL;
    }

    return zmk_esb_hid_send_packet(HID_PACKET_TYPE_MOUSE, 
                                   (uint8_t *)report, sizeof(*report));
}
#endif // IS_ENABLED(CONFIG_ZMK_POINTING)

/**
 * Check if ESB transport is ready to send reports
 * This checks if hardware/software is ready for ESB communication
 */
bool zmk_esb_hid_is_ready(void) {
    return esb_transport_ready && 
           is_blesb_connected() && 
           is_blesb_in_esb_mode();
}

/**
 * Check if ESB profile is connected to DONGLE  
 * This checks if there's an active connection to receive reports
 * (Similar to zmk_ble_active_profile_is_connected)
 */
bool zmk_esb_active_profile_is_connected(void) {
    return esb_connection_active && zmk_esb_hid_is_ready();
}

/**
 * Initialize ESB HID transport
 * Gets UART device from device tree and sets up communication
 */
static int zmk_esb_hid_init(void) {
    LOG_INF("Initializing ESB HID transport");

    // Get UART device from device tree
    // Uses same UART as HCI (shared between BLE and ESB modes)
    esb_uart_dev = DEVICE_DT_GET(DT_CHOSEN(zephyr_bt_uart));
    
    if (!device_is_ready(esb_uart_dev)) {
        LOG_ERR("ESB UART device not ready");
        return -ENODEV;
    }

    // TODO: Configure GPIO pins for BLESB mode detection
    // - RTS/CTS pins for detecting ESB vs BLE mode
    // - Connection detection pin for BLESB presence
    // - Set up GPIO interrupts for mode change detection
    
    // TODO: Implement connection monitoring
    // - Start periodic connection health checks
    // - Set up ESB feedback monitoring from BLESB
    // - Initialize connection state machine
    
    esb_transport_ready = true;
    LOG_INF("ESB HID transport initialized successfully");
    
    // Start with disconnected state - connection established on first successful transmission
    update_esb_connection_state(false);
    
    return 0;
}

// Initialize ESB transport after kernel is ready
SYS_INIT(zmk_esb_hid_init, APPLICATION, CONFIG_ZMK_ESB_INIT_PRIORITY);

#endif /* CONFIG_ZMK_ESB */