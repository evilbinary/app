#include "nes_core.h"

#include "stdio.h"
#include "string.h"
#include "stdint.h"

#define PULL mem(++S, 1, 0, 0)
#define PUSH(x) mem(S--, 1, x, 1)

static uint8_t *romfile = NULL;
static uint32_t romfile_len = 0;
static uint8_t *rom = NULL;
static uint8_t *chrrom = NULL;
static uint8_t prg[4];
static uint8_t chr[8];
static uint8_t prgbits = 14;
static uint8_t chrbits = 12;
static uint8_t A = 0;
static uint8_t X = 0;
static uint8_t Y = 0;
static uint8_t P = 4;
static uint8_t S = (uint8_t)~2U;
static uint8_t PCH = 0;
static uint8_t PCL = 0;
static uint8_t addr_lo = 0;
static uint8_t addr_hi = 0;
static uint8_t nomem = 0;
static uint8_t result = 0;
static uint8_t val = 0;
static uint8_t cross = 0;
static uint8_t tmp = 0;
static uint8_t ppumask = 0;
static uint8_t ppuctrl = 0;
static uint8_t ppustatus = 0;
static uint8_t ppubuf = 0;
static uint8_t W = 0;
static uint8_t fine_x = 0;
static uint8_t opcode = 0;
static uint8_t nmi_irq = 0;
static uint8_t ntb = 0;
static uint8_t ptb_lo = 0;
static uint8_t vram[2048];
static uint8_t palette_ram[64];
static uint8_t ram[8192];
static uint8_t chrram[8192];
static uint8_t prgram[8192];
static uint8_t oam[256];
static const uint8_t mask[] = {
	128, 64, 1, 2, 1, 0, 0, 1, 4, 0, 0, 4, 0, 0, 64, 0, 8, 0, 0, 8,
};
static uint8_t keys = 0;
static uint8_t mirror = 0;
static uint8_t mmc1_bits = 0;
static uint8_t mmc1_data = 0;
static uint8_t mmc1_ctrl = 0;
static uint8_t mmc3_chrprg[8];
static uint8_t mmc3_bits = 0;
static uint8_t mmc3_irq = 0;
static uint8_t mmc3_latch = 0;
static uint8_t chrbank0 = 0;
static uint8_t chrbank1 = 0;
static uint8_t prgbank = 0;
static uint8_t input_mask = 0;
static uint8_t core_loaded = 0;

static uint16_t scany = 0;
static uint16_t T = 0;
static uint16_t V = 0;
static uint16_t sum = 0;
static uint16_t dot = 0;
static uint16_t atb = 0;
static uint16_t shift_hi = 0;
static uint16_t shift_lo = 0;
static uint16_t cycles = 0;
static uint16_t frame_buffer[256U * 240U];
static int shift_at = 0;

static const uint16_t nes_palette_bgr565[64] = {
	25356, 34816, 39011, 30854, 24714, 4107,  106,   2311,
	2468,  2561,  4642,  6592,  20832, 0,     0,     0,
	44373, 49761, 55593, 51341, 43186, 18675, 434,   654,
	4939,  5058,  3074,  19362, 37667, 0,     0,     0,
	65535, 64717, 64497, 64342, 62331, 43932, 23612, 9465,
	1429,  1550,  20075, 36358, 52713, 16904, 0,     0,
	65535, 65207, 65113, 65083, 65053, 58911, 50814, 42620,
	40667, 40729, 48951, 53078, 61238, 44405, 0,     0,
};
static uint16_t nes_palette_rgb565[64];
static uint8_t nes_palette_ready = 0;

static void nes_core_set_error(char *error, uint32_t cap, const char *message)
{
	if (error == NULL || cap == 0U) {
		return;
	}

	memset(error, 0, cap);
	if (message == NULL) {
		return;
	}

	strncpy(error, message, cap - 1U);
}

static uint16_t nes_core_bgr565_to_rgb565(uint16_t color)
{
	return (uint16_t)((color & 0x07e0U) |
			  ((color & 0xf800U) >> 11) |
			  ((color & 0x001fU) << 11));
}

static void nes_core_prepare_palette(void)
{
	if (nes_palette_ready) {
		return;
	}

	for (uint32_t i = 0; i < 64U; i++) {
		nes_palette_rgb565[i] = nes_core_bgr565_to_rgb565(nes_palette_bgr565[i]);
	}

	nes_palette_ready = 1U;
}

