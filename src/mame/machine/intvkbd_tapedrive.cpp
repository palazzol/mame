// license:BSD-3-Clause
// copyright-holders:Nathan Woods, Miodrag Milanovic
/*********************************************************************

    cassette.cpp

    Interface to the cassette image abstraction code

*********************************************************************/

#include "emu.h"
#include "formats/imageutl.h"
#include "formats/intvkbd_cas.h"
#include "intvkbd_tapedrive.h"
#include "ui/uimain.h"

#define VERBOSE 0
#include "logmacro.h"

/* The nominal bit rate is 3000bps.  We update the tape at a faster rate to minimize latency */
#define INTVKBD_TAPEDRIVE_UPDATE_RATE (24000)

enum intvkbd_motor_state intvkbd_tapedrive_device::get_motor_state()
{
	switch(m_motor_state) {
		case 0:
		case 1:
		case 2:
		case 3:
			return DRIVE_STOPPED;
		break;
		case 4:
			return DRIVE_EJECT;
		break;
		case 5:
			return DRIVE_PLAY;
		break;
		case 6:
			return DRIVE_REWIND;
		break;
		case 7:
			return DRIVE_FF;
		break;
	}
	return DRIVE_STOPPED;  // can't happen
}

// device type definition
DEFINE_DEVICE_TYPE(INTVKBD_TAPEDRIVE, intvkbd_tapedrive_device, "intvkbd_tapedrive_image", "Intellivision KC Tape Drive")

//-------------------------------------------------
//  intvkbd_tapedrive_device - constructor
//-------------------------------------------------

intvkbd_tapedrive_device::intvkbd_tapedrive_device(const machine_config &mconfig, const char *tag, device_t *owner, uint32_t clock)
	: device_t(mconfig, INTVKBD_TAPEDRIVE, tag, owner, clock),
	device_image_interface(mconfig, *this),
	m_cassette(nullptr),
	m_position(0),
	m_position_time(0),
	m_value(0),
	m_channel(2),
	m_speed(0),
	m_direction(0),
	m_formats(intvkbd_cassette_formats),
	m_create_opts(nullptr),
	m_interface(nullptr),
	m_motor_state(0),
	m_writing(false),
	m_audio_b_mute(false),
	m_audio_a_mute(false),
	m_channel_select(false),
	m_erase(false),
	m_write_data(false),
	m_tape_int_cb(*this)
{
}

//-------------------------------------------------
//  intvkbd_tapedrive_device - destructor
//-------------------------------------------------

intvkbd_tapedrive_device::~intvkbd_tapedrive_device()
{
}

//-------------------------------------------------
//  device_config_complete - perform any
//  operations now that the configuration is
//  complete
//-------------------------------------------------

void intvkbd_tapedrive_device::device_config_complete()
{
	m_extension_list[0] = '\0';
	for (int i = 0; m_formats[i]; i++ )
		image_specify_extension( m_extension_list, 256, m_formats[i]->extensions );
}

void intvkbd_tapedrive_device::device_timer(emu_timer &timer, device_timer_id id, int param, void *ptr)
{
	// TBD
	update();
	timer_set(attotime::from_hz(INTVKBD_TAPEDRIVE_UPDATE_RATE));
}

/*********************************************************************
    cassette IO
*********************************************************************/

bool intvkbd_tapedrive_device::is_motor_on()
{
	if ((get_motor_state() == DRIVE_STOPPED) || (get_motor_state() == DRIVE_EJECT))
		return false;
	else
		return true;
}

void intvkbd_tapedrive_device::update_energy(int16_t sample)
{
	const double ALPHA = 0.9;
	//osd_printf_info("%d ",abs(sample_buf[i]));
	energy_level = abs(sample)*ALPHA + (1.0-ALPHA)*energy_level;
	//osd_printf_info("energy_level = %d\n",energy_level);
}

void intvkbd_tapedrive_device::process_read_bit(bool bit)
{
	m_read_data = bit;
	// Do tape interrupt
	if (!m_tape_int_cb.isnull()) {
		m_tape_int_cb(0);
		//osd_printf_info("TAPE IRQ\n");
	} else
		LOG("m_tape_int_cb not set!\n");
}

