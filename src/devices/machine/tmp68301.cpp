// license:BSD-3-Clause
// copyright-holders:Luca Elia
/***************************************************************************

    TMP68301 basic emulation + Interrupt Handling

    The Toshiba TMP68301 is a 68HC000 + serial I/O, parallel I/O,
    3 timers, address decoder, wait generator, interrupt controller,
    all integrated in a single chip.

    TODO:
    - Interrupt generation: handle pending / in-service mechanisms
    - Parallel port: handle timing latency
    - Serial port: not done at all
    - (and many other things)

***************************************************************************/

#include "emu.h"
#include "machine/tmp68301.h"

const device_type TMP68301 = &device_creator<tmp68301_device>;
const device_type TMP68301_SERIAL = &device_creator<tmp68301_serial_device>;
const device_type TMP68301_RS232 = &device_creator<tmp68301_rs232_device>;

static ADDRESS_MAP_START( tmp68301_regs, AS_0, 16, tmp68301_device )
//  AM_RANGE(0x000,0x3ff) AM_RAM
	AM_RANGE(0x094,0x095) AM_READWRITE (imr_r,   imr_w)
	AM_RANGE(0x098,0x099) AM_READWRITE (iisr_r,  iisr_w)

	/* Parallel Port */
	AM_RANGE(0x100,0x101) AM_READWRITE (pdir_r,  pdir_w)
	AM_RANGE(0x10a,0x10b) AM_READWRITE (pdr_r,   pdr_w)

	/* Serial Port */
	AM_RANGE(0x180,0x181) AM_DEVREADWRITE8("ser0", tmp68301_serial_device, smr_r,  smr_w,  0x00ff)
	AM_RANGE(0x182,0x183) AM_DEVREADWRITE8("ser0", tmp68301_serial_device, scmr_r, scmr_w, 0x00ff)
	AM_RANGE(0x184,0x185) AM_DEVREADWRITE8("ser0", tmp68301_serial_device, sbrr_r, sbrr_w, 0x00ff)
	AM_RANGE(0x186,0x187) AM_DEVREADWRITE8("ser0", tmp68301_serial_device, ssr_r,  ssr_w,  0x00ff)
	AM_RANGE(0x188,0x189) AM_DEVREADWRITE8("ser0", tmp68301_serial_device, sdr_r,  sdr_w,  0x00ff)

	AM_RANGE(0x18c,0x18d) AM_READWRITE8(spr_r,   spr_w,   0x00ff)
	AM_RANGE(0x18e,0x18f) AM_READWRITE8(scr_r,   scr_w,   0x00ff)

	AM_RANGE(0x190,0x191) AM_DEVREADWRITE8("ser1", tmp68301_serial_device, smr_r,  smr_w,  0x00ff)
	AM_RANGE(0x192,0x193) AM_DEVREADWRITE8("ser1", tmp68301_serial_device, scmr_r, scmr_w, 0x00ff)
	AM_RANGE(0x194,0x195) AM_DEVREADWRITE8("ser1", tmp68301_serial_device, sbrr_r, sbrr_w, 0x00ff)
	AM_RANGE(0x196,0x197) AM_DEVREADWRITE8("ser1", tmp68301_serial_device, ssr_r,  ssr_w,  0x00ff)
	AM_RANGE(0x198,0x199) AM_DEVREADWRITE8("ser1", tmp68301_serial_device, sdr_r,  sdr_w,  0x00ff)

	AM_RANGE(0x1a0,0x1a1) AM_DEVREADWRITE8("ser2", tmp68301_serial_device, smr_r,  smr_w,  0x00ff)
	AM_RANGE(0x1a2,0x1a3) AM_DEVREADWRITE8("ser2", tmp68301_serial_device, scmr_r, scmr_w, 0x00ff)
	AM_RANGE(0x1a4,0x1a5) AM_DEVREADWRITE8("ser2", tmp68301_serial_device, sbrr_r, sbrr_w, 0x00ff)
	AM_RANGE(0x1a6,0x1a7) AM_DEVREADWRITE8("ser2", tmp68301_serial_device, ssr_r,  ssr_w,  0x00ff)
	AM_RANGE(0x1a8,0x1a9) AM_DEVREADWRITE8("ser2", tmp68301_serial_device, sdr_r,  sdr_w,  0x00ff)
