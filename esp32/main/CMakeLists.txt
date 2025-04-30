idf_component_register(SRCS "wendigo.c" "common.c"
		    REQUIRES bt
		    REQUIRES esp_wifi
		    PRIV_REQUIRES fatfs
                    INCLUDE_DIRS ".")
