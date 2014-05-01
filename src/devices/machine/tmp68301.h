// license:BSD-3-Clause
// copyright-holders:Luca Elia
#ifndef TMP68301_H
#define TMP68301_H

#include "cpu/m68000/m68000.h"

//**************************************************************************
//  INTERFACE CONFIGURATION MACROS
//**************************************************************************

#define MCFG_TMP68301_ADD(_tag, _cpu) \
	MCFG_DEVICE_ADD( _tag, TMP68301, 0 ) \
	downcast<tmp68301_device *>(device)->set_cpu_tag(_cpu);

/* TODO: serial ports, frequency & hook it up with m68k */
#define MCFG_TMP68301_IN_PARALLEL_CB(_devcb) \
	devcb = &tmp68301_device::set_in_parallel_callback(*device, DEVCB_##_devcb);

#define MCFG_TMP68301_OUT_PARALLEL_CB(_devcb) \
	devcb = &tmp68301_device::set_out_parallel_callback(*device, DEVCB_##_devcb);

#define MCFG_TMP68301_SERIAL_ADD( _tag ) \
	MCFG_DEVICE_ADD( _tag, TMP68301_SERIAL, 0 )

#define MCFG_TMP68301_RS232_ADD( _tag ) \
	MCFG_DEVICE_ADD( _tag, TMP68301_RS232, 0 )

#define MCFG_TMP68301_SERIAL_TX_CALLBACK(_devcb) \
	devcb = &tmp68301_serial_device::set_tx_cb(*device, DEVCB_##_devcb);

#define MCFG_TMP68301_SERIAL_RTS_CALLBACK(_devcb) \
	devcb = &tmp68301_rs232_device::set_rts_cb(*device, DEVCB_##_devcb);

#define MCFG_TMP68301_SERIAL_DTR_CALLBACK(_devcb) \
	devcb = &tmp68301_rs232_device::set_str_cb(*device, DEVCB_##_devcb);

//**************************************************************************
//  TYPE DEFINITIONS
//**************************************************************************

class tmp68301_serial_device : public device_t
{
public:
	tmp68301_serial_device(const machine_config &mconfig, const char *tag, device_t *owner, UINT32 clock);
	tmp68301_serial_device(const machine_config &mconfig, device_type type, const char *name, const char *tag, device_t *owner, UINT32 clock, const char *shortname, const char *source);

	DECLARE_READ8_MEMBER(smr_r);
	DECLARE_WRITE8_MEMBER(smr_w);
	DECLARE_READ8_MEMBER(scmr_r);
	DECLARE_WRITE8_MEMBER(scmr_w);
	DECLARE_READ8_MEMBER(sbrr_r);
	DECLARE_WRITE8_MEMBER(sbrr_w);
	DECLARE_READ8_MEMBER(ssr_r);
	DECLARE_WRITE8_MEMBER(ssr_w);
	DECLARE_READ8_MEMBER(sdr_r);
	DECLARE_WRITE8_MEMBER(sdr_w);

	DECLARE_WRITE_LINE_MEMBER(rx_w);

	template<class _Object> static devcb_base &set_tx_cb(device_t &device, _Object object) { return downcast<tmp68301_serial_device &>(device).tx_cb.set_callback(object); }

	void set_prescaled_clock(double clock);

protected:
	devcb_write_line tx_cb;
	attotime clock_interval;
	double prescaled_clock;

	UINT8 smr, scmr, ssr, sbrr;

	virtual void device_start() override;
	virtual void device_reset() override;

	void clock_update();
};

class tmp68301_rs232_device : public tmp68301_serial_device {
public:
	tmp68301_rs232_device(const machine_config &mconfig, const char *tag, device_t *owner, UINT32 clock);

	DECLARE_WRITE_LINE_MEMBER(cts_w);
	DECLARE_WRITE_LINE_MEMBER(dsr_w);

	template<class _Object> static devcb_base &set_rts_cb(device_t &device, _Object object) { return downcast<tmp68301_rs232_device &>(device).rts_cb.set_callback(object); }
	template<class _Object> static devcb_base &set_dtr_cb(device_t &device, _Object object) { return downcast<tmp68301_rs232_device &>(device).dtr_cb.set_callback(object); }

protected:
	devcb_write_line rts_cb, dtr_cb;

	virtual void device_start() override;
	virtual void device_reset() override;
};

class tmp68301_device : public device_t,
						public device_memory_interface
{
public:
	tmp68301_device(const machine_config &mconfig, const char *tag, device_t *owner, UINT32 clock);
	~tmp68301_device() {}

	template<class _Object> static devcb_base &set_in_parallel_callback(device_t &device, _Object object) { return downcast<tmp68301_device &>(device).m_in_parallel_cb.set_callback(object); }
	template<class _Object> static devcb_base &set_out_parallel_callback(device_t &device, _Object object) { return downcast<tmp68301_device &>(device).m_out_parallel_cb.set_callback(object); }

	void set_cpu_tag(const char *tag);

	// Hardware Registers
	DECLARE_READ16_MEMBER( regs_r );
	DECLARE_WRITE16_MEMBER( regs_w );

	// Interrupts
	void external_interrupt_0();
	void external_interrupt_1();
	void external_interrupt_2();

	DECLARE_READ16_MEMBER(imr_r);
	DECLARE_WRITE16_MEMBER(imr_w);
	DECLARE_READ16_MEMBER(iisr_r);
	DECLARE_WRITE16_MEMBER(iisr_w);
	DECLARE_READ16_MEMBER(pdr_r);
	DECLARE_WRITE16_MEMBER(pdr_w);
	DECLARE_READ16_MEMBER(pdir_r);
	DECLARE_WRITE16_MEMBER(pdir_w);

	DECLARE_READ8_MEMBER(spr_r);
	DECLARE_WRITE8_MEMBER(spr_w);
	DECLARE_READ8_MEMBER(scr_r);
	DECLARE_WRITE8_MEMBER(scr_w);

	IRQ_CALLBACK_MEMBER(irq_callback);
protected:
	// device-level overrides
	virtual void device_start() override;
	virtual void device_reset() override;
	virtual const address_space_config *memory_space_config(address_spacenum spacenum = AS_0) const override;
	virtual machine_config_constructor device_mconfig_additions() const override;

private:
	devcb_read16         m_in_parallel_cb;
	devcb_write16        m_out_parallel_cb;

	required_device<tmp68301_rs232_device> m_ser0;
	required_device<tmp68301_serial_device> m_ser1;
	required_device<tmp68301_serial_device> m_ser2;

	const char *m_cpu_tag;
	m68000_device *m_cpu;

	// internal state
	UINT16 m_regs[0x400];

	UINT8 m_IE[3];        // 3 External Interrupt Lines
	emu_timer *m_tmp68301_timer[3];        // 3 Timers

	UINT16 m_irq_vector[8];

	TIMER_CALLBACK_MEMBER( timer_callback );
	void update_timer( int i );
	void update_irq_state();

	UINT16 m_imr;
	UINT16 m_iisr;
	UINT16 m_pdir;
	UINT16 m_pdr;

	UINT8 m_scr, m_spr;

	inline UINT16 read_word(offs_t address);
	inline void write_word(offs_t address, UINT16 data);
	const address_space_config      m_space_config;

	void recalc_serial_clock();
};

extern const device_type TMP68301;
extern const device_type TMP68301_SERIAL;
extern const device_type TMP68301_RS232;

#endif
