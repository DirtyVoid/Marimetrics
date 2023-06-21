add_library(filesystem filesystem.c fatfs_bindings.c newlib_bindings.c)
target_link_libraries(filesystem PRIVATE board sdcard fatfs)
