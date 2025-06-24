idf_component_register(SRCS "main.c" "peer-manager.c" "wifi-manager.c"
  INCLUDE_DIRS "." "includes"
  REQUIRES libpeer esp_wifi nvs_flash
)

idf_component_get_property(lib libpeer COMPONENT_LIB)
target_compile_options(${lib} PRIVATE -Wno-error=restrict)
target_compile_options(${lib} PRIVATE -Wno-error=stringop-truncation)

idf_component_get_property(lib sepfy__srtp COMPONENT_LIB)
target_compile_options(${lib} PRIVATE -Wno-error=incompatible-pointer-types)
