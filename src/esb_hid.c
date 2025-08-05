#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/uart.h>
#include <zephyr/logging/log.h>

#include <zmk/hid.h>
#include <zmk_feature_esb_transport/esb.h>
#include <zmk_feature_esb_transport/esb_hid.h>

LOG_MODULE_DECLARE(zmk, CONFIG_ZMK_LOG_LEVEL);

// UART device for HID data transmission
static const struct device *esb_uart_dev;

// HID packet header structure
struct hid_packet_header {
    uint8_t type;      // 1=keyboard, 2=consumer, 3=mouse
    uint8_t length;    // HID report data length
} __packed;

// HID packet types
#define HID_PACKET_TYPE_KEYBOARD 1
#define HID_PACKET_TYPE_CONSUMER 2
#define HID_PACKET_TYPE_MOUSE    3

// UART transmission utility for HID data
static int uart_esb_send(const uint8_t *data, size_t len) {
    if (!esb_uart_dev || !device_is_ready(esb_uart_dev)) {
        return -ENODEV;
    }
    
    // Simple protocol: send data directly to UART for BLESB to forward via ESB
    for (size_t i = 0; i < len; i++) {
        uart_poll_out(esb_uart_dev, data[i]);
    }
    
    return 0;
}

// Send HID report with header
static int zmk_esb_hid_send_report(uint8_t type, const uint8_t *report, size_t len) {
    if (!zmk_esb_active_profile_is_connected()) {
        return -ENOTCONN;
    }
    
    // Create packet with header
    struct hid_packet_header header = {
        .type = type,
        .length = (uint8_t)len
    };
    
    // Send header + data
    int err = uart_esb_send((uint8_t *)&header, sizeof(header));
    if (err) {
        return err;
    }
    
    return uart_esb_send(report, len);
}

// Public HID transmission functions
int zmk_esb_hid_send_keyboard_report(void) {
    struct zmk_hid_keyboard_report *report = zmk_hid_get_keyboard_report();
    return zmk_esb_hid_send_report(HID_PACKET_TYPE_KEYBOARD, 
                                   (uint8_t *)&report->body, 
                                   sizeof(report->body));
}

int zmk_esb_hid_send_consumer_report(void) {
    struct zmk_hid_consumer_report *report = zmk_hid_get_consumer_report();
    return zmk_esb_hid_send_report(HID_PACKET_TYPE_CONSUMER,
                                   (uint8_t *)report,
                                   sizeof(*report));
}

#if IS_ENABLED(CONFIG_ZMK_POINTING)
int zmk_esb_hid_send_mouse_report(void) {
    struct zmk_hid_mouse_report *report = zmk_hid_get_mouse_report();
    return zmk_esb_hid_send_report(HID_PACKET_TYPE_MOUSE,
                                   (uint8_t *)report,
                                   sizeof(*report));
}
#endif

// Check if ESB HID is ready for transmission
bool zmk_esb_hid_is_ready(void) {
    return zmk_esb_active_profile_is_connected();
}

// Initialize ESB HID transport
static int esb_hid_init(void) {
    esb_uart_dev = DEVICE_DT_GET(DT_CHOSEN(zmk_esb_uart));
    
    if (!device_is_ready(esb_uart_dev)) {
        LOG_ERR("ESB UART device not ready for HID transmission");
        return -ENODEV;
    }
    
    LOG_INF("ESB HID transport initialized");
    return 0;
}

SYS_INIT(esb_hid_init, POST_KERNEL, CONFIG_ZMK_ESB_HID_INIT_PRIORITY);