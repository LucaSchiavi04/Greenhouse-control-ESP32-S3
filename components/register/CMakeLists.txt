idf_component_register(SRCS "register.c"
                    INCLUDE_DIRS "include"
                    REQUIRES nvs_flash joltwallet__littlefs esp_timer
                    )
