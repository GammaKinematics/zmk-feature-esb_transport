#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/uart.h>
#include <zephyr/sys/reboot.h>
#include <zephyr/logging/log.h>

#include <zmk/event_manager.h>
#include <zmk_feature_esb_transport/esb.h>
#include <zmk_feature_esb_transport/events/esb_conn_state_changed.h>

LOG_MODULE_DECLARE(zmk, CONFIG_ZMK_LOG_LEVEL);

// Connection state management
static bool esb_connected = false;
static const struct device *esb_uart_dev;

// Update ESB connection state and raise events
static void update_esb_connection_state(bool connected) {
    if (esb_connected != connected) {
        esb_connected = connected;
        
        // Raise the event
        raise_zmk_esb_conn_state_changed((struct zmk_esb_conn_state_changed){
            .connected = connected
        });
        
        LOG_INF("ESB connection: %s", connected ? "ready" : "not ready");
    }
}

// Public API for esb_hid.c and core ZMK
bool zmk_esb_active_profile_is_connected(void) {
    return esb_connected;
}

// UART utility
static void uart_send_string(const char *str) {
    if (!esb_uart_dev || !device_is_ready(esb_uart_dev)) {
        return;
    }
    
    size_t len = strlen(str);
    for (size_t i = 0; i < len; i++) {
        uart_poll_out(esb_uart_dev, str[i]);
    }
}

// UART RX callback - handles all protocol message processing
static void uart_rx_callback(const struct device *dev, void *user_data) {
    static char rx_buffer[32];
    static int rx_pos = 0;
    
    uint8_t c;
    while (uart_fifo_read(dev, &c, 1) == 1) {
        if (c == '\n') {
            rx_buffer[rx_pos] = '\0';
            
            // Process protocol messages directly in callback
            if (strcmp(rx_buffer, "ESB") == 0) {
                LOG_INF("BLESB confirmed ESB mode - enabling ESB transport");
                update_esb_connection_state(true);
                
            } else if (strcmp(rx_buffer, "RST") == 0) {
                LOG_INF("BLESB requesting reset - coordinated reboot");
                uart_send_string("RST\n");  // ACK reset request
                k_sleep(K_MSEC(50));        // Brief delay for UART TX
                sys_reboot(SYS_REBOOT_COLD);
                
            } else {
                LOG_WRN("Unknown BLESB message: %s", rx_buffer);
            }
            
            rx_pos = 0;
        } else if (rx_pos < sizeof(rx_buffer) - 1) {
            rx_buffer[rx_pos++] = c;
        }
    }
}

// ESB initialization function
static int zmk_esb_init(void) {
    LOG_INF("Initializing ESB transport");
    
    // Get UART device
    esb_uart_dev = DEVICE_DT_GET(DT_CHOSEN(zmk_esb_uart));
    
    if (!device_is_ready(esb_uart_dev)) {
        LOG_ERR("ESB UART device not ready");
        return -ENODEV;
    }
    
    // Set up UART interrupt for receiving messages
    uart_irq_callback_user_data_set(esb_uart_dev, uart_rx_callback, NULL);
    uart_irq_rx_enable(esb_uart_dev);
    
    // ESB starts disabled (like BLE) - callback will enable if BLESB responds
    update_esb_connection_state(false);
    
    // Query BLESB - async response via callback
    LOG_INF("Querying BLESB for ESB availability");
    uart_send_string("ESB\n");
    
    LOG_INF("ESB transport initialized - waiting for BLESB response");
    return 0;  // Always succeeds, async response enables transport
}

SYS_INIT(zmk_esb_init, APPLICATION, CONFIG_ZMK_ESB_INIT_PRIORITY);