ADDRESS_MAP_END

static MACHINE_CONFIG_FRAGMENT( tmp68301 )
	MCFG_TMP68301_RS232_ADD ("ser0")
	MCFG_TMP68301_SERIAL_ADD("ser1")
	MCFG_TMP68301_SERIAL_ADD("ser2")
MACHINE_CONFIG_END

// IRQ Mask register, 0x94
READ16_MEMBER(tmp68301_device::imr_r)
{
	return m_imr;
}

WRITE16_MEMBER(tmp68301_device::imr_w)
{
	COMBINE_DATA(&m_imr);
}

// IRQ In-Service Register
READ16_MEMBER(tmp68301_device::iisr_r)
{
	return m_iisr;
}

WRITE16_MEMBER(tmp68301_device::iisr_w)
{
	COMBINE_DATA(&m_iisr);
}

/* Parallel direction: 1 = output, 0 = input */
READ16_MEMBER(tmp68301_device::pdir_r)
{
	return m_pdir;
}

WRITE16_MEMBER(tmp68301_device::pdir_w)
{
	COMBINE_DATA(&m_pdir);
}

READ16_MEMBER(tmp68301_device::pdr_r)
{
	return (m_in_parallel_cb(0) & ~m_pdir) | (m_pdr & m_pdir);
}

WRITE16_MEMBER(tmp68301_device::pdr_w)
{
	UINT16 old = m_pdr;
	COMBINE_DATA(&m_pdr);
	m_pdr = (old & ~m_pdir) | (m_pdr & m_pdir);
	m_out_parallel_cb(0, m_pdr, mem_mask);
}


tmp68301_device::tmp68301_device(const machine_config &mconfig, const char *tag, device_t *owner, UINT32 clock)
	: device_t(mconfig, TMP68301, "TMP68301", tag, owner, clock, "tmp68301", __FILE__),
		device_memory_interface(mconfig, *this),
		m_in_parallel_cb(*this),
		m_out_parallel_cb(*this),
	    m_ser0(*this, "ser0"),
	    m_ser1(*this, "ser1"),
	    m_ser2(*this, "ser2"),
		m_imr(0),
		m_iisr(0),
		m_pdir(0),
		m_pdr(0),
		m_scr(0),
		m_space_config("regs", ENDIANNESS_LITTLE, 16, 10, 0, nullptr, *ADDRESS_MAP_NAME(tmp68301_regs))
{
	memset(m_regs, 0, sizeof(m_regs));
	memset(m_IE, 0, sizeof(m_IE));
	memset(m_irq_vector, 0, sizeof(m_irq_vector));
}

void tmp68301_device::set_cpu_tag(const char *tag)
{
	m_cpu_tag = tag;
}

//-------------------------------------------------
//  device_start - device-specific startup
//-------------------------------------------------

void tmp68301_device::device_start()
{
	int i;
	for (i = 0; i < 3; i++)
		m_tmp68301_timer[i] = machine().scheduler().timer_alloc(timer_expired_delegate(FUNC(tmp68301_device::timer_callback), this));

	m_in_parallel_cb.resolve_safe(0);
	m_out_parallel_cb.resolve_safe();

	save_item(NAME(m_regs));
	save_item(NAME(m_IE));
	save_item(NAME(m_irq_vector));
	save_item(NAME(m_imr));
	save_item(NAME(m_iisr));
	save_item(NAME(m_pdr));
	save_item(NAME(m_scr));
	save_item(NAME(m_pdir));

	m_cpu = machine().device<m68000_device>(m_cpu_tag);
}

//-------------------------------------------------
//  device_reset - device-specific reset
//-------------------------------------------------

