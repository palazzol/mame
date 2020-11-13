// license:BSD-3-Clause
// copyright-holders:Nathan Woods, Miodrag Milanovic, Frank Palazzolo
/*********************************************************************

    intvkbd_tapedrive.h

    (originally based on cassette.h)

*********************************************************************/

#ifndef MAME_MACHINE_INTVKBD_TAPEDRIVE_H
#define MAME_MACHINE_INTVKBD_TAPEDRIVE_H

#include "formats/cassimg.h"
#include "softlist_dev.h"
	
#if 0
enum intvkbd_tapedrive_state
{
	/* this part of the state is controlled by drivers */
	INTVKBD_CASSETTE_STOPPED            = 0,
	INTVKBD_CASSETTE_PLAY               = 1,
	INTVKBD_CASSETTE_RECORD             = 2,
	INTVKBD_CASSETTE_MOTOR_ENABLED      = 0,
	INTVKBD_CASSETTE_MOTOR_DISABLED     = 4,
	INTVKBD_CASSETTE_SPEAKER_ENABLED    = 0,
	INTVKBD_CASSETTE_SPEAKER_MUTED      = 8,

	/* masks */
	INTVKBD_CASSETTE_MASK_UISTATE       = 0,
	INTVKBD_CASSETTE_MASK_MOTOR         = 4,
	INTVKBD_CASSETTE_MASK_SPEAKER       = 8,
	INTVKBD_CASSETTE_MASK_DRVSTATE      = 12
};
#endif

/***************************************************************************
    TYPE DEFINITIONS
***************************************************************************/

enum intvkbd_motor_state {
	DRIVE_STOPPED,
	DRIVE_EJECT,
	DRIVE_PLAY,
	DRIVE_REWIND,
	DRIVE_FF,
};
	
// ======================> based on cassette_image_device

class intvkbd_tapedrive_device :   public device_t,
								   public device_image_interface
{
public:
	// construction/destruction
	intvkbd_tapedrive_device(const machine_config &mconfig, const char *tag, device_t *owner, uint32_t clock = 0);
	virtual ~intvkbd_tapedrive_device();

	// image-level overrides
	virtual image_init_result call_load() override;
	virtual image_init_result call_create(int format_type, util::option_resolution *format_options) override;
	virtual void call_unload() override;
	virtual std::string call_display() override;
	//virtual const software_list_loader &get_software_list_loader() const override { return image_software_list_loader::instance(); }

	virtual iodevice_t image_type() const noexcept override { return IO_CASSETTE; }

	virtual bool is_readable()  const noexcept override { return 1; }
	virtual bool is_writeable() const noexcept override { return 1; }
	virtual bool is_creatable() const noexcept override { return 1; }
	virtual bool must_be_loaded() const noexcept override { return 0; }
	virtual bool is_reset_on_load() const noexcept override { return 0; }
	virtual const char *image_interface() const noexcept override { return m_interface; }
	virtual const char *file_extensions() const noexcept override { return m_extension_list; }

	// specific implementation

	double input();
	void output(double value);

	cassette_image* get_image() { return m_cassette.get(); }
	double get_position();
	double get_length();
	
	/* read functions */
	bool get_read_data(void);
	bool get_ready(void);
	bool get_leader_detect(void);
	bool get_tape_missing(void);
	bool get_playing(void);
	bool get_no_data(void);
	
	/* write functions */
	void set_motor_enable(bool motor_enable);
	void set_motor_forward(bool motor_forware);
	void set_motor_fast(bool motor_fast);
	void set_write_mode(bool write_mode);
	void set_audio_b_mute(bool audio_b_mute);
	void set_audio_a_mute(bool audio_a_mute);
	void set_channel(bool channel);
	void set_erase(bool erase);
	void set_write_data(bool data);
	
	auto int_callback() { return m_tape_int_cb.bind(); }
	
protected:
	enum intvkbd_motor_state get_motor_state(void);
	bool is_motor_on();
	void set_channel_internal(int channel);
	void update_motor_state(uint8_t motor_state);
	void update_energy(int16_t sample);
	void update_read_bit(int16_t sample);
	void update();
	void process_read_bit(bool bit);

	// device-level overrides
	virtual void device_config_complete() override;
	virtual void device_start() override;
	virtual void device_timer(emu_timer &timer, device_timer_id id, int param, void *ptr) override;
	virtual const bool use_software_list_file_extension_for_filetype() const override { return true; }

private:
	cassette_image::ptr m_cassette;
	double          m_position;
	double          m_position_time;
	int32_t         m_value;
	int             m_channel;
	double          m_speed; // speed multiplier for tape speeds other than standard 1.875ips (used in adam driver)
	int             m_direction; // direction select
	char            m_extension_list[256];
	const struct cassette_image::Format*    const *m_formats;
	const struct cassette_image::Options    *m_create_opts;
	const char *                    m_interface;

	/* write state */
	uint8_t m_motor_state;
	bool m_writing;
	bool m_audio_b_mute;
	bool m_audio_a_mute;
	bool m_channel_select;
	bool m_erase;
	bool m_write_data;
	
	devcb_write_line m_tape_int_cb;           // This will be called whenever a new data bit is read
	bool m_read_data;
	
    int16_t energy_level;
    int16_t decode_buffer[2];
    
	image_init_result internal_load(bool is_create);
};

// device type definition
DECLARE_DEVICE_TYPE(INTVKBD_TAPEDRIVE, intvkbd_tapedrive_device)

#endif // MAME_MACHINE_INTVKBD_TAPEDRIVE_H
