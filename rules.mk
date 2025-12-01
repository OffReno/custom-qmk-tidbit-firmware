# Project specific files
PIN_COMPATIBLE = promicro

# # Only include these for the *native* Pro Micro/Bit-C build.
# # When converting to RP2040 etc. (CONVERT_TO != empty) they must be excluded.
# ifeq ($(strip $(CONVERT_TO)),)
#     # Project specific files (Bit-C LED helper, remote_kb, etc.)
#     SRC += common/bitc_led.c 
#     I2C_DRIVER_REQUIRED = yes
#     UART_DRIVER_REQUIRED = yes
# endif