void intvkbd_tapedrive_device::update_read_bit(int16_t sample)
{
	static int16_t last_sample;
	if (((sample >= 0) && (last_sample < 0)) ||
	    ((sample < 0) && (last_sample >= 0))) {
		//osd_printf_info("t %d %d ",decode_buffer[0],decode_buffer[1]);
		if (abs(decode_buffer[0] - 16) < 2) { // decode bit 0
			//osd_printf_info("0");
			process_read_bit(false);
			decode_buffer[0] = 0;
			decode_buffer[1] = 0;
		} else if ((abs(decode_buffer[0] - 8) < 2) &&
		           (abs(decode_buffer[1] - 8) < 2)) { // decode bit 1
		    //osd_printf_info("1");
			process_read_bit(true);
			decode_buffer[0] = 0;
			decode_buffer[1] = 0;
		} else {
			//osd_printf_info("X");
			decode_buffer[1] = decode_buffer[0];
			decode_buffer[0] = 0;
		}
	} else {
		if (decode_buffer[0] > 20) {
			process_read_bit(0);  // fake bit, just for the irq, pll is not synced
			decode_buffer[0] = 0;
			decode_buffer[1] = 0;
		}
	}
	decode_buffer[0] = decode_buffer[0] + 1;
	last_sample = sample;
}

void intvkbd_tapedrive_device::update()
{
	static int16_t sample_buf[10];

	double cur_time = device().machine().time().as_double();
	if (is_motor_on())
	{
		double new_position = m_position + (cur_time - m_position_time)*m_speed*m_direction;
		double length = get_length();
		if (new_position > length)
			new_position = length;
		else if (new_position < 0)
			new_position = 0;

		if ((get_motor_state() == DRIVE_PLAY) && (m_writing)) {
			//cassette_put_sample(m_cassette, m_channel, m_position, new_position - m_position, m_value);
		}
		else if ((get_motor_state() == DRIVE_PLAY) ||
		    (get_motor_state() == DRIVE_FF) ||
		    (get_motor_state() == DRIVE_REWIND)) {
			if ( m_cassette ) {
				int num_samples;
				if (m_speed > 0) {
					num_samples = (new_position-m_position)*48000;
					m_cassette->get_samples(m_channel, m_position, new_position-m_position,
										 num_samples, 2, sample_buf, cassette_image::WAVEFORM_16BIT);
					for(int i=0; i<num_samples; i++) {
						update_energy(sample_buf[i]);
					}
				} else {
					num_samples = (m_position-new_position)*48000;
					m_cassette->get_samples(m_channel, new_position, m_position-new_position,
									 num_samples, 2, sample_buf, cassette_image::WAVEFORM_16BIT);
					for(int i=num_samples-1; i>=0; i--) {
						update_energy(sample_buf[i]);
					}
				}
				if (get_motor_state() == DRIVE_PLAY) {
					//osd_printf_info("s ");
					for(int i=0; i<num_samples; i++) {
						//osd_printf_info("%d ",sample_buf[i]);
					}
					//osd_printf_info("\n");
					for(int i=0; i<num_samples; i++) {
						update_read_bit(sample_buf[i]);
						if (num_samples == 1)
							update_read_bit(sample_buf[0]);
					}
				}
			}
		}
		m_position = new_position;
	}
	m_position_time = cur_time;
}

double intvkbd_tapedrive_device::input()
{
	int32_t sample;
	double double_value;

	update();
	sample = m_value;
	double_value = sample / ((double) 0x7FFFFFFF);

	LOG("cassette_input(): time_index=%g value=%g\n", m_position, double_value);

	return double_value;
}

void intvkbd_tapedrive_device::output(double value)
{
	if ((get_motor_state() == DRIVE_PLAY) && m_writing && (m_value != value))
	{
		update();

		value = std::min(value, 1.0);
		value = std::max(value, -1.0);

		m_value = (int32_t) (value * 0x7FFFFFFF);
	}
}

double intvkbd_tapedrive_device::get_position()
{
	double position = m_position;

	if (is_motor_on())
		position += (device().machine().time().as_double() - m_position_time)*m_speed*m_direction;
	return position;
}

double intvkbd_tapedrive_device::get_length()
{
	if (!m_cassette) return 0.0;

	cassette_image::Info info = m_cassette->get_info();
	return ((double) info.sample_count) / info.sample_frequency;
}

bool intvkbd_tapedrive_device::get_read_data(void)
{
	//osd_printf_info("READ DATA %d\n",m_read_data?1:0);
	return m_read_data;
}

bool intvkbd_tapedrive_device::get_ready(void)
{
	return true;  // TBD for now
}

bool intvkbd_tapedrive_device::get_leader_detect(void)
{
	double length = get_length();
	double position = get_position();
	//osd_printf_info("intvkbd_tapedrive: position=%g\n",position);
	if ((position <= 0.0) || (position >= length)) {
		//osd_printf_info("intvkbd_tapedrive: Leader Detected\n");
		return true;
	} else {
		//osd_printf_info("intvkbd_tapedrive: Leader NOT Detected\n");
		return false;
	}
}

