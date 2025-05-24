idf_component_register(SRCS "bluetooth.c" "wendigo.c" "common.c" "wifi.c"
		    REQUIRES bt
		    REQUIRES esp_wifi
			REQUIRES console
			REQUIRES nvs_flash
			REQUIRES cmd_system
			REQUIRES cmd_wifi
			REQUIRES cmd_nvs
			REQUIRES esp_driver_uart
		    PRIV_REQUIRES fatfs
                    INCLUDE_DIRS ".")
target_link_libraries(${COMPONENT_LIB} -Wl,-zmuldefs)
target_compile_definitions(${COMPONENT_LIB} PUBLIC "-DLOG_LOCAL_LEVEL=ESP_LOG_INFO")