static void nes_core_reset_runtime(void)
{
	memset(prg, 0, sizeof(prg));
	memset(chr, 0, sizeof(chr));
	memset(vram, 0, sizeof(vram));
	memset(palette_ram, 0, sizeof(palette_ram));
	memset(ram, 0, sizeof(ram));
	memset(chrram, 0, sizeof(chrram));
	memset(prgram, 0, sizeof(prgram));
	memset(oam, 0, sizeof(oam));
	memset(mmc3_chrprg, 0, sizeof(mmc3_chrprg));
	memset(frame_buffer, 0, sizeof(frame_buffer));

	prgbits = 14;
	chrbits = 12;
	A = 0;
	X = 0;
	Y = 0;
	P = 4;
	S = (uint8_t)~2U;
	PCH = 0;
	PCL = 0;
	addr_lo = 0;
	addr_hi = 0;
	nomem = 0;
	result = 0;
	val = 0;
	cross = 0;
	tmp = 0;
	ppumask = 0;
	ppuctrl = 0;
	ppustatus = 0;
	ppubuf = 0;
	W = 0;
	fine_x = 0;
	opcode = 0;
	nmi_irq = 0;
	ntb = 0;
	ptb_lo = 0;
	keys = 0;
	mirror = 0;
	mmc1_bits = 0;
	mmc1_data = 0;
	mmc1_ctrl = 0;
	mmc3_bits = 0;
	mmc3_irq = 0;
	mmc3_latch = 0;
	chrbank0 = 0;
	chrbank1 = 0;
	prgbank = 0;
	scany = 0;
	T = 0;
	V = 0;
	sum = 0;
	dot = 0;
	atb = 0;
	shift_hi = 0;
	shift_lo = 0;
	cycles = 0;
	shift_at = 0;
}

static uint8_t *get_chr_byte(uint16_t a)
{
	return &chrrom[(uint32_t)chr[a >> chrbits] << chrbits | (a % (1U << chrbits))];
}

static uint8_t *get_nametable_byte(uint16_t a)
{
	return &vram[mirror == 0   ? a % 1024U
		     : mirror == 1 ? a % 1024U + 1024U
		     : mirror == 2 ? a & 2047U
				   : ((a / 2U) & 1024U) | (a % 1024U)];
}

