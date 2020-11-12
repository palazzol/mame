// license:BSD-3-Clause
// copyright-holders:Aaron Giles
/*************************************************************************

    Atari GT hardware

*************************************************************************/

#include "audio/cage.h"
#include "machine/adc0808.h"
#include "machine/atarigen.h"
#include "machine/timer.h"
#include "video/atarirle.h"
#include "emupal.h"
#include "tilemap.h"

#define CRAM_ENTRIES        0x4000
#define TRAM_ENTRIES        0x4000
#define MRAM_ENTRIES        0x8000

#define ADDRSEQ_COUNT   4

class atarigt_state : public atarigen_state
{
public:
	atarigt_state(const machine_config &mconfig, device_type type, const char *tag) :
		atarigen_state(mconfig, type, tag),
		m_palette(*this, "palette"),
		m_colorram(*this, "colorram", 0x80000, ENDIANNESS_BIG),
		m_adc(*this, "adc"),
		m_playfield_tilemap(*this, "playfield"),
		m_alpha_tilemap(*this, "alpha"),
		m_rle(*this, "rle"),
		m_service_io(*this, "SERVICE"),
		m_coin_io(*this, "COIN"),
		m_fake_io(*this, "FAKE"),
		m_mo_command(*this, "mo_command"),
		m_cage(*this, "cage")
	{ }

	bool           m_is_primrage;
	required_device<palette_device> m_palette;
	memory_share_creator<uint16_t> m_colorram;

	optional_device<adc0808_device> m_adc;

	required_device<tilemap_device> m_playfield_tilemap;
	required_device<tilemap_device> m_alpha_tilemap;
	required_device<atari_rle_objects_device> m_rle;

	optional_ioport m_service_io;
	optional_ioport m_coin_io;
	optional_ioport m_fake_io;

	bool            m_scanline_int_state;
	bool            m_video_int_state;

	bitmap_ind16    m_pf_bitmap;
	bitmap_ind16    m_an_bitmap;

	uint8_t           m_playfield_tile_bank;
	uint8_t           m_playfield_color_bank;
	uint16_t          m_playfield_xscroll;
	uint16_t          m_playfield_yscroll;

	uint32_t          m_tram_checksum;

	required_shared_ptr<uint32_t> m_mo_command;
	optional_device<atari_cage_device> m_cage;

	void            (atarigt_state::*m_protection_w)(address_space &space, offs_t offset, uint16_t data);
	void            (atarigt_state::*m_protection_r)(address_space &space, offs_t offset, uint16_t *data);

	bool            m_ignore_writes;
	offs_t          m_protaddr[ADDRSEQ_COUNT];
	uint8_t           m_protmode;
	uint16_t          m_protresult;
	std::unique_ptr<uint8_t[]> m_protdata;

	INTERRUPT_GEN_MEMBER(scanline_int_gen);
	DECLARE_WRITE_LINE_MEMBER(video_int_write_line);
	void scanline_int_ack_w(uint32_t data = 0);
	void video_int_ack_w(uint32_t data = 0);
	TIMER_DEVICE_CALLBACK_MEMBER(scanline_update);
	uint32_t special_port2_r();
	uint32_t special_port3_r();
	uint8_t analog_port_r(offs_t offset);
	void latch_w(offs_t offset, uint32_t data, uint32_t mem_mask = ~0);
	void mo_command_w(offs_t offset, uint32_t data, uint32_t mem_mask = ~0);
	void led_w(offs_t offset, uint32_t data, uint32_t mem_mask = ~0);
	uint32_t sound_data_r(offs_t offset, uint32_t mem_mask = ~0);
	void sound_data_w(offs_t offset, uint32_t data, uint32_t mem_mask = ~0);
	uint32_t colorram_protection_r(address_space &space, offs_t offset, uint32_t mem_mask = ~0);
	void colorram_protection_w(address_space &space, offs_t offset, uint32_t data, uint32_t mem_mask = ~0);
	void tmek_pf_w(offs_t offset, uint32_t data, uint32_t mem_mask = ~0);

	void cage_irq_callback(uint8_t data);

	void atarigt_colorram_w(offs_t address, uint16_t data, uint16_t mem_mask);
	uint16_t atarigt_colorram_r(offs_t address);
	void init_primrage();
	void init_tmek();
	TILE_GET_INFO_MEMBER(get_alpha_tile_info);
	TILE_GET_INFO_MEMBER(get_playfield_tile_info);
	TILEMAP_MAPPER_MEMBER(atarigt_playfield_scan);
	virtual void machine_start() override;
	DECLARE_VIDEO_START(atarigt);
	uint32_t screen_update_atarigt(screen_device &screen, bitmap_rgb32 &bitmap, const rectangle &cliprect);
	void atarigt(machine_config &config);
	void tmek(machine_config &config);
	void primrage20(machine_config &config);
	void primrage(machine_config &config);
	void main_map(address_map &map);
private:
	void tmek_update_mode(offs_t offset);
	void tmek_protection_w(address_space &space, offs_t offset, uint16_t data);
	void tmek_protection_r(address_space &space, offs_t offset, uint16_t *data);
	void primrage_update_mode(offs_t offset);
	void primrage_protection_w(address_space &space, offs_t offset, uint16_t data);
	void primrage_protection_r(address_space &space, offs_t offset, uint16_t *data);
	void compute_fake_pots(int *pots);
};
