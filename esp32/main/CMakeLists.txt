idf_component_register(SRCS "bluetooth.c" "wendigo.c" "common.c"
		    REQUIRES bt
		    REQUIRES esp_wifi
			REQUIRES console
			REQUIRES nvs_flash
			REQUIRES cmd_system
			REQUIRES cmd_wifi
			REQUIRES cmd_nvs
		    PRIV_REQUIRES fatfs
                    INCLUDE_DIRS ".")
target_link_libraries(${COMPONENT_LIB} -Wl,-zmuldefs)