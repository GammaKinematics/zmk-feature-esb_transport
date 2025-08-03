if(CONFIG_ZMK_ESB)
    target_sources(app PRIVATE
        src/esb_hid.c
        src/events/esb_conn_state_changed.c
    )
    target_include_directories(app PRIVATE include)
endif()