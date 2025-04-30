idf_component_register(SRCS "wendigo_main.c"
                       PRIV_REQUIRES spi_flash
		       REQUIRES bt
		       REQUIRES esp_wifi
                       INCLUDE_DIRS ".")