void tmp68301_device::device_reset()
{
	int i;

	for (i = 0; i < 3; i++)
		m_IE[i] = 0;

	m_imr = 0x7f7; // mask all irqs
	m_scr = 0x00;
	m_spr = 0x00;

	double prescaled_clock = double(m_cpu->unscaled_clock())/256;
	m_ser0->set_prescaled_clock(prescaled_clock);
	m_ser1->set_prescaled_clock(prescaled_clock);
	m_ser2->set_prescaled_clock(prescaled_clock);	
}

//-------------------------------------------------
//  memory_space_config - return a description of
//  any address spaces owned by this device
//-------------------------------------------------

const address_space_config *tmp68301_device::memory_space_config(address_spacenum spacenum) const
{
	return (spacenum == AS_0) ? &m_space_config : nullptr;
}

machine_config_constructor tmp68301_device::device_mconfig_additions() const
{
	return MACHINE_CONFIG_NAME(tmp68301);
}

//**************************************************************************
//  INLINE HELPERS
//**************************************************************************

//-------------------------------------------------
//  read_byte - read a byte at the given address
//-------------------------------------------------

inline UINT16 tmp68301_device::read_word(offs_t address)
{
	return space(AS_0).read_word(address << 1);
}

//-------------------------------------------------
//  write_byte - write a byte at the given address
//-------------------------------------------------

inline void tmp68301_device::write_word(offs_t address, UINT16 data)
{
	space(AS_0).write_word(address << 1, data);
}

IRQ_CALLBACK_MEMBER(tmp68301_device::irq_callback)
{
	int vector = m_irq_vector[irqline];
//  logerror("%s: irq callback returns %04X for level %x\n",machine.describe_context(),vector,int_level);
	return vector;
}

TIMER_CALLBACK_MEMBER( tmp68301_device::timer_callback )
{
	int i = param;
	UINT16 TCR  =   m_regs[(0x200 + i * 0x20)/2];
	UINT16 ICR  =   m_regs[0x8e/2+i];    // Interrupt Controller Register (ICR7..9)
	UINT16 IVNR =   m_regs[0x9a/2];      // Interrupt Vector Number Register (IVNR)

//  logerror("s: callback timer %04X, j = %d\n",machine.describe_context(),i,tcount);

	if  (   (TCR & 0x0004) &&   // INT
			!(m_imr & (0x100<<i))
		)
	{
		int level = ICR & 0x0007;

		// Interrupt Vector Number Register (IVNR)
		m_irq_vector[level]  =   IVNR & 0x00e0;
		m_irq_vector[level]  +=  4+i;

		m_cpu->set_input_line(level,HOLD_LINE);
	}

	if (TCR & 0x0080)   // N/1
	{
		// Repeat
		update_timer(i);
	}
	else
	{
		// One Shot
	}
}

void tmp68301_device::update_timer( int i )
{
	UINT16 TCR  =   m_regs[(0x200 + i * 0x20)/2];
	UINT16 MAX1 =   m_regs[(0x204 + i * 0x20)/2];
	UINT16 MAX2 =   m_regs[(0x206 + i * 0x20)/2];

	int max = 0;
	attotime duration = attotime::zero;

	m_tmp68301_timer[i]->adjust(attotime::never,i);

	// timers 1&2 only
	switch( (TCR & 0x0030)>>4 )                     // MR2..1
	{
	case 1:
		max = MAX1;
		break;
	case 2:
		max = MAX2;
		break;
	}

	switch ( (TCR & 0xc000)>>14 )                   // CK2..1
	{
	case 0: // System clock (CLK)
		if (max)
		{
			int scale = (TCR & 0x3c00)>>10;         // P4..1
			if (scale > 8) scale = 8;
			duration = attotime::from_hz(m_cpu->unscaled_clock()) * ((1 << scale) * max);
		}
		break;
	}

//  logerror("%s: TMP68301 Timer %d, duration %lf, max %04X\n",machine().describe_context(),i,duration,max);

	if (!(TCR & 0x0002))                // CS
	{
		if (duration != attotime::zero)
			m_tmp68301_timer[i]->adjust(duration,i);
		else
			logerror("%s: TMP68301 error, timer %d duration is 0\n",machine().describe_context(),i);
	}
}

