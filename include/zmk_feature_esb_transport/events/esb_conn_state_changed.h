#pragma once

#include <zephyr/kernel.h>
#include <zmk/event_manager.h>

struct zmk_esb_conn_state_changed {
    bool connected;
};

ZMK_EVENT_DECLARE(zmk_esb_conn_state_changed);