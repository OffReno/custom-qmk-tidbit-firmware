#include "quantum.h"

// --- Bit-C helper stubs (not used on RP2040 converters) ---
void set_bitc_LED(uint8_t led_state) { (void)led_state; }
void matrix_init_remote_kb(void) {}
void matrix_scan_remote_kb(void) {}
bool process_record_remote_kb(uint16_t keycode, keyrecord_t *record) {
    (void)keycode; (void)record;
    return true;
}
