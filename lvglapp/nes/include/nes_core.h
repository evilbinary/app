#ifndef __TRANQUILOS_APPS_NES_CORE_H__
#define __TRANQUILOS_APPS_NES_CORE_H__

#include "stdint.h"

#define NES_CORE_VISIBLE_WIDTH 256U
#define NES_CORE_VISIBLE_HEIGHT 224U
#define NES_CORE_ROM_MAX_BYTES (1024U * 1024U)

enum {
	NES_BUTTON_A = 0x01U,
	NES_BUTTON_B = 0x02U,
	NES_BUTTON_SELECT = 0x04U,
	NES_BUTTON_START = 0x08U,
	NES_BUTTON_UP = 0x10U,
	NES_BUTTON_DOWN = 0x20U,
	NES_BUTTON_LEFT = 0x40U,
	NES_BUTTON_RIGHT = 0x80U,
};

uint8_t nes_core_load_rom(uint8_t *rom_file, uint32_t rom_len,
			     char *error, uint32_t error_cap);
void nes_core_unload(void);
uint8_t nes_core_is_loaded(void);
void nes_core_set_buttons(uint8_t buttons);
uint8_t nes_core_run_frame(uint16_t *dst_visible_rgb565);
const uint16_t *nes_core_get_visible_frame(void);

#endif /* __TRANQUILOS_APPS_NES_CORE_H__ */
