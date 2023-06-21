add_subdirectory(nRF5_SDK)

add_library(bmi160 bmi160_driver/bmi160.c)
target_include_directories(bmi160 SYSTEM INTERFACE bmi160_driver)

add_library(bmm150 BMM150-Sensor-API/bmm150.c)
target_include_directories(bmm150 SYSTEM INTERFACE BMM150-Sensor-API)

#the cJSON CMakeLists is avoided because its a configuration nightmare.The
#library is only a single source file anyways.
add_library(cjson cJSON/cJSON.c cJSON/cJSON_Utils.c)
target_include_directories(cjson SYSTEM INTERFACE cJSON)
target_link_libraries(cjson INTERFACE m)

# note: the fatfs library was modified by moving the ffconf file out of the
# source directory and edits were made. If the fatfs library is updated in the
# future ensure to migrate the settings in ffconf.
#
# fatfs was configured WITHOUT thread safety. Keep this in mind if an RTOS is
# used. Threat safety can either be enabled within fatfs for with a lock on an
# upper application layer.
add_library(fatfs fatfs/source/ff.c fatfs/source/ffunicode.c)
target_include_directories(fatfs SYSTEM INTERFACE fatfs fatfs/source)
target_include_directories(fatfs PRIVATE fatfs)

add_subdirectory(preprocessor)


add_library(micro-ecc micro-ecc/uECC.c)
target_include_directories(micro-ecc SYSTEM INTERFACE micro-ecc)
target_compile_definitions(micro-ecc PUBLIC uECC_ENABLE_VLI_API=0 uECC_SQUARE_FUNC=0 uECC_SUPPORT_COMPRESSED_POINT=0 uECC_VLI_NATIVE_LITTLE_ENDIAN=1)