static uint8_t mem(uint8_t lo, uint8_t hi, uint8_t write_val, uint8_t write)
{
	uint16_t addr = (uint16_t)hi << 8 | lo;

	switch (hi >>= 4) {
		case 0:
		case 1:
			return write ? (ram[addr] = write_val) : ram[addr];

		case 2:
		case 3:
			lo &= 7U;
			if (lo == 7U) {
				uint8_t buffered = ppubuf;
				uint8_t *ppu_ptr =
					V < 8192U ? ((write && chrrom != chrram) ? &buffered : get_chr_byte(V))
						  : V < 16128U ? get_nametable_byte(V)
							       : palette_ram + (uint8_t)(((V & 19U) == 16U) ? (V ^ 16U) : V);
				if (write) {
					*ppu_ptr = write_val;
				} else {
					ppubuf = *ppu_ptr;
				}
				V += (ppuctrl & 4U) ? 32U : 1U;
				V %= 16384U;
				return buffered;
			}

			if (write) {
				switch (lo) {
					case 0:
						ppuctrl = write_val;
						T = (uint16_t)((T & 0xf3ffU) | ((write_val % 4U) << 10));
						break;
					case 1:
						ppumask = write_val;
						break;
					case 5:
						T = (W ^= 1U)
							    ? (uint16_t)((fine_x = write_val & 7U), (T & ~31U) | (write_val / 8U))
							    : (uint16_t)((T & 0x8c1fU) |
									 (uint16_t)((write_val % 8U) << 12) |
									 (uint16_t)((write_val * 4U) & 0x3e0U));
						break;
					case 6:
						T = (W ^= 1U)
							    ? (uint16_t)((T & 0x00ffU) | ((write_val % 64U) << 8))
							    : (V = (uint16_t)((T & ~0x00ffU) | write_val));
						break;
					default:
						break;
				}
			}

			if (lo == 2U) {
				uint8_t status = ppustatus & 0xe0U;
				ppustatus &= 0x7fU;
				W = 0;
				return status;
			}
			break;

		case 4: {
			if (write && lo == 20U) {
				for (uint16_t i = 256U; i-- > 0U;) {
					oam[i] = mem((uint8_t)i, write_val, 0, 0);
				}
			}

			if (lo == 22U) {
				if (write) {
					keys = input_mask;
				} else {
					uint8_t key = keys & 1U;
					keys >>= 1;
					return key;
				}
			}
			return 0;
		}

		case 6:
		case 7:
			addr &= 8191U;
			return write ? (prgram[addr] = write_val) : prgram[addr];

		default:
			if (write) {
				switch (romfile[6] >> 4) {
					case 7:
						mirror = (uint8_t)!(write_val / 16U);
						prg[0] = (uint8_t)((write_val % 8U) * 2U);
						prg[1] = (uint8_t)(prg[0] + 1U);
						break;
					case 4: {
						uint8_t addr1 = addr & 1U;
						switch (hi >> 1) {
							case 4:
								*(addr1 ? &mmc3_chrprg[mmc3_bits & 7U] : &mmc3_bits) = write_val;
								tmp = (uint8_t)((mmc3_bits >> 5) & 4U);
								for (int i = 4; i-- > 0;) {
									chr[0 + i + tmp] = (uint8_t)((mmc3_chrprg[i / 2] & (uint8_t)~!(i % 2)) | (i % 2));
									chr[4 + i - tmp] = mmc3_chrprg[2 + i];
								}
								tmp = (uint8_t)((mmc3_bits >> 5) & 2U);
								prg[0 + tmp] = mmc3_chrprg[6];
								prg[1] = mmc3_chrprg[7];
								prg[3] = (uint8_t)(romfile[4] * 2U - 1U);
								prg[2 - tmp] = (uint8_t)(prg[3] - 1U);
								break;
							case 5:
								if (!addr1) {
									mirror = (uint8_t)(2U + (write_val % 2U));
								}
								break;
							case 6:
								if (!addr1) {
									mmc3_latch = write_val;
								}
								break;
							case 7:
								mmc3_irq = addr1;
								break;
							default:
								break;
						}
						break;
					}
					case 3:
						chr[0] = (uint8_t)((write_val % 4U) * 2U);
						chr[1] = (uint8_t)(chr[0] + 1U);
						break;
					case 2:
						prg[0] = write_val & 31U;
						break;
					case 1:
						if (write_val & 0x80U) {
							mmc1_bits = 5U;
							mmc1_data = 0U;
							mmc1_ctrl |= 12U;
						} else if ((mmc1_data = (uint8_t)(mmc1_data / 2U | (write_val << 4 & 16U))),
							   !--mmc1_bits) {
							mmc1_bits = 5U;
							tmp = (uint8_t)(addr >> 13);
							*(tmp == 4U ? (mirror = mmc1_data & 3U, &mmc1_ctrl)
							    : tmp == 5U ? &chrbank0
							    : tmp == 6U ? &chrbank1
									: &prgbank) = mmc1_data;

							chr[0] = (uint8_t)(chrbank0 & (uint8_t)~!(mmc1_ctrl & 16U));
							chr[1] = (uint8_t)((mmc1_ctrl & 16U) ? chrbank1 : (chrbank0 | 1U));

							tmp = (uint8_t)(mmc1_ctrl / 4U % 4U - 2U);
							prg[0] = !tmp ? 0U : tmp == 1U ? prgbank : (uint8_t)(prgbank & (uint8_t)~1U);
							prg[1] = !tmp ? prgbank
								      : tmp == 1U ? (uint8_t)(romfile[4] - 1U)
										   : (uint8_t)(prgbank | 1U);
						}
						break;
					default:
						break;
				}
			}

			return rom[((uint32_t)(prg[(hi - 8U) >> (prgbits - 12U)] &
					    ((romfile[4] << (14U - prgbits)) - 1U))
				    << prgbits) |
				   (addr & ((1U << prgbits) - 1U))];
	}

	return (uint8_t)~0U;
}

static uint8_t read_pc(void)
{
	val = mem(PCL, PCH, 0, 0);
	if (!++PCL) {
		++PCH;
	}
	return val;
}

