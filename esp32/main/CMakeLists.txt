idf_component_register(SRCS "wendigo.c" "bluetooth.c" "common.c"
                    INCLUDE_DIRS ".")
target_link_libraries(${COMPONENT_LIB} -Wl,-zmuldefs)