bool intvkbd_tapedrive_device::get_tape_missing(void)
{
	if (!m_cassette) return true;
	return false;
}

bool intvkbd_tapedrive_device::get_playing(void)
{
	if (energy_level < 1000)   // TBD for now
		return true;
	else {
		//osd_printf_info("p energy_level=%d\n",energy_level);
		return false;
	}
}

bool intvkbd_tapedrive_device::get_no_data(void)
{
	if (energy_level < 1000)   // TBD for now
		return true;
	else {
		//osd_printf_info("nd energy_level=%d\n",energy_level);
		return false;
	}
}

void intvkbd_tapedrive_device::set_channel_internal(int channel)
{
	if (channel != m_channel)
		update();
	m_channel = channel;
}

void intvkbd_tapedrive_device::update_motor_state(uint8_t motor_state)
{
	if (motor_state == m_motor_state)
		return;
	if ((motor_state < 4) && (m_motor_state < 4)) {
		m_motor_state = motor_state;
		return;
	}
	update();
	m_motor_state = motor_state;

	if (m_motor_state <= 4)
		m_speed = 0;
	if (m_motor_state == 5)
		m_speed = 1;
	else if (m_motor_state == 6)
		m_speed = -4;
	else
		m_speed = 2;

	const char *name[8] = { "STOP", "STOP", "STOP", "STOP", "EJECT", "PLAY", "REWIND", "FFWD" };
	LOG("intvkbd_tapedrive: motor_state=%s\n",name[m_motor_state]);
	LOG("intvkbd_tapedrive: position=%g\n",get_position());
}

void intvkbd_tapedrive_device::set_motor_enable(bool motor_enable)
{
	uint8_t motor_state = m_motor_state;
	if (motor_enable)
		motor_state |= 4;
	else
		motor_state &= 7-4;
	update_motor_state(motor_state);
}

void intvkbd_tapedrive_device::set_motor_forward(bool motor_forward)
{
	uint8_t motor_state = m_motor_state;
	if (motor_forward)
		motor_state |= 2;
	else
		motor_state &= 7-2;
	update_motor_state(motor_state);
}

void intvkbd_tapedrive_device::set_motor_fast(bool motor_fast)
{
	uint8_t motor_state = m_motor_state;
	if (motor_fast)
		motor_state |= 1;
	else
		motor_state &= 7-1;
	update_motor_state(motor_state);
}

void intvkbd_tapedrive_device::set_write_mode(bool wm)
{
	if (wm == m_writing)
		return;
	update();
	// TBD support for audio recording here is last set_channel_internal(true);
	m_writing = wm;
}

void intvkbd_tapedrive_device::set_audio_b_mute(bool audio_b_mute)
{
}

void intvkbd_tapedrive_device::set_audio_a_mute(bool audio_a_mute)
{
}

void intvkbd_tapedrive_device::set_channel(bool cs)
{
	// "Tape Drive Control: Mode"
	// If read mode:
	//  0=Read Channel B Data, 1 = Read Channel A Data
	// If write mode:
	//  0=Write Channel B data, 1 = Record Channel B Audio

	if (!m_writing) {  // read mode
		if (cs) {
			set_channel_internal(1);
		} else {
			set_channel_internal(2);
		}
	} else {
		if (cs) {
			// recording audio to track 3
		} else {
			set_channel_internal(2); // recording data
		}
	}
}

void intvkbd_tapedrive_device::set_erase(bool erase)
{
}

void intvkbd_tapedrive_device::set_write_data(bool data)
{
}

/*********************************************************************
    cassette device init/load/unload/specify
*********************************************************************/

void intvkbd_tapedrive_device::device_start()
{
	/* set to default state */
	m_cassette = nullptr;
	//m_state = m_default_state;   TBD!!!
	m_value = 0;

	m_tape_int_cb.resolve();

	timer_alloc();
	timer_set(attotime::from_hz(INTVKBD_TAPEDRIVE_UPDATE_RATE));
}

image_init_result intvkbd_tapedrive_device::call_create(int format_type, util::option_resolution *format_options)
{
	return internal_load(true);
}

image_init_result intvkbd_tapedrive_device::call_load()
{
	return internal_load(false);
}