static uint8_t set_nz(uint8_t nz_val)
{
	return P = (uint8_t)((P & 125U) | (nz_val & 128U) | (!nz_val * 2U));
}

static void nes_core_copy_visible_frame(uint16_t *dst_visible_rgb565)
{
	for (uint32_t y = 0; y < NES_CORE_VISIBLE_HEIGHT; y++) {
		for (uint32_t x = 0; x < NES_CORE_VISIBLE_WIDTH; x++) {
			dst_visible_rgb565[y * NES_CORE_VISIBLE_WIDTH + x] =
				frame_buffer[(y + 8U) * 256U + x];
		}
	}
}

const uint16_t *nes_core_get_visible_frame(void)
{
	return frame_buffer + (8U * 256U);
}

uint8_t nes_core_load_rom(uint8_t *rom_file, uint32_t rom_len,
			     char *error, uint32_t error_cap)
{
	uint32_t trainer_size = 0;
	uint32_t prg_bytes = 0;
	uint32_t chr_bytes = 0;
	uint32_t mapper = 0;

	nes_core_set_error(error, error_cap, NULL);
	nes_core_prepare_palette();
	nes_core_unload();

	if (rom_file == NULL || rom_len < 16U) {
		nes_core_set_error(error, error_cap, "ROM is missing or too small.");
		return 0U;
	}
	if (!(rom_file[0] == 'N' && rom_file[1] == 'E' && rom_file[2] == 'S' && rom_file[3] == 0x1aU)) {
		nes_core_set_error(error, error_cap, "Only iNES ROM files are supported.");
		return 0U;
	}
	if ((rom_file[7] & 0x0cU) == 0x08U) {
		nes_core_set_error(error, error_cap, "NES 2.0 ROM files are not supported yet.");
		return 0U;
	}

	mapper = (uint32_t)(rom_file[6] >> 4) | (uint32_t)(rom_file[7] & 0xf0U);
	if (!(mapper == 0U || mapper == 1U || mapper == 2U || mapper == 3U ||
	      mapper == 4U || mapper == 7U)) {
		snprintf(error, error_cap, "Mapper %u is not supported by smolnes.", (unsigned int)mapper);
		return 0U;
	}
	if (mapper > 15U) {
		nes_core_set_error(error, error_cap, "Only 4-bit iNES mapper IDs are supported.");
		return 0U;
	}

	trainer_size = (rom_file[6] & 0x04U) ? 512U : 0U;
	prg_bytes = (uint32_t)rom_file[4] << 14;
	chr_bytes = (uint32_t)rom_file[5] << 13;
	if (prg_bytes == 0U) {
		nes_core_set_error(error, error_cap, "ROM does not contain any PRG data.");
		return 0U;
	}
	if (16U + trainer_size + prg_bytes + chr_bytes > rom_len) {
		nes_core_set_error(error, error_cap, "ROM file is truncated.");
		return 0U;
	}

	romfile = rom_file;
	romfile_len = rom_len;
	nes_core_reset_runtime();

	rom = romfile + 16U + trainer_size;
	chrrom = romfile[5] ? (rom + prg_bytes) : chrram;
	prg[1] = (uint8_t)(romfile[4] - 1U);
	chr[1] = romfile[5] ? (uint8_t)(romfile[5] * 2U - 1U) : 1U;
	mirror = (uint8_t)(3U - (romfile[6] % 2U));

	if ((romfile[6] >> 4) == 4U) {
		mem(0, 128, 0, 1);
		prgbits--;
		chrbits = (uint8_t)(chrbits - 2U);
	}

	PCL = mem((uint8_t)~3U, (uint8_t)~0U, 0, 0);
	PCH = mem((uint8_t)~2U, (uint8_t)~0U, 0, 0);
	core_loaded = 1U;
	return 1U;
}

void nes_core_unload(void)
{
	core_loaded = 0U;
	input_mask = 0U;
	romfile = NULL;
	romfile_len = 0U;
	rom = NULL;
	chrrom = NULL;
	nes_core_reset_runtime();
}

uint8_t nes_core_is_loaded(void)
{
	return core_loaded;
}

void nes_core_set_buttons(uint8_t buttons)
{
	input_mask = buttons;
}

