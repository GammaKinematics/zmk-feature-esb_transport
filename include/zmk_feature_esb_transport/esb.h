#pragma once

#include <stdbool.h>

/**
 * @brief Check if ESB transport is connected and ready
 * 
 * This function returns the current connection state of the ESB transport.
 * The ESB transport is considered connected when BLESB has confirmed it is
 * in ESB mode via the UART protocol.
 * 
 * @return true if ESB transport is available and ready, false otherwise
 */
bool zmk_esb_active_profile_is_connected(void);

// Future expansion space for:
// - Profile management functions  
// - Address configuration functions
// - Channel selection functions
// - Connection quality functions