/* Update the IRQ state based on all possible causes */
void tmp68301_device::update_irq_state()
{
	int i;

	/* Take care of external interrupts */

	UINT16 IVNR =   m_regs[0x9a/2];      // Interrupt Vector Number Register (IVNR)

	for (i = 0; i < 3; i++)
	{
		if  (   (m_IE[i]) &&
				!(m_imr & (1<<i))
			)
		{
			UINT16 ICR  =   m_regs[0x80/2+i];    // Interrupt Controller Register (ICR0..2)

			// Interrupt Controller Register (ICR0..2)
			int level = ICR & 0x0007;

			// Interrupt Vector Number Register (IVNR)
			m_irq_vector[level]  =   IVNR & 0x00e0;
			m_irq_vector[level]  +=  i;

			m_IE[i] = 0;     // Interrupts are edge triggerred

			m_cpu->set_input_line(level,HOLD_LINE);
		}
	}
}

READ16_MEMBER( tmp68301_device::regs_r )
{
	return read_word(offset);
}

WRITE16_MEMBER( tmp68301_device::regs_w )
{
	COMBINE_DATA(&m_regs[offset]);

	write_word(offset,m_regs[offset]);

	if (!ACCESSING_BITS_0_7)    return;

//  logerror("CPU #0 PC %06X: TMP68301 Reg %04X<-%04X & %04X\n",space.device().safe_pc(),offset*2,data,mem_mask^0xffff);

	switch( offset * 2 )
	{
		// Timers
		case 0x200:
		case 0x220:
		case 0x240:
		{
			int i = ((offset*2) >> 5) & 3;

			update_timer( i );
		}
		break;
	}
}

void tmp68301_device::external_interrupt_0()    { m_IE[0] = 1;   update_irq_state(); }
void tmp68301_device::external_interrupt_1()    { m_IE[1] = 1;   update_irq_state(); }
void tmp68301_device::external_interrupt_2()    { m_IE[2] = 1;   update_irq_state(); }


// Serial subsystem

tmp68301_serial_device::tmp68301_serial_device(const machine_config &mconfig, const char *tag, device_t *owner, UINT32 clock) :
	device_t(mconfig, TMP68301_SERIAL, "TMP68301 Serial", tag, owner, clock, "tmp68301_serial", __FILE__),
	tx_cb(*this)
{
}

tmp68301_serial_device::tmp68301_serial_device(const machine_config &mconfig, device_type type, const char *name, const char *tag, device_t *owner, UINT32 clock, const char *shortname, const char *source) :
	device_t(mconfig, type, name, tag, owner, clock, shortname, source),
	tx_cb(*this)
{
}

tmp68301_rs232_device::tmp68301_rs232_device(const machine_config &mconfig, const char *tag, device_t *owner, UINT32 clock) :
	tmp68301_serial_device(mconfig, TMP68301_SERIAL, "TMP68301 RS232", tag, owner, clock, "tmp68301_rs232", __FILE__),
	rts_cb(*this),
	dtr_cb(*this)
{
}

void tmp68301_serial_device::device_start()
{
	prescaled_clock = 0;
	clock_interval = attotime::never;
}

void tmp68301_serial_device::device_reset()
{
	smr  = 0xc2;
	scmr = 0x10;
	ssr  = 0x04;
	sbrr = 0x00;
	clock_interval = attotime::never;
}

void tmp68301_rs232_device::device_start()
{
	tmp68301_serial_device::device_start();
}

void tmp68301_rs232_device::device_reset()
{
	tmp68301_serial_device::device_reset();
}

READ8_MEMBER(tmp68301_device::scr_r)
{
	return m_scr;
}

WRITE8_MEMBER(tmp68301_device::scr_w)
{
	logerror("%s: scr_w %02x clokc=%s reset=%s serial_int=%s (%06x)\n", tag(), data,
			 data & 0x80 ? "internal" : "external",
			 data & 0x40 ? "on" : "off",
			 data & 0x01 ? "off" : "on",
			 space.device().safe_pc());

	/*
	    *--- ---- CKSE
	    --*- ---- RES
	    ---- ---* INTM
	*/

	m_scr = data & 0xa1;
	recalc_serial_clock();
}