uint8_t nes_core_run_frame(uint16_t *dst_visible_rgb565)
{
	uint8_t frame_ready = 0U;

	if (!core_loaded || romfile == NULL || romfile_len < 16U) {
		return 0U;
	}

	for (;;) {
		cycles = 0;
		nomem = 0;
		if (nmi_irq) {
			goto nmi_irq;
		}

		opcode = read_pc();
		switch (opcode & 31U) {
			case 0:
				if (opcode & 0x80U) {
					read_pc();
					nomem = 1U;
					goto nomemop;
				}

				switch (opcode >> 5) {
					case 0: {
						!++PCL && ++PCH;
nmi_irq:
						PUSH(PCH);
						PUSH(PCL);
						PUSH((uint8_t)(P | 32U));
						{
							uint16_t veclo = (uint16_t)(~1U - (nmi_irq & 4U));
							PCL = mem((uint8_t)veclo, (uint8_t)~0U, 0, 0);
							PCH = mem((uint8_t)(veclo + 1U), (uint8_t)~0U, 0, 0);
						}
						nmi_irq = 0;
						cycles++;
						break;
					}
					case 1:
						result = read_pc();
						PUSH(PCH);
						PUSH(PCL);
						PCH = read_pc();
						PCL = result;
						break;
					case 2:
						P = (uint8_t)(PULL & (uint8_t)~32U);
						PCL = PULL;
						PCH = PULL;
						break;
					case 3:
						PCL = PULL;
						PCH = PULL;
						!++PCL && ++PCH;
						break;
					default:
						break;
				}

				cycles += 4U;
				break;

			case 16:
				read_pc();
				if (((!(P & mask[opcode >> 6])) ^ ((opcode / 32U) & 1U)) != 0U) {
					cross = (uint8_t)((PCL + (int8_t)val) >> 8);
					PCH = (uint8_t)(PCH + cross);
					PCL = (uint8_t)(PCL + val);
					cycles = (uint16_t)(cycles + (cross ? 2U : 1U));
				}
				break;

			case 8:
			case 24:
				switch (opcode >>= 4) {
					case 0:
						PUSH((uint8_t)(P | 48U));
						cycles++;
						break;
					case 2:
						P = (uint8_t)(PULL & (uint8_t)~16U);
						cycles += 2U;
						break;
					case 4:
						PUSH(A);
						cycles++;
						break;
					case 6:
						set_nz(A = PULL);
						cycles += 2U;
						break;
					case 8:
						set_nz(--Y);
						break;
					case 9:
						set_nz(A = Y);
						break;
					case 10:
						set_nz(Y = A);
						break;
					case 12:
						set_nz(++Y);
						break;
					case 14:
						set_nz(++X);
						break;
					default:
						P = (uint8_t)((P & (uint8_t)~mask[opcode + 3U]) | mask[opcode + 4U]);
						break;
				}
				break;

			case 10:
			case 26:
				switch (opcode >> 4) {
					case 8:
						set_nz(A = X);
						break;
					case 9:
						S = X;
						break;
					case 10:
						set_nz(X = A);
						break;
					case 11:
						set_nz(X = S);
						break;
					case 12:
						set_nz(--X);
						break;
					case 14:
						break;
					default:
						nomem = 1U;
						val = A;
						goto nomemop;
				}
				break;

			case 1:
				read_pc();
				val = (uint8_t)(val + X);
				addr_lo = mem(val, 0, 0, 0);
				addr_hi = mem((uint8_t)(val + 1U), 0, 0, 0);
				cycles += 4U;
				goto opcode_addr;

			case 2:
			case 9:
				read_pc();
				nomem = 1U;
				goto nomemop;

			case 17:
				addr_lo = mem(read_pc(), 0, 0, 0);
				addr_hi = mem((uint8_t)(val + 1U), 0, 0, 0);
				cycles++;
				goto add_x_or_y;

			case 4:
			case 5:
			case 6:
			case 20:
			case 21:
			case 22:
				addr_lo = read_pc();
				cross = (uint8_t)((opcode & 31U) > 6U);
				if (cross) {
					addr_lo = (uint8_t)(addr_lo + (((opcode & 214U) == 150U) ? Y : X));
				}
				addr_hi = 0;
				cycles = (uint16_t)(cycles - !cross);
				goto opcode_addr;

			case 12:
			case 13:
			case 14:
			case 25:
			case 28:
			case 29:
			case 30:
				addr_lo = read_pc();
				addr_hi = read_pc();
				if ((opcode & 31U) < 25U) {
					goto opcode_addr;
				}
add_x_or_y:
				val = ((opcode & 31U) < 28U || opcode == 190U) ? Y : X;
				cross = (uint8_t)(addr_lo + val > 255U);
				addr_hi = (uint8_t)(addr_hi + cross);
				addr_lo = (uint8_t)(addr_lo + val);
				cycles = (uint16_t)(cycles +
						    ((((opcode & 224U) == 128U) ||
						      ((opcode % 16U == 14U) && opcode != 190U)) ||
						     cross));
opcode_addr:
				cycles += 2U;
				if (opcode != 76U && (opcode & 224U) != 128U) {
					val = mem(addr_lo, addr_hi, 0, 0);
				}

nomemop:
				result = 0;
				switch (opcode & 227U) {
					case 1:
						set_nz(A |= val);
						break;
					case 33:
						set_nz(A &= val);
						break;
					case 65:
						set_nz(A ^= val);
						break;
					case 225:
						val = (uint8_t)~val;
					case 97:
						sum = (uint16_t)(A + val + (P % 2U));
						P = (uint8_t)((P & (uint8_t)~65U) |
							      (sum > 255U) |
							      (((A ^ sum) & (val ^ sum) & 128U) / 2U));
						set_nz(A = (uint8_t)sum);
						break;
					case 34:
						result = (uint8_t)(P & 1U);
					case 2:
						result = (uint8_t)(result | (val * 2U));
						P = (uint8_t)((P & (uint8_t)~1U) | (val / 128U));
						goto memop;
					case 98:
						result = (uint8_t)(P << 7);
					case 66:
						result = (uint8_t)(result | (val / 2U));
						P = (uint8_t)((P & (uint8_t)~1U) | (val & 1U));
						goto memop;
					case 194:
						result = (uint8_t)(val - 1U);
						goto memop;
					case 226:
						result = (uint8_t)(val + 1U);
memop:
						set_nz(result);
						if (nomem) {
							A = result;
						} else {
							cycles += 2U;
							mem(addr_lo, addr_hi, result, 1);
						}
						break;
					case 32:
						P = (uint8_t)((P & 61U) | (val & 192U) | (!(A & val) * 2U));
						break;
					case 64:
						PCL = addr_lo;
						PCH = addr_hi;
						cycles--;
						break;
					case 96:
						PCL = val;
						PCH = mem((uint8_t)(addr_lo + 1U), addr_hi, 0, 0);
						cycles++;
						break;
					default: {
						uint8_t opcodehi3 = opcode / 32U;
						uint8_t *reg = ((opcode % 4U == 2U) || opcodehi3 == 7U)
									 ? &X
									 : (opcode % 4U == 1U ? &A : &Y);
						if (opcodehi3 == 4U) {
							mem(addr_lo, addr_hi, *reg, 1);
						} else if (opcodehi3 != 5U) {
							P = (uint8_t)((P & (uint8_t)~1U) | (*reg >= val));
							set_nz((uint8_t)(*reg - val));
						} else {
							set_nz(*reg = val);
						}
						break;
					}
				}
				break;
			default:
				break;
		}

		for (tmp = (uint8_t)(cycles * 3U + 6U); tmp-- > 0U;) {
			if (ppumask & 24U) {
				if (scany < 240U) {
					if ((uint16_t)(dot - 256U) > 63U) {
						if (dot < 256U) {
							uint8_t color = (uint8_t)(((shift_hi >> (14U - fine_x)) & 2U) |
										 ((shift_lo >> (15U - fine_x)) & 1U));
							uint8_t palette = (uint8_t)((shift_at >> (28U - fine_x * 2U)) & 12U);

							if (ppumask & 16U) {
								for (uint8_t *sprite = oam; sprite < oam + 256U; sprite += 4U) {
									uint16_t sprite_h = (ppuctrl & 32U) ? 16U : 8U;
									uint16_t sprite_x = (uint16_t)(dot - sprite[3]);
									uint16_t sprite_y = (uint16_t)(scany - sprite[0] - 1U);
									uint16_t sx = (uint16_t)(sprite_x ^ (! (sprite[2] & 64U) * 7U));
									uint16_t sy = (uint16_t)(sprite_y ^ ((sprite[2] & 128U) ? (sprite_h - 1U) : 0U));

									if (sprite_x < 8U && sprite_y < sprite_h) {
										uint16_t sprite_tile = sprite[1];
										uint16_t sprite_addr =
											((ppuctrl & 32U)
											     ? ((sprite_tile % 2U) << 12) |
												   (sprite_tile << 4 & (uint16_t)-32) |
												   (sy * 2U & 16U)
											     : ((ppuctrl & 8U) << 9) | (sprite_tile << 4)) |
											(sy & 7U);
										uint16_t sprite_color =
											(uint16_t)((*get_chr_byte((uint16_t)(sprite_addr + 8U)) >> sx) << 1U & 2U) |
											(uint16_t)((*get_chr_byte(sprite_addr) >> sx) & 1U);

										if (sprite_color) {
											if (!(sprite[2] & 32U && color)) {
												color = (uint8_t)sprite_color;
												palette = (uint8_t)(16U | (sprite[2] * 4U & 12U));
											}
											if (sprite == oam && color) {
												ppustatus |= 64U;
											}
											break;
										}
									}
								}
							}

							frame_buffer[scany * 256U + dot] =
								nes_palette_rgb565[palette_ram[color ? (palette | color) : 0U] & 63U];
						}

						if (dot < 336U) {
							shift_hi <<= 1;
							shift_lo <<= 1;
							shift_at <<= 2;
						}

						{
							int temp = ((ppuctrl << 8) & 4096) | (ntb << 4) | (V >> 12);
							switch (dot & 7U) {
								case 1:
									ntb = *get_nametable_byte(V);
									break;
								case 3:
									atb = (uint16_t)((*get_nametable_byte((uint16_t)((V & 0x0c00U) | 0x03c0U |
																((V >> 4) & 0x0038U) |
																((V / 4U) & 7U))) >>
												((V >> 5 & 2U) | ((V / 2U) & 1U)) * 2U) %
										   4U * 0x5555U);
									break;
								case 5:
									ptb_lo = *get_chr_byte((uint16_t)temp);
									break;
								case 7: {
									uint8_t ptb_hi = *get_chr_byte((uint16_t)(temp | 8));
									V = (uint16_t)(V % 32U == 31U ? (V & (uint16_t)~31U) ^ 1024U : V + 1U);
									shift_hi = (uint16_t)(shift_hi | ptb_hi);
									shift_lo = (uint16_t)(shift_lo | ptb_lo);
									shift_at |= atb;
									break;
								}
								default:
									break;
							}
						}
					}

					if (dot == 256U) {
						uint16_t next_v = 0U;

						if ((V & (7U << 12)) != (7U << 12)) {
							next_v = (uint16_t)(V + 4096U);
						} else if ((V & 0x03e0U) == 928U) {
							next_v = (uint16_t)((V & 0x8c1fU) ^ 2048U);
						} else if ((V & 0x03e0U) == 0x03e0U) {
							next_v = (uint16_t)(V & 0x8c1fU);
						} else {
							next_v = (uint16_t)((V & 0x8c1fU) | ((V + 32U) & 0x03e0U));
						}

						V = (uint16_t)((next_v & (uint16_t)~0x041fU) | (T & 0x041fU));
					}
				}

				if (((scany + 1U) % 262U) < 241U && dot == 261U && mmc3_irq && !mmc3_latch--) {
					nmi_irq = 1U;
				}

				if (scany == 261U && (uint16_t)(dot - 280U) < 25U) {
					V = (uint16_t)((V & 0x841fU) | (T & 0x7be0U));
				}
			}

			if (dot == 1U) {
				if (scany == 241U) {
					if (ppuctrl & 128U) {
						nmi_irq = 4U;
					}
					ppustatus |= 128U;
					frame_ready = 1U;
				}

				if (scany == 261U) {
					ppustatus = 0U;
				}
			}

			if (++dot == 341U) {
				dot = 0U;
				scany = (uint16_t)((scany + 1U) % 262U);
			}
		}

		if (frame_ready) {
			if (dst_visible_rgb565 != NULL) {
				nes_core_copy_visible_frame(dst_visible_rgb565);
			}
			return 1U;
		}
	}
}