image_init_result intvkbd_tapedrive_device::internal_load(bool is_create)
{
	cassette_image::error err;
	device_image_interface *image = nullptr;
	interface(image);

	if (is_create)
	{
		// creating an image
		err = cassette_image::create((void *)image, &image_ioprocs, &cassette_image::wavfile_format, m_create_opts, cassette_image::FLAG_READWRITE | cassette_image::FLAG_SAVEONEXIT, m_cassette);
		if (err != cassette_image::error::SUCCESS)
			goto error;
	}
	else
	{
		// opening an image
		bool retry;
		do
		{
			// we probably don't want to retry...
			retry = false;

			// try opening the cassette
			int cassette_flags = is_readonly()
				? cassette_image::FLAG_READONLY
				: (cassette_image::FLAG_READWRITE | cassette_image::FLAG_SAVEONEXIT);
			err = cassette_image::open_choices((void *)image, &image_ioprocs, filetype(), m_formats, cassette_flags, m_cassette);

			// special case - if we failed due to readwrite not being supported, make the image be read only and retry
			if (err == cassette_image::error::READ_WRITE_UNSUPPORTED)
			{
				make_readonly();
				retry = true;
			}
		}
		while(retry);

		if (err != cassette_image::error::SUCCESS)
			goto error;
	}

	/* set to default state, but only change the UI state */
	//change_state(m_default_state, INTVKBD_CASSETTE_MASK_UISTATE);   ??TBD

	/* reset the position */
	m_position = 0.0;
	m_position_time = device().machine().time().as_double();

	/* default channel to 0, speed multiplier to 1 */
	m_channel = 2;
	m_speed = 1;
	m_direction = 1;

	return image_init_result::PASS;

error:
	image_error_t imgerr = IMAGE_ERROR_UNSPECIFIED;
	switch(err)
	{
		case cassette_image::error::INTERNAL:
			imgerr = IMAGE_ERROR_INTERNAL;
			break;
		case cassette_image::error::UNSUPPORTED:
			imgerr = IMAGE_ERROR_UNSUPPORTED;
			break;
		case cassette_image::error::OUT_OF_MEMORY:
			imgerr = IMAGE_ERROR_OUTOFMEMORY;
			break;
		case cassette_image::error::INVALID_IMAGE:
			imgerr = IMAGE_ERROR_INVALIDIMAGE;
			break;
		default:
			imgerr = IMAGE_ERROR_UNSPECIFIED;
			break;
	}
	image->seterror(imgerr, "" );
	return image_init_result::FAIL;
}



void intvkbd_tapedrive_device::call_unload()
{
	/* if we are recording, write the value to the image */
	if ((get_motor_state() == DRIVE_PLAY) && m_writing)
		update();

	/* close out the cassette */
	m_cassette->save();
	m_cassette = nullptr;

	/* set to default state, but only change the UI state */
	//change_state(INTVKBD_CASSETTE_STOPPED, INTVKBD_CASSETTE_MASK_UISTATE); /// ??TBD
}


//-------------------------------------------------
//  display a small tape animation, with the
//  current position in the tape image
//-------------------------------------------------

std::string intvkbd_tapedrive_device::call_display()
{
	const int ANIMATION_FPS = 1;

	std::string result;

	// only show the image when a cassette is loaded and the motor is on
	if (exists() && is_motor_on())
	{
		int n;
		double position, length;
		//intvkbd_tapedrive_state uistate;
		static const char *shapes[] = { u8"\u2500", u8"\u2572", u8"\u2502", u8"\u2571" };

		// figure out where we are in the cassette
		position = get_position();
		length = get_length();
		//uistate = (intvkbd_tapedrive_state)(get_state() & INTVKBD_CASSETTE_MASK_UISTATE);  ??TBD

		// choose which frame of the animation we are at
		n = ((int)position / ANIMATION_FPS) % ARRAY_LENGTH(shapes);

		// play or record
		const char *status_icon = (get_motor_state() == DRIVE_PLAY)
			? u8"\u25BA"
			: u8"\u25CF";

		// Since you can have anything in a BDF file, we will use crude ascii characters instead
		result = string_format("%s %s %02d:%02d (%04d) [%02d:%02d (%04d)]",
			shapes[n],                  // animation
			status_icon,                // play or record
			((int)position / 60),
			((int)position % 60),
			(int)position,
			((int)length / 60),
			((int)length % 60),
			(int)length);

		// make sure tape stops at end when playing
		if ((get_motor_state() == DRIVE_PLAY))
		{
			if (m_cassette)
			{
				if (get_position() > get_length())
				{
					//m_state = (intvkbd_tapedrive_state)((m_state & ~INTVKBD_CASSETTE_MASK_UISTATE) | INTVKBD_CASSETTE_STOPPED);  ??TBD
				}
			}
		}
	}
	return result;
}