READ8_MEMBER(tmp68301_device::spr_r)
{
	logerror("%s: spr_r (%06x)\n", tag(), space.device().safe_pc());
	return m_spr;
}

WRITE8_MEMBER(tmp68301_device::spr_w)
{
	logerror("%s: spr_w %02x (%06x)\n", tag(), data, space.device().safe_pc());
	m_spr = data;
	recalc_serial_clock();
}

void tmp68301_device::recalc_serial_clock()
{
	double prescaled_clock = m_scr & 0x20 ? 0 : m_spr ? double(m_cpu->unscaled_clock())/m_spr : double(clock())/256;
	m_ser0->set_prescaled_clock(prescaled_clock);
	m_ser1->set_prescaled_clock(prescaled_clock);
	m_ser2->set_prescaled_clock(prescaled_clock);
}

READ8_MEMBER(tmp68301_serial_device::smr_r)
{
	logerror("%s: smr_r (%06x)\n", tag(), space.device().safe_pc());
	return smr;
}

WRITE8_MEMBER(tmp68301_serial_device::smr_w)
{
	logerror("%s: smr_w %02x rx_int=%s tx_int=%s er_int=%s mode=%d%c%c (%06x)\n",
			 tag(), data,
			 data & 0x80 ? "off" : "on",
			 data & 0x02 ? "off" : "on",
			 data & 0x40 ? "off" : "on",
			 5 + ((data >> 2) & 3),
			 data & 0x10 ? data & 0x20 ? 'o' : 'e' : 'n',
			 data & 0x01 ? '2' : '1',
			 space.device().safe_pc());
	smr = data;
}

READ8_MEMBER(tmp68301_serial_device::scmr_r)
{
	logerror("%s: scmr_r (%06x)\n", tag(), space.device().safe_pc());
	return scmr;
}

WRITE8_MEMBER(tmp68301_serial_device::scmr_w)
{
	logerror("%s: scmr_w %02x ers=%s break=%s rx=%s tx=%s rts=%s dtr=%s (%06x)\n", tag(), data,
			 data & 0x10 ? "reset" : "off",
			 data & 0x08 ? "on" : "off",
			 data & 0x04 ? "on" : "off",
			 data & 0x01 ? "on" : "off",
			 data & 0x20 ? "low" : "high",
			 data & 0x02 ? "low" : "high",
			 space.device().safe_pc());
	scmr = data;
}

READ8_MEMBER(tmp68301_serial_device::sbrr_r)
{
	logerror("%s: sbrr_r (%06x)\n", tag(), space.device().safe_pc());
	return sbrr;
}

WRITE8_MEMBER(tmp68301_serial_device::sbrr_w)
{
	logerror("%s: sbrr_w %02x (%06x)\n", tag(), data, space.device().safe_pc());
	sbrr = data;
	clock_update();
}

READ8_MEMBER(tmp68301_serial_device::ssr_r)
{
	logerror("%s: ssr_r (%06x)\n", tag(), space.device().safe_pc());
	return ssr;
}

WRITE8_MEMBER(tmp68301_serial_device::ssr_w)
{
	logerror("%s: ssr_w %02x (%06x)\n", tag(), data, space.device().safe_pc());
	ssr = data;
}

READ8_MEMBER(tmp68301_serial_device::sdr_r)
{
	logerror("%s: sdr_r (%06x)\n", tag(), space.device().safe_pc());
	return 0x00;
}

WRITE8_MEMBER(tmp68301_serial_device::sdr_w)
{
	logerror("%s: sdr_w %02x (%06x)\n", tag(), data, space.device().safe_pc());
}

void tmp68301_serial_device::set_prescaled_clock(double clock)
{
	prescaled_clock = clock;
	clock_update();
}

void tmp68301_serial_device::clock_update()
{
	if(!prescaled_clock || !sbrr || (sbrr & (sbrr - 1))) {
		clock_interval = attotime::never;
		return;
	}

	double base_rate = prescaled_clock / sbrr;
	clock_interval = attotime::from_seconds(1/base_rate);
	logerror("%s: Baud rate %gHz\n", tag(), base_rate/8);
}
