// license:BSD-3-Clause
// copyright-holders:Nicola Salmoria, Mike Coates, Frank Palazzolo, Aaron Giles
/***************************************************************************

    Bally Astrocade-based hardware

***************************************************************************/

#include "emu.h"
#include "includes/astrocde.h"

#include "cpu/z80/z80.h"
#include "sound/astrocde.h"
#include "video/resnet.h"

#include <cmath>


/*************************************
 *
 *  Machine setup
 *
 *************************************/

void astrocde_state::machine_start()
{
	save_item(NAME(m_ram_write_enable));
	save_item(NAME(m_input_select));

	m_input_select = 0;
}

void seawolf2_state::machine_start()
{
	astrocde_state::machine_start();

	save_item(NAME(m_port_1_last));
	save_item(NAME(m_port_2_last));

	m_port_1_last = m_port_2_last = 0xff;
}

void tenpindx_state::machine_start()
{
	astrocde_state::machine_start();

	m_lamps.resolve();
}



/*************************************
 *
 *  Constants
 *
 *************************************/

#define RNG_PERIOD      ((1 << 17) - 1)
#define VERT_OFFSET     (22)                /* pixels from top of screen to top of game area */
#define HORZ_OFFSET     (16)                /* pixels from left of screen to left of game area */

/*************************************
 *
 *  Scanline conversion
 *
 *************************************/

inline int astrocde_state::mame_vpos_to_astrocade_vpos(int scanline)
{
	scanline -= VERT_OFFSET;
	if (scanline < 0)
		scanline += 262;
	return scanline;
}



/*************************************
 *
 *  Palette initialization
 *
 *************************************/

void astrocde_state::RGB_converter_PCB(double R_minus_Y, double B_minus_Y, double Y, double Vee,
					                   double &R, double &G, double &B) const
{
	// This emulates the Midway RGB converter board
	// Inputs are voltages as they come from the custom DATA chip
	// Outputs are RGB voltages at the output pins, possibly clipping 

	// This board is based on the TBA530 RGB Matrix Preamplifier IC.  
	// Variables 'Vxx' are voltages at the pins of this IC

	const double Vzener = 5.6;  // CR1 - 1N5232B
	const double Vdiode = 0.7;	// Assumed diode drop inside TBA530

	// Resistor Values
	const double R25 = 1000.0;
	const double R16 = 1000.0;
	const double R18 = 12000.0;
	const double R13 = 2000.0;
	const double R4 = 3000.0;
	const double R10 = 1000.0;
	const double R17 = 1000.0;
	const double R5 = 3000.0;
	const double R2 = 2000.0;
	const double R3 = 3000.0;
	const double R20 = 1200.0;
	const double R19 = 620.0;
	const double R6 = 2000.0;
	const double R21 = 100.0;
	const double R22 = 1000.0;

	// Output voltages are clipped by the TBA530, and maybe arcade monitor
	const double VclipH = 2.0;
	const double VclipL = 0.0;

	// Voltage Divider at pin 12
	const double V12 = Vee * R19 / (R19 + R20);

	// Zener Diode offsets down by about 5.6V
	const double A0 = R22 / (R21 + R22);
	const double B0 = (-R22*Vzener + R21*Vee) / (R21 + R22);
	double V5 = A0*Y + B0;
	if (V5 < Vee) V5 = Vee;
	
	// How does V5 affect V4, V2, V3 ?
	// It creates an offset on them based on the internal transistor diagram
	// This is my best idea of how the TBA530 works internally

	// Internal resistor on emitter of NPN connected to V5
	double V_Rbottom = V5 - Vdiode - Vee;
	// scaling factor, assuming upper resistor is equal * (equal gains)
	double V_Rtop = V_Rbottom * 1.0;

	// This is the offset to V4, V2, and V3, if we are in equilibrium
	double Voffset_due_to_V5 = Vdiode + V_Rtop;

	// Assume G output is not clipping for now, calculate V3
	double V3 = V12 + Voffset_due_to_V5;
	// Now Calculate G, but we need lots of work
	// first we need V9
	// Kirchoff's Current Law at V9
	const double A1 = (R16 * R18) / (R25 * R16 + R25 * R18 + R16 * R18);
	const double B1 = (R25 * R16) / (R25 * R16 + R25 * R18 + R16 * R18);
	const double C1 = (R25 * R18 * Vee) / (R25 * R16 + R25 * R18 + R16 * R18);
	double V9 = A1 * R_minus_Y + B1 * V3 + C1;

	// Now we need V15
	// Kirchoff's Current Law at V15
	const double A2 = (R17 * R5) / (R10 * R17 + R10 * R5 + R17 * R5);
	const double B2 = (R10 * R17) / (R10 * R17 + R10 * R5 + R17 * R5);
	const double C2 = (R10 * R5 * Vee) / (R10 * R17 + R10 * R5 + R17 * R5);
	double V15 = A2 * B_minus_Y + B2 * V3 + C2;

	// Now, calculate G assuming no clipping
	const double A3 = (R5 * R6 + R18 * R6 + R5 * R18) / (R18 * R5);
	const double B3 = -R6 / R18;
	const double C3 = -R6 / R5;
	G = A3 * V3 + B3 * V9 + C3 * V15;

	// Check for G clipping
	bool G_clipping = false;
	if (G > VclipH) {
		G = VclipH;
		G_clipping = true;
	}
	if (G < VclipL) {
		G = VclipL;
		G_clipping = true;
	}
	// If G clips, we must recalculate V3, V9, and V15
	if (G_clipping) {
		double Veq1 = R_minus_Y * (R16 / (R16 + R25)) + Vee * (R25 / (R16 + R25));
		const double Req1 = R16 * R25 / (R16 + R25);
		double Veq2 = B_minus_Y * (R17 / (R17 + R10)) + Vee * (R10 / (R17 + R10));
		const double Req2 = R17 * R10 / (R17 + R10);
		double Num = Veq1 * R6 / (Req1 + R18) + Veq2 * R6 / (Req2 + R5) + G;
		double Den = 1.0 + R6 / (Req1 + R18) + R6 / (Req2 + R5);
		V3 = Num / Den;
		V9 = A1 * R_minus_Y + B1 * V3 + C1;
		V15 = A2 * B_minus_Y + B2 * V3 + C2;
	}

	// Now we are ready to calculate R
	double V4 = V9 + Voffset_due_to_V5;
	// Assume no clipping
	R = (1 + R13 / R4) * V4;
	// Adjust for possible clipping
	if (R > VclipH) {
		R = VclipH;
	} else if (R < VclipL) {
		R = VclipL;
	}
	V4 = R * R4 / (R4 + R13);

	// Finally we are ready to calculate B
	double V2 = V15 + Voffset_due_to_V5;
	// Assume no clipping
	B = (1 + R2 / R3) * V2;
	// Adjust for possible clipping
	if (B > VclipH) {
		B = VclipH;
	} else if (B < VclipL) {
		B = VclipL;
	}
	V4 = B * R3 / (R3 + R2);
}

void astrocde_state::astrocade_palette(palette_device &palette) const
{
	/*
	    The Astrocade has a 256 color palette: 32 colors with 8 luminance
	    values for each color. The 32 colors circle around the YUV color
	    space, with the exception of the first 8 which are grayscale.

	    We actually build a 512 entry palette with an extra bit of
	    luminance information. This is because the sparkle/star circuitry
	    on Wizard of Wor and Gorf replaces the luminance with a value
	    that has a 4-bit resolution.
	*/

	// Resistor Ladder DACs are inside the Custom DATA chip

	// Color DAC is not uniform, is is basically sinusoidal
	// These are the measured proportions of Vcc on the
	// Resistor ladder used for R-Y and B-Y
	const double color_dac[17] = {
		0.01510204082,
		0.0287755102,
		0.05816326531,
		0.106122449,
		0.1685714286,
		0.2428571429,
		0.3265306122,
		0.4142857143,
		0.5081632653,
		0.6,
		0.687755102,
		0.7693877551,
		0.8408163265,
		0.8979591837,
		0.9428571429,
		0.9673469388,
		0.9816326531
	};

	// DAC fractions of Vcc for luma min and max, step sizes are uniform
	// These are the measured proportions of Vcc on the
	// max and min values of Y
	const double luma_min = 0.27917;
	const double luma_max = 0.69375;

	// These are implemented as tables in the Custom DATA IC
	// (Values come from US Patent #4,301,503)

	const u8 R_minus_Y_lookup[32] = {
		 8, 9,10,11,12,13,14,15,
		16,15,14,13,12,11,10, 9,
		 8, 7, 6, 5, 4, 3, 2, 1,
		 0, 1, 2, 3, 4, 5, 6, 7
	};

	const u8 B_minus_Y_lookup[32] = {
		 8,15,14,13,12,11,10, 9,
		 8, 7, 6, 5, 4, 3, 2, 1,
		 0, 1, 2, 3, 4, 5, 6, 7,
		 8, 9,10,11,12,13,14,15   // Note, this 8 is slightly less than the other 2, TBD check
	};

	const double Vcc = 4.9;	   // Nominal after drops from power supply to DATA IC
	const double Vee = -4.9;   // Nominal after drops from power supply to RGB board

	// loop over color values
	for (int color = 0; color < 32; color++)
	{
		double r_minus_y = color_dac[R_minus_Y_lookup[color]] * Vcc;
		double b_minus_y = color_dac[B_minus_Y_lookup[color]] * Vcc;

		// iterate over luminance values
		for (int luma = 0; luma < 16; luma++)
		{
			double y = (luma / 15.0 * (luma_max - luma_min) + luma_min) * Vcc;
			double R, G, B;

			RGB_converter_PCB(r_minus_y, b_minus_y, y, Vee, R, G, B);

			// Can model the effect of the arcade monitor, if needed 
			const double arcade_monitor_max = 2.0;
			const double arcade_monitor_min = 0.0;

			int r = (R - arcade_monitor_min) / (arcade_monitor_max - arcade_monitor_min) * 255;
			int g = (G - arcade_monitor_min) / (arcade_monitor_max - arcade_monitor_min) * 255;
			int b = (B - arcade_monitor_min) / (arcade_monitor_max - arcade_monitor_min) * 255;

			// clamp and store
			r = (std::min)((std::max)(r, 0), 255);
			g = (std::min)((std::max)(g, 0), 255);
			b = (std::min)((std::max)(b, 0), 255);
			palette.set_pen_color(color * 16 + luma, rgb_t(r, g, b));
		}
	}

#if 0
	// Acquired Data
	palette.set_pen_color(0, rgb_t(7, 1, 9));
	palette.set_pen_color(1, rgb_t(7, 1, 9));
	palette.set_pen_color(2, rgb_t(7, 1, 9));
	palette.set_pen_color(3, rgb_t(7, 1, 9));
	palette.set_pen_color(4, rgb_t(7, 1, 10));
	palette.set_pen_color(5, rgb_t(7, 1, 10));
	palette.set_pen_color(6, rgb_t(7, 2, 13));
	palette.set_pen_color(7, rgb_t(7, 2, 13));
	palette.set_pen_color(8, rgb_t(17, 50, 71));
	palette.set_pen_color(9, rgb_t(17, 50, 71));
	palette.set_pen_color(10, rgb_t(92, 142, 165));
	palette.set_pen_color(11, rgb_t(92, 142, 165));
	palette.set_pen_color(12, rgb_t(166, 218, 236));
	palette.set_pen_color(13, rgb_t(166, 218, 236));
	palette.set_pen_color(14, rgb_t(233, 251, 255));
	palette.set_pen_color(15, rgb_t(233, 251, 255));
	palette.set_pen_color(16, rgb_t(7, 5, 58));
	palette.set_pen_color(17, rgb_t(7, 5, 58));
	palette.set_pen_color(18, rgb_t(7, 5, 124));
	palette.set_pen_color(19, rgb_t(7, 5, 124));
	palette.set_pen_color(20, rgb_t(7, 6, 195));
	palette.set_pen_color(21, rgb_t(7, 6, 195));
	palette.set_pen_color(22, rgb_t(11, 6, 249));
	palette.set_pen_color(23, rgb_t(11, 6, 249));
	palette.set_pen_color(24, rgb_t(67, 6, 254));
	palette.set_pen_color(25, rgb_t(67, 6, 254));
	palette.set_pen_color(26, rgb_t(151, 35, 255));
	palette.set_pen_color(27, rgb_t(151, 35, 255));
	palette.set_pen_color(28, rgb_t(221, 107, 255));
	palette.set_pen_color(29, rgb_t(221, 107, 255));
	palette.set_pen_color(30, rgb_t(255, 183, 255));
	palette.set_pen_color(31, rgb_t(255, 183, 255));
	palette.set_pen_color(32, rgb_t(6, 5, 42));
	palette.set_pen_color(33, rgb_t(6, 5, 42));
	palette.set_pen_color(34, rgb_t(6, 5, 107));
	palette.set_pen_color(35, rgb_t(6, 5, 107));
	palette.set_pen_color(36, rgb_t(9, 6, 180));
	palette.set_pen_color(37, rgb_t(9, 6, 180));
	palette.set_pen_color(38, rgb_t(51, 6, 242));
	palette.set_pen_color(39, rgb_t(51, 6, 242));
	palette.set_pen_color(40, rgb_t(122, 6, 254));
	palette.set_pen_color(41, rgb_t(122, 6, 254));
	palette.set_pen_color(42, rgb_t(204, 36, 255));
	palette.set_pen_color(43, rgb_t(204, 36, 255));
	palette.set_pen_color(44, rgb_t(254, 108, 255));
	palette.set_pen_color(45, rgb_t(254, 108, 255));
	palette.set_pen_color(46, rgb_t(255, 184, 255));
	palette.set_pen_color(47, rgb_t(255, 184, 255));
	palette.set_pen_color(48, rgb_t(6, 5, 24));
	palette.set_pen_color(49, rgb_t(6, 5, 24));
	palette.set_pen_color(50, rgb_t(7, 5, 82));
	palette.set_pen_color(51, rgb_t(7, 5, 82));
	palette.set_pen_color(52, rgb_t(35, 5, 154));
	palette.set_pen_color(53, rgb_t(35, 5, 154));
	palette.set_pen_color(54, rgb_t(102, 6, 222));
	palette.set_pen_color(55, rgb_t(102, 6, 222));
	palette.set_pen_color(56, rgb_t(175, 6, 255));
	palette.set_pen_color(57, rgb_t(175, 6, 255));
	palette.set_pen_color(58, rgb_t(246, 41, 254));
	palette.set_pen_color(59, rgb_t(246, 41, 254));
	palette.set_pen_color(60, rgb_t(255, 113, 255));
	palette.set_pen_color(61, rgb_t(255, 113, 255));
	palette.set_pen_color(62, rgb_t(255, 189, 255));
	palette.set_pen_color(63, rgb_t(255, 189, 255));
	palette.set_pen_color(64, rgb_t(6, 4, 10));
	palette.set_pen_color(65, rgb_t(6, 4, 10));
	palette.set_pen_color(66, rgb_t(17, 5, 49));
	palette.set_pen_color(67, rgb_t(17, 5, 49));
	palette.set_pen_color(68, rgb_t(78, 5, 119));
	palette.set_pen_color(69, rgb_t(78, 5, 119));
	palette.set_pen_color(70, rgb_t(148, 6, 191));
	palette.set_pen_color(71, rgb_t(148, 6, 191));
	palette.set_pen_color(72, rgb_t(219, 6, 249));
	palette.set_pen_color(73, rgb_t(219, 6, 249));
	palette.set_pen_color(74, rgb_t(255, 48, 255));
	palette.set_pen_color(75, rgb_t(255, 48, 255));
	palette.set_pen_color(76, rgb_t(255, 122, 255));
	palette.set_pen_color(77, rgb_t(255, 122, 255));
	palette.set_pen_color(78, rgb_t(255, 199, 255));
	palette.set_pen_color(79, rgb_t(255, 199, 255));
	palette.set_pen_color(80, rgb_t(7, 4, 8));
	palette.set_pen_color(81, rgb_t(7, 4, 8));
	palette.set_pen_color(82, rgb_t(45, 4, 18));
	palette.set_pen_color(83, rgb_t(45, 4, 18));
	palette.set_pen_color(84, rgb_t(114, 5, 76));
	palette.set_pen_color(85, rgb_t(114, 5, 76));
	palette.set_pen_color(86, rgb_t(185, 5, 149));
	palette.set_pen_color(87, rgb_t(185, 5, 149));
	palette.set_pen_color(88, rgb_t(245, 6, 220));
	palette.set_pen_color(89, rgb_t(245, 6, 220));
	palette.set_pen_color(90, rgb_t(255, 61, 255));
	palette.set_pen_color(91, rgb_t(255, 61, 255));
	palette.set_pen_color(92, rgb_t(255, 137, 255));
	palette.set_pen_color(93, rgb_t(255, 137, 255));
	palette.set_pen_color(94, rgb_t(255, 211, 255));
	palette.set_pen_color(95, rgb_t(255, 211, 255));
	palette.set_pen_color(96, rgb_t(15, 3, 9));
	palette.set_pen_color(97, rgb_t(15, 3, 9));
	palette.set_pen_color(98, rgb_t(71, 3, 9));
	palette.set_pen_color(99, rgb_t(71, 3, 9));
	palette.set_pen_color(100, rgb_t(142, 4, 34));
	palette.set_pen_color(101, rgb_t(142, 4, 34));
	palette.set_pen_color(102, rgb_t(211, 5, 100));
	palette.set_pen_color(103, rgb_t(211, 5, 100));
	palette.set_pen_color(104, rgb_t(254, 8, 174));
	palette.set_pen_color(105, rgb_t(254, 8, 174));
	palette.set_pen_color(106, rgb_t(255, 75, 249));
	palette.set_pen_color(107, rgb_t(255, 75, 249));
	palette.set_pen_color(108, rgb_t(255, 152, 255));
	palette.set_pen_color(109, rgb_t(255, 152, 255));
	palette.set_pen_color(110, rgb_t(255, 225, 255));
	palette.set_pen_color(111, rgb_t(255, 225, 255));
	palette.set_pen_color(112, rgb_t(26, 2, 9));
	palette.set_pen_color(113, rgb_t(26, 2, 9));
	palette.set_pen_color(114, rgb_t(87, 3, 9));
	palette.set_pen_color(115, rgb_t(87, 3, 9));
	palette.set_pen_color(116, rgb_t(158, 4, 10));
	palette.set_pen_color(117, rgb_t(158, 4, 10));
	palette.set_pen_color(118, rgb_t(225, 4, 49));
	palette.set_pen_color(119, rgb_t(225, 4, 49));
	palette.set_pen_color(120, rgb_t(255, 13, 121));
	palette.set_pen_color(121, rgb_t(255, 13, 121));
	palette.set_pen_color(122, rgb_t(255, 95, 212));
	palette.set_pen_color(123, rgb_t(255, 95, 212));
	palette.set_pen_color(124, rgb_t(255, 173, 255));
	palette.set_pen_color(125, rgb_t(255, 173, 255));
	palette.set_pen_color(126, rgb_t(255, 239, 255));
	palette.set_pen_color(127, rgb_t(255, 239, 255));
	palette.set_pen_color(128, rgb_t(32, 2, 8));
	palette.set_pen_color(129, rgb_t(32, 2, 8));
	palette.set_pen_color(130, rgb_t(94, 2, 10));
	palette.set_pen_color(131, rgb_t(94, 2, 10));
	palette.set_pen_color(132, rgb_t(165, 2, 9));
	palette.set_pen_color(133, rgb_t(165, 2, 9));
	palette.set_pen_color(134, rgb_t(231, 3, 15));
	palette.set_pen_color(135, rgb_t(231, 3, 15));
	palette.set_pen_color(136, rgb_t(255, 27, 73));
	palette.set_pen_color(137, rgb_t(255, 27, 73));
	palette.set_pen_color(138, rgb_t(255, 112, 166));
	palette.set_pen_color(139, rgb_t(255, 112, 166));
	palette.set_pen_color(140, rgb_t(255, 192, 238));
	palette.set_pen_color(141, rgb_t(255, 192, 238));
	palette.set_pen_color(142, rgb_t(255, 248, 255));
	palette.set_pen_color(143, rgb_t(255, 248, 255));
	palette.set_pen_color(144, rgb_t(24, 1, 9));
	palette.set_pen_color(145, rgb_t(24, 1, 9));
	palette.set_pen_color(146, rgb_t(86, 1, 9));
	palette.set_pen_color(147, rgb_t(86, 1, 9));
	palette.set_pen_color(148, rgb_t(157, 1, 10));
	palette.set_pen_color(149, rgb_t(157, 1, 10));
	palette.set_pen_color(150, rgb_t(225, 3, 10));
	palette.set_pen_color(151, rgb_t(225, 3, 10));
	palette.set_pen_color(152, rgb_t(255, 45, 22));
	palette.set_pen_color(153, rgb_t(255, 45, 22));
	palette.set_pen_color(154, rgb_t(255, 137, 104));
	palette.set_pen_color(155, rgb_t(255, 137, 104));
	palette.set_pen_color(156, rgb_t(255, 213, 183));
	palette.set_pen_color(157, rgb_t(255, 213, 183));
	palette.set_pen_color(158, rgb_t(255, 251, 247));
	palette.set_pen_color(159, rgb_t(255, 251, 247));
	palette.set_pen_color(160, rgb_t(14, 0, 9));
	palette.set_pen_color(161, rgb_t(14, 0, 9));
	palette.set_pen_color(162, rgb_t(69, 0, 9));
	palette.set_pen_color(163, rgb_t(69, 0, 9));
	palette.set_pen_color(164, rgb_t(141, 0, 10));
	palette.set_pen_color(165, rgb_t(141, 0, 10));
	palette.set_pen_color(166, rgb_t(209, 6, 9));
	palette.set_pen_color(167, rgb_t(209, 6, 9));
	palette.set_pen_color(168, rgb_t(254, 67, 10));
	palette.set_pen_color(169, rgb_t(254, 67, 10));
	palette.set_pen_color(170, rgb_t(255, 158, 52));
	palette.set_pen_color(171, rgb_t(255, 158, 52));
	palette.set_pen_color(172, rgb_t(255, 230, 130));
	palette.set_pen_color(173, rgb_t(255, 230, 130));
	palette.set_pen_color(174, rgb_t(255, 250, 204));
	palette.set_pen_color(175, rgb_t(255, 250, 204));
	palette.set_pen_color(176, rgb_t(7, 0, 9));
	palette.set_pen_color(177, rgb_t(7, 0, 9));
	palette.set_pen_color(178, rgb_t(42, 0, 9));
	palette.set_pen_color(179, rgb_t(42, 0, 9));
	palette.set_pen_color(180, rgb_t(111, 0, 10));
	palette.set_pen_color(181, rgb_t(111, 0, 10));
	palette.set_pen_color(182, rgb_t(182, 18, 10));
	palette.set_pen_color(183, rgb_t(182, 18, 10));
	palette.set_pen_color(184, rgb_t(245, 89, 10));
	palette.set_pen_color(185, rgb_t(245, 89, 10));
	palette.set_pen_color(186, rgb_t(255, 179, 17));
	palette.set_pen_color(187, rgb_t(255, 179, 17));
	palette.set_pen_color(188, rgb_t(255, 243, 79));
	palette.set_pen_color(189, rgb_t(255, 243, 79));
	palette.set_pen_color(190, rgb_t(255, 250, 152));
	palette.set_pen_color(191, rgb_t(255, 250, 152));
	palette.set_pen_color(192, rgb_t(7, 0, 9));
	palette.set_pen_color(193, rgb_t(7, 0, 9));
	palette.set_pen_color(194, rgb_t(15, 0, 9));
	palette.set_pen_color(195, rgb_t(15, 0, 9));
	palette.set_pen_color(196, rgb_t(73, 0, 9));
	palette.set_pen_color(197, rgb_t(73, 0, 9));
	palette.set_pen_color(198, rgb_t(145, 36, 10));
	palette.set_pen_color(199, rgb_t(145, 36, 10));
	palette.set_pen_color(200, rgb_t(217, 111, 11));
	palette.set_pen_color(201, rgb_t(217, 111, 11));
	palette.set_pen_color(202, rgb_t(255, 200, 12));
	palette.set_pen_color(203, rgb_t(255, 200, 12));
	palette.set_pen_color(204, rgb_t(255, 247, 38));
	palette.set_pen_color(205, rgb_t(255, 247, 38));
	palette.set_pen_color(206, rgb_t(255, 249, 106));
	palette.set_pen_color(207, rgb_t(255, 249, 106));
	palette.set_pen_color(208, rgb_t(7, 0, 9));
	palette.set_pen_color(209, rgb_t(7, 0, 9));
	palette.set_pen_color(210, rgb_t(7, 0, 10));
	palette.set_pen_color(211, rgb_t(7, 0, 10));
	palette.set_pen_color(212, rgb_t(32, 1, 9));
	palette.set_pen_color(213, rgb_t(32, 1, 9));
	palette.set_pen_color(214, rgb_t(97, 53, 10));
	palette.set_pen_color(215, rgb_t(97, 53, 10));
	palette.set_pen_color(216, rgb_t(172, 131, 10));
	palette.set_pen_color(217, rgb_t(172, 131, 10));
	palette.set_pen_color(218, rgb_t(247, 216, 11));
	palette.set_pen_color(219, rgb_t(247, 216, 11));
	palette.set_pen_color(220, rgb_t(255, 248, 15));
	palette.set_pen_color(221, rgb_t(255, 248, 15));
	palette.set_pen_color(222, rgb_t(255, 248, 68));
	palette.set_pen_color(223, rgb_t(255, 248, 68));
	palette.set_pen_color(224, rgb_t(7, 0, 9));
	palette.set_pen_color(225, rgb_t(7, 0, 9));
	palette.set_pen_color(226, rgb_t(7, 0, 10));
	palette.set_pen_color(227, rgb_t(7, 0, 10));
	palette.set_pen_color(228, rgb_t(8, 6, 9));
	palette.set_pen_color(229, rgb_t(8, 6, 9));
	palette.set_pen_color(230, rgb_t(48, 69, 10));
	palette.set_pen_color(231, rgb_t(48, 69, 10));
	palette.set_pen_color(232, rgb_t(121, 146, 11));
	palette.set_pen_color(233, rgb_t(121, 146, 11));
	palette.set_pen_color(234, rgb_t(206, 229, 12));
	palette.set_pen_color(235, rgb_t(206, 229, 12));
	palette.set_pen_color(236, rgb_t(255, 247, 13));
	palette.set_pen_color(237, rgb_t(255, 247, 13));
	palette.set_pen_color(238, rgb_t(255, 247, 41));
	palette.set_pen_color(239, rgb_t(255, 247, 41));
	palette.set_pen_color(240, rgb_t(7, 0, 10));
	palette.set_pen_color(241, rgb_t(7, 0, 10));
	palette.set_pen_color(242, rgb_t(7, 0, 10));
	palette.set_pen_color(243, rgb_t(7, 0, 10));
	palette.set_pen_color(244, rgb_t(7, 13, 10));
	palette.set_pen_color(245, rgb_t(7, 13, 10));
	palette.set_pen_color(246, rgb_t(11, 80, 11));
	palette.set_pen_color(247, rgb_t(11, 80, 11));
	palette.set_pen_color(248, rgb_t(66, 159, 11));
	palette.set_pen_color(249, rgb_t(66, 159, 11));
	palette.set_pen_color(250, rgb_t(152, 238, 11));
	palette.set_pen_color(251, rgb_t(152, 238, 11));
	palette.set_pen_color(252, rgb_t(222, 246, 12));
	palette.set_pen_color(253, rgb_t(222, 246, 12));
	palette.set_pen_color(254, rgb_t(255, 247, 28));
	palette.set_pen_color(255, rgb_t(255, 247, 28));
	palette.set_pen_color(256, rgb_t(7, 0, 10));
	palette.set_pen_color(257, rgb_t(7, 0, 10));
	palette.set_pen_color(258, rgb_t(7, 0, 9));
	palette.set_pen_color(259, rgb_t(7, 0, 9));
	palette.set_pen_color(260, rgb_t(7, 21, 9));
	palette.set_pen_color(261, rgb_t(7, 21, 9));
	palette.set_pen_color(262, rgb_t(8, 91, 10));
	palette.set_pen_color(263, rgb_t(8, 91, 10));
	palette.set_pen_color(264, rgb_t(17, 168, 11));
	palette.set_pen_color(265, rgb_t(17, 168, 11));
	palette.set_pen_color(266, rgb_t(90, 241, 11));
	palette.set_pen_color(267, rgb_t(90, 241, 11));
	palette.set_pen_color(268, rgb_t(164, 246, 12));
	palette.set_pen_color(269, rgb_t(164, 246, 12));
	palette.set_pen_color(270, rgb_t(231, 246, 23));
	palette.set_pen_color(271, rgb_t(231, 246, 23));
	palette.set_pen_color(272, rgb_t(8, 0, 8));
	palette.set_pen_color(273, rgb_t(8, 0, 8));
	palette.set_pen_color(274, rgb_t(7, 0, 9));
	palette.set_pen_color(275, rgb_t(7, 0, 9));
	palette.set_pen_color(276, rgb_t(8, 23, 10));
	palette.set_pen_color(277, rgb_t(8, 23, 10));
	palette.set_pen_color(278, rgb_t(7, 95, 11));
	palette.set_pen_color(279, rgb_t(7, 95, 11));
	palette.set_pen_color(280, rgb_t(8, 171, 10));
	palette.set_pen_color(281, rgb_t(8, 171, 10));
	palette.set_pen_color(282, rgb_t(35, 242, 11));
	palette.set_pen_color(283, rgb_t(35, 242, 11));
	palette.set_pen_color(284, rgb_t(103, 246, 11));
	palette.set_pen_color(285, rgb_t(103, 246, 11));
	palette.set_pen_color(286, rgb_t(175, 246, 27));
	palette.set_pen_color(287, rgb_t(175, 246, 27));
	palette.set_pen_color(288, rgb_t(8, 0, 9));
	palette.set_pen_color(289, rgb_t(8, 0, 9));
	palette.set_pen_color(290, rgb_t(8, 0, 10));
	palette.set_pen_color(291, rgb_t(8, 0, 10));
	palette.set_pen_color(292, rgb_t(7, 21, 10));
	palette.set_pen_color(293, rgb_t(7, 21, 10));
	palette.set_pen_color(294, rgb_t(8, 93, 10));
	palette.set_pen_color(295, rgb_t(8, 93, 10));
	palette.set_pen_color(296, rgb_t(8, 170, 11));
	palette.set_pen_color(297, rgb_t(8, 170, 11));
	palette.set_pen_color(298, rgb_t(8, 242, 11));
	palette.set_pen_color(299, rgb_t(8, 242, 11));
	palette.set_pen_color(300, rgb_t(48, 246, 11));
	palette.set_pen_color(301, rgb_t(48, 246, 11));
	palette.set_pen_color(302, rgb_t(117, 246, 39));
	palette.set_pen_color(303, rgb_t(117, 246, 39));
	palette.set_pen_color(304, rgb_t(8, 0, 10));
	palette.set_pen_color(305, rgb_t(8, 0, 10));
	palette.set_pen_color(306, rgb_t(7, 0, 10));
	palette.set_pen_color(307, rgb_t(7, 0, 10));
	palette.set_pen_color(308, rgb_t(7, 17, 10));
	palette.set_pen_color(309, rgb_t(7, 17, 10));
	palette.set_pen_color(310, rgb_t(8, 85, 11));
	palette.set_pen_color(311, rgb_t(8, 85, 11));
	palette.set_pen_color(312, rgb_t(9, 164, 11));
	palette.set_pen_color(313, rgb_t(9, 164, 11));
	palette.set_pen_color(314, rgb_t(8, 239, 12));
	palette.set_pen_color(315, rgb_t(8, 239, 12));
	palette.set_pen_color(316, rgb_t(11, 246, 14));
	palette.set_pen_color(317, rgb_t(11, 246, 14));
	palette.set_pen_color(318, rgb_t(66, 247, 65));
	palette.set_pen_color(319, rgb_t(66, 247, 65));
	palette.set_pen_color(320, rgb_t(7, 0, 9));
	palette.set_pen_color(321, rgb_t(7, 0, 9));
	palette.set_pen_color(322, rgb_t(8, 0, 10));
	palette.set_pen_color(323, rgb_t(8, 0, 10));
	palette.set_pen_color(324, rgb_t(7, 10, 10));
	palette.set_pen_color(325, rgb_t(7, 10, 10));
	palette.set_pen_color(326, rgb_t(7, 76, 10));
	palette.set_pen_color(327, rgb_t(7, 76, 10));
	palette.set_pen_color(328, rgb_t(8, 153, 11));
	palette.set_pen_color(329, rgb_t(8, 153, 11));
	palette.set_pen_color(330, rgb_t(8, 235, 11));
	palette.set_pen_color(331, rgb_t(8, 235, 11));
	palette.set_pen_color(332, rgb_t(8, 247, 34));
	palette.set_pen_color(333, rgb_t(8, 247, 34));
	palette.set_pen_color(334, rgb_t(24, 247, 102));
	palette.set_pen_color(335, rgb_t(24, 247, 102));
	palette.set_pen_color(336, rgb_t(8, 0, 10));
	palette.set_pen_color(337, rgb_t(8, 0, 10));
	palette.set_pen_color(338, rgb_t(8, 0, 10));
	palette.set_pen_color(339, rgb_t(8, 0, 10));
	palette.set_pen_color(340, rgb_t(8, 4, 10));
	palette.set_pen_color(341, rgb_t(8, 4, 10));
	palette.set_pen_color(342, rgb_t(8, 63, 10));
	palette.set_pen_color(343, rgb_t(8, 63, 10));
	palette.set_pen_color(344, rgb_t(8, 140, 11));
	palette.set_pen_color(345, rgb_t(8, 140, 11));
	palette.set_pen_color(346, rgb_t(9, 224, 17));
	palette.set_pen_color(347, rgb_t(9, 224, 17));
	palette.set_pen_color(348, rgb_t(9, 247, 74));
	palette.set_pen_color(349, rgb_t(9, 247, 74));
	palette.set_pen_color(350, rgb_t(9, 247, 146));
	palette.set_pen_color(351, rgb_t(9, 247, 146));
	palette.set_pen_color(352, rgb_t(7, 0, 10));
	palette.set_pen_color(353, rgb_t(7, 0, 10));
	palette.set_pen_color(354, rgb_t(8, 0, 11));
	palette.set_pen_color(355, rgb_t(8, 0, 11));
	palette.set_pen_color(356, rgb_t(7, 0, 10));
	palette.set_pen_color(357, rgb_t(7, 0, 10));
	palette.set_pen_color(358, rgb_t(8, 47, 11));
	palette.set_pen_color(359, rgb_t(8, 47, 11));
	palette.set_pen_color(360, rgb_t(8, 124, 10));
	palette.set_pen_color(361, rgb_t(8, 124, 10));
	palette.set_pen_color(362, rgb_t(8, 212, 50));
	palette.set_pen_color(363, rgb_t(8, 212, 50));
	palette.set_pen_color(364, rgb_t(9, 248, 124));
	palette.set_pen_color(365, rgb_t(9, 248, 124));
	palette.set_pen_color(366, rgb_t(9, 249, 197));
	palette.set_pen_color(367, rgb_t(9, 249, 197));
	palette.set_pen_color(368, rgb_t(7, 0, 9));
	palette.set_pen_color(369, rgb_t(7, 0, 9));
	palette.set_pen_color(370, rgb_t(7, 0, 10));
	palette.set_pen_color(371, rgb_t(7, 0, 10));
	palette.set_pen_color(372, rgb_t(8, 0, 10));
	palette.set_pen_color(373, rgb_t(8, 0, 10));
	palette.set_pen_color(374, rgb_t(7, 31, 11));
	palette.set_pen_color(375, rgb_t(7, 31, 11));
	palette.set_pen_color(376, rgb_t(8, 105, 21));
	palette.set_pen_color(377, rgb_t(8, 105, 21));
	palette.set_pen_color(378, rgb_t(9, 196, 101));
	palette.set_pen_color(379, rgb_t(9, 196, 101));
	palette.set_pen_color(380, rgb_t(9, 248, 178));
	palette.set_pen_color(381, rgb_t(9, 248, 178));
	palette.set_pen_color(382, rgb_t(8, 249, 242));
	palette.set_pen_color(383, rgb_t(8, 249, 242));
	palette.set_pen_color(384, rgb_t(8, 0, 10));
	palette.set_pen_color(385, rgb_t(8, 0, 10));
	palette.set_pen_color(386, rgb_t(8, 0, 10));
	palette.set_pen_color(387, rgb_t(8, 0, 10));
	palette.set_pen_color(388, rgb_t(7, 0, 10));
	palette.set_pen_color(389, rgb_t(7, 0, 10));
	palette.set_pen_color(390, rgb_t(8, 14, 12));
	palette.set_pen_color(391, rgb_t(8, 14, 12));
	palette.set_pen_color(392, rgb_t(8, 84, 65));
	palette.set_pen_color(393, rgb_t(8, 84, 65));
	palette.set_pen_color(394, rgb_t(8, 176, 157));
	palette.set_pen_color(395, rgb_t(8, 176, 157));
	palette.set_pen_color(396, rgb_t(9, 242, 231));
	palette.set_pen_color(397, rgb_t(9, 242, 231));
	palette.set_pen_color(398, rgb_t(9, 249, 255));
	palette.set_pen_color(399, rgb_t(9, 249, 255));
	palette.set_pen_color(400, rgb_t(7, 1, 9));
	palette.set_pen_color(401, rgb_t(7, 1, 9));
	palette.set_pen_color(402, rgb_t(7, 1, 10));
	palette.set_pen_color(403, rgb_t(7, 1, 10));
	palette.set_pen_color(404, rgb_t(8, 1, 10));
	palette.set_pen_color(405, rgb_t(8, 1, 10));
	palette.set_pen_color(406, rgb_t(8, 5, 42));
	palette.set_pen_color(407, rgb_t(8, 5, 42));
	palette.set_pen_color(408, rgb_t(8, 63, 118));
	palette.set_pen_color(409, rgb_t(8, 63, 118));
	palette.set_pen_color(410, rgb_t(8, 154, 210));
	palette.set_pen_color(411, rgb_t(8, 154, 210));
	palette.set_pen_color(412, rgb_t(9, 227, 255));
	palette.set_pen_color(413, rgb_t(9, 227, 255));
	palette.set_pen_color(414, rgb_t(8, 251, 255));
	palette.set_pen_color(415, rgb_t(8, 251, 255));
	palette.set_pen_color(416, rgb_t(7, 2, 9));
	palette.set_pen_color(417, rgb_t(7, 2, 9));
	palette.set_pen_color(418, rgb_t(8, 2, 10));
	palette.set_pen_color(419, rgb_t(8, 2, 10));
	palette.set_pen_color(420, rgb_t(7, 2, 29));
	palette.set_pen_color(421, rgb_t(7, 2, 29));
	palette.set_pen_color(422, rgb_t(8, 3, 93));
	palette.set_pen_color(423, rgb_t(8, 3, 93));
	palette.set_pen_color(424, rgb_t(8, 42, 171));
	palette.set_pen_color(425, rgb_t(8, 42, 171));
	palette.set_pen_color(426, rgb_t(8, 134, 249));
	palette.set_pen_color(427, rgb_t(8, 134, 249));
	palette.set_pen_color(428, rgb_t(8, 208, 255));
	palette.set_pen_color(429, rgb_t(8, 208, 255));
	palette.set_pen_color(430, rgb_t(8, 250, 255));
	palette.set_pen_color(431, rgb_t(8, 250, 255));
	palette.set_pen_color(432, rgb_t(7, 3, 9));
	palette.set_pen_color(433, rgb_t(7, 3, 9));
	palette.set_pen_color(434, rgb_t(8, 3, 15));
	palette.set_pen_color(435, rgb_t(8, 3, 15));
	palette.set_pen_color(436, rgb_t(8, 3, 71));
	palette.set_pen_color(437, rgb_t(8, 3, 71));
	palette.set_pen_color(438, rgb_t(8, 3, 142));
	palette.set_pen_color(439, rgb_t(8, 3, 142));
	palette.set_pen_color(440, rgb_t(8, 26, 216));
	palette.set_pen_color(441, rgb_t(8, 26, 216));
	palette.set_pen_color(442, rgb_t(8, 111, 255));
	palette.set_pen_color(443, rgb_t(8, 111, 255));
	palette.set_pen_color(444, rgb_t(8, 187, 255));
	palette.set_pen_color(445, rgb_t(8, 187, 255));
	palette.set_pen_color(446, rgb_t(9, 247, 255));
	palette.set_pen_color(447, rgb_t(9, 247, 255));
	palette.set_pen_color(448, rgb_t(8, 4, 10));
	palette.set_pen_color(449, rgb_t(8, 4, 10));
	palette.set_pen_color(450, rgb_t(7, 4, 45));
	palette.set_pen_color(451, rgb_t(7, 4, 45));
	palette.set_pen_color(452, rgb_t(8, 4, 114));
	palette.set_pen_color(453, rgb_t(8, 4, 114));
	palette.set_pen_color(454, rgb_t(7, 4, 186));
	palette.set_pen_color(455, rgb_t(7, 4, 186));
	palette.set_pen_color(456, rgb_t(8, 12, 247));
	palette.set_pen_color(457, rgb_t(8, 12, 247));
	palette.set_pen_color(458, rgb_t(8, 90, 255));
	palette.set_pen_color(459, rgb_t(8, 90, 255));
	palette.set_pen_color(460, rgb_t(8, 167, 255));
	palette.set_pen_color(461, rgb_t(8, 167, 255));
	palette.set_pen_color(462, rgb_t(27, 236, 255));
	palette.set_pen_color(463, rgb_t(27, 236, 255));
	palette.set_pen_color(464, rgb_t(7, 4, 21));
	palette.set_pen_color(465, rgb_t(7, 4, 21));
	palette.set_pen_color(466, rgb_t(7, 4, 79));
	palette.set_pen_color(467, rgb_t(7, 4, 79));
	palette.set_pen_color(468, rgb_t(8, 5, 151));
	palette.set_pen_color(469, rgb_t(8, 5, 151));
	palette.set_pen_color(470, rgb_t(7, 4, 219));
	palette.set_pen_color(471, rgb_t(7, 4, 219));
	palette.set_pen_color(472, rgb_t(8, 7, 255));
	palette.set_pen_color(473, rgb_t(8, 7, 255));
	palette.set_pen_color(474, rgb_t(8, 71, 255));
	palette.set_pen_color(475, rgb_t(8, 71, 255));
	palette.set_pen_color(476, rgb_t(13, 148, 255));
	palette.set_pen_color(477, rgb_t(13, 148, 255));
	palette.set_pen_color(478, rgb_t(69, 220, 255));
	palette.set_pen_color(479, rgb_t(69, 220, 255));
	palette.set_pen_color(480, rgb_t(7, 5, 41));
	palette.set_pen_color(481, rgb_t(7, 5, 41));
	palette.set_pen_color(482, rgb_t(7, 5, 107));
	palette.set_pen_color(483, rgb_t(7, 5, 107));
	palette.set_pen_color(484, rgb_t(7, 6, 178));
	palette.set_pen_color(485, rgb_t(7, 6, 178));
	palette.set_pen_color(486, rgb_t(8, 5, 241));
	palette.set_pen_color(487, rgb_t(8, 5, 241));
	palette.set_pen_color(488, rgb_t(8, 6, 254));
	palette.set_pen_color(489, rgb_t(8, 6, 254));
	palette.set_pen_color(490, rgb_t(9, 56, 254));
	palette.set_pen_color(491, rgb_t(9, 56, 254));
	palette.set_pen_color(492, rgb_t(50, 134, 255));
	palette.set_pen_color(493, rgb_t(50, 134, 255));
	palette.set_pen_color(494, rgb_t(122, 208, 255));
	palette.set_pen_color(495, rgb_t(122, 208, 255));
	palette.set_pen_color(496, rgb_t(7, 5, 57));
	palette.set_pen_color(497, rgb_t(7, 5, 57));
	palette.set_pen_color(498, rgb_t(8, 5, 123));
	palette.set_pen_color(499, rgb_t(8, 5, 123));
	palette.set_pen_color(500, rgb_t(7, 6, 196));
	palette.set_pen_color(501, rgb_t(7, 6, 196));
	palette.set_pen_color(502, rgb_t(7, 6, 249));
	palette.set_pen_color(503, rgb_t(7, 6, 249));
	palette.set_pen_color(504, rgb_t(8, 6, 254));
	palette.set_pen_color(505, rgb_t(8, 6, 254));
	palette.set_pen_color(506, rgb_t(37, 44, 254));
	palette.set_pen_color(507, rgb_t(37, 44, 254));
	palette.set_pen_color(508, rgb_t(107, 119, 255));
	palette.set_pen_color(509, rgb_t(107, 119, 255));
	palette.set_pen_color(510, rgb_t(181, 198, 255));
	palette.set_pen_color(511, rgb_t(181, 198, 255));
#endif

}


void astrocde_state::profpac_palette(palette_device &palette) const
{
	// Professor Pac-Man uses a more standard 12-bit RGB palette layout
	static constexpr int resistances[4] = { 6200, 3000, 1500, 750 };

	// compute the color output resistor weights
	double weights[4];
	compute_resistor_weights(0, 255, -1.0,
			4, resistances, weights, 1500, 0,
			4, resistances, weights, 1500, 0,
			4, resistances, weights, 1500, 0);

	// initialize the palette with these colors
	for (int i = 0; i < 4096; i++)
	{
		int bit0, bit1, bit2, bit3;

		// blue component
		bit0 = BIT(i, 0);
		bit1 = BIT(i, 1);
		bit2 = BIT(i, 2);
		bit3 = BIT(i, 3);
		int const b = combine_weights(weights, bit0, bit1, bit2, bit3);

		// green component
		bit0 = BIT(i, 4);
		bit1 = BIT(i, 5);
		bit2 = BIT(i, 6);
		bit3 = BIT(i, 7);
		int const g = combine_weights(weights, bit0, bit1, bit2, bit3);

		// red component
		bit0 = BIT(i, 8);
		bit1 = BIT(i, 9);
		bit2 = BIT(i, 10);
		bit3 = BIT(i, 11);
		int const r = combine_weights(weights, bit0, bit1, bit2, bit3);

		palette.set_pen_color(i, rgb_t(r, g, b));
	}
}



/*************************************
 *
 *  Video startup
 *
 *************************************/

void astrocde_state::video_start()
{
	/* allocate timers */
	m_scanline_timer = timer_alloc(TIMER_SCANLINE);
	m_scanline_timer->adjust(m_screen->time_until_pos(1), 1);
	m_intoff_timer = timer_alloc(TIMER_INTERRUPT_OFF);

	/* register for save states */
	init_savestate();

	/* initialize the sparkle and stars */
	if (m_video_config & AC_STARS)
		init_sparklestar();
}


VIDEO_START_MEMBER(astrocde_state,profpac)
{
	/* allocate timers */
	m_scanline_timer = timer_alloc(TIMER_SCANLINE);
	m_scanline_timer->adjust(m_screen->time_until_pos(1), 1);
	m_intoff_timer = timer_alloc(TIMER_INTERRUPT_OFF);

	/* allocate videoram */
	m_profpac_videoram = std::make_unique<uint16_t[]>(0x4000 * 4);

	/* register for save states */
	init_savestate();

	/* register our specific save state data */
	save_pointer(NAME(m_profpac_videoram), 0x4000 * 4);
	save_item(NAME(m_profpac_palette));
	save_item(NAME(m_profpac_colormap));
	save_item(NAME(m_profpac_intercept));
	save_item(NAME(m_profpac_vispage));
	save_item(NAME(m_profpac_readpage));
	save_item(NAME(m_profpac_readshift));
	save_item(NAME(m_profpac_writepage));
	save_item(NAME(m_profpac_writemode));
	save_item(NAME(m_profpac_writemask));
	save_item(NAME(m_profpac_vw));

	std::fill(std::begin(m_profpac_palette), std::end(m_profpac_palette), 0);
}


void astrocde_state::init_savestate()
{
	save_item(NAME(m_sparkle));

	save_item(NAME(m_interrupt_enabl));
	save_item(NAME(m_interrupt_vector));
	save_item(NAME(m_interrupt_scanline));
	save_item(NAME(m_vertical_feedback));
	save_item(NAME(m_horizontal_feedback));

	save_item(NAME(m_colors));
	save_item(NAME(m_colorsplit));
	save_item(NAME(m_bgdata));
	save_item(NAME(m_vblank));
	save_item(NAME(m_video_mode));

	save_item(NAME(m_funcgen_expand_color));
	save_item(NAME(m_funcgen_control));
	save_item(NAME(m_funcgen_expand_count));
	save_item(NAME(m_funcgen_rotate_count));
	save_item(NAME(m_funcgen_rotate_data));
	save_item(NAME(m_funcgen_shift_prev_data));
	save_item(NAME(m_funcgen_intercept));

	save_item(NAME(m_pattern_source));
	save_item(NAME(m_pattern_mode));
	save_item(NAME(m_pattern_dest));
	save_item(NAME(m_pattern_skip));
	save_item(NAME(m_pattern_width));
	save_item(NAME(m_pattern_height));
}



/*************************************
 *
 *  Video update
 *
 *************************************/

uint32_t astrocde_state::screen_update_astrocde(screen_device &screen, bitmap_ind16 &bitmap, const rectangle &cliprect)
{
	uint8_t const *const videoram = m_videoram;
	uint32_t sparklebase = 0;
	const int colormask = (m_video_config & AC_MONITOR_BW) ? 0 : 0x1f0;
	int xystep = 2 - m_video_mode;

	/* compute the starting point of sparkle for the current frame */
	int width = screen.width();
	int height = screen.height();

	if (m_video_config & AC_STARS)
		sparklebase = (screen.frame_number() * (uint64_t)(width * height)) % RNG_PERIOD;

	/* iterate over scanlines */
	for (int y = cliprect.min_y; y <= cliprect.max_y; y++)
	{
		uint16_t *dest = &bitmap.pix(y);
		int effy = mame_vpos_to_astrocade_vpos(y);
		uint16_t offset = (effy / xystep) * (80 / xystep);
		uint32_t sparkleoffs = 0, staroffs = 0;

		/* compute the star and sparkle offset at the start of this line */
		if (m_video_config & AC_STARS)
		{
			staroffs = ((effy < 0) ? (effy + 262) : effy) * width;
			sparkleoffs = sparklebase + y * width;
			if (sparkleoffs >= RNG_PERIOD)
				sparkleoffs -= RNG_PERIOD;
		}

		/* iterate over groups of 4 pixels */
		for (int x = 0; x < 456/4; x += xystep)
		{
			int effx = x - HORZ_OFFSET/4;
			const uint8_t *colorbase = &m_colors[(effx < m_colorsplit) ? 4 : 0];

			/* select either video data or background data */
			uint8_t data = (effx >= 0 && effx < 80 && effy >= 0 && effy < m_vblank) ? videoram[offset++] : m_bgdata;

			/* iterate over the 4 pixels */
			for (int xx = 0; xx < 4; xx++)
			{
				uint8_t pixdata = (data >> 6) & 3;
				int colordata = colorbase[pixdata] << 1;
				int luma = colordata & 0x0f;

				/* handle stars/sparkle */
				if (m_video_config & AC_STARS)
				{
					/* if sparkle is enabled for this pixel index and either it is non-zero or a star */
					/* then adjust the intensity */
					if (m_sparkle[pixdata] == 0)
					{
						if (pixdata != 0 || (m_sparklestar[staroffs] & 0x10))
						{
							int luma2 = m_sparklestar[sparkleoffs] & 0x0f;
							//if (luma2 < luma)
							//	luma = luma2;
							// TBD
							luma = luma * (((luma2+0.0)/30)+0.5);
						}
						else if (pixdata == 0)
							colordata = luma = 0;
					}

					/* update sparkle/star offsets */
					staroffs++;
					if (++sparkleoffs >= RNG_PERIOD)
						sparkleoffs = 0;
				}
				rgb_t const color = (colordata & colormask) | luma;

				/* store the final color to the destination and shift */
				*dest++ = color;
				if (xystep == 2)
					*dest++ = color;
				data <<= 2;
			}
		}
	}

	return 0;
}


uint32_t astrocde_state::screen_update_profpac(screen_device &screen, bitmap_ind16 &bitmap, const rectangle &cliprect)
{
	/* iterate over scanlines */
	for (int y = cliprect.min_y; y <= cliprect.max_y; y++)
	{
		int effy = mame_vpos_to_astrocade_vpos(y);
		uint16_t *dest = &bitmap.pix(y);
		uint16_t offset = m_profpac_vispage * 0x4000 + effy * 80;

		/* star with black */

		/* iterate over groups of 4 pixels */
		for (int x = 0; x < 456/4; x++)
		{
			int effx = x - HORZ_OFFSET/4;

			/* select either video data or background data */
			uint16_t data = (effx >= 0 && effx < 80 && effy >= 0 && effy < m_vblank) ? m_profpac_videoram[offset++] : 0;

			/* iterate over the 4 pixels */
			*dest++ = m_profpac_palette[(data >> 12) & 0x0f];
			*dest++ = m_profpac_palette[(data >> 8) & 0x0f];
			*dest++ = m_profpac_palette[(data >> 4) & 0x0f];
			*dest++ = m_profpac_palette[(data >> 0) & 0x0f];
		}
	}

	return 0;
}



/*************************************
 *
 *  Interrupt generation
 *
 *************************************/

void astrocde_state::device_timer(emu_timer &timer, device_timer_id id, int param, void *ptr)
{
	switch (id)
	{
	case TIMER_INTERRUPT_OFF:
		m_maincpu->set_input_line(0, CLEAR_LINE);
		break;
	case TIMER_SCANLINE:
		scanline_callback(ptr, param);
		break;
	default:
		throw emu_fatalerror("Unknown id in astrocde_state::device_timer");
	}
}

WRITE_LINE_MEMBER(astrocde_state::lightpen_trigger_w)
{
	if (state)
	{
		uint8_t res_shift = 1 - m_video_mode;
		astrocade_trigger_lightpen(mame_vpos_to_astrocade_vpos(m_screen->vpos()) & ~res_shift, (m_screen->hpos() >> res_shift) + 12);
	}
}

void astrocde_state::astrocade_trigger_lightpen(uint8_t vfeedback, uint8_t hfeedback)
{
	/* both bits 1 and 4 enable lightpen interrupts; bit 4 enables them even in horizontal */
	/* blanking regions; we treat them both the same here */
	if ((m_interrupt_enabl & 0x12) != 0)
	{
		/* bit 0 controls the interrupt mode: mode 0 means assert until acknowledged */
		if ((m_interrupt_enabl & 0x01) == 0)
		{
			m_maincpu->set_input_line_and_vector(0, HOLD_LINE, m_interrupt_vector & 0xf0); // Z80
			m_intoff_timer->adjust(m_screen->time_until_pos(vfeedback));
		}

		/* mode 1 means assert for 1 instruction */
		else
		{
			m_maincpu->set_input_line_and_vector(0, ASSERT_LINE, m_interrupt_vector & 0xf0); // Z80
			m_intoff_timer->adjust(m_maincpu->cycles_to_attotime(1));
		}

		/* latch the feedback registers */
		m_vertical_feedback = vfeedback;
		m_horizontal_feedback = hfeedback;
	}
}



/*************************************
 *
 *  Per-scanline callback
 *
 *************************************/

TIMER_CALLBACK_MEMBER(astrocde_state::scanline_callback)
{
	int scanline = param;
	int astrocade_scanline = mame_vpos_to_astrocade_vpos(scanline);

	/* force an update against the current scanline */
	if (scanline > 0)
		m_screen->update_partial(scanline - 1);

	/* generate a scanline interrupt if it's time */
	if (astrocade_scanline == m_interrupt_scanline && (m_interrupt_enabl & 0x08) != 0)
	{
		/* bit 2 controls the interrupt mode: mode 0 means assert until acknowledged */
		if ((m_interrupt_enabl & 0x04) == 0)
		{
			m_maincpu->set_input_line_and_vector(0, HOLD_LINE, m_interrupt_vector); // Z80
			m_intoff_timer->adjust(m_screen->time_until_vblank_end());
		}

		/* mode 1 means assert for 1 instruction */
		else
		{
			m_maincpu->set_input_line_and_vector(0, ASSERT_LINE, m_interrupt_vector); // Z80
			m_intoff_timer->adjust(m_maincpu->cycles_to_attotime(1));
		}
	}

	/* on some games, the horizontal drive line is conected to the lightpen interrupt */
	else if (m_video_config & AC_LIGHTPEN_INTS)
		astrocade_trigger_lightpen(astrocade_scanline, 8);

	/* advance to the next scanline */
	scanline++;
	if (scanline >= m_screen->height())
		scanline = 0;
	m_scanline_timer->adjust(m_screen->time_until_pos(scanline), scanline);
}



/*************************************
 *
 *  Data chip registers
 *
 *************************************/

uint8_t astrocde_state::video_register_r(offs_t offset)
{
	uint8_t result = 0xff;

	/* these are the core registers */
	switch (offset & 0xff)
	{
	case 0x08:  /* intercept feedback */
		result = m_funcgen_intercept;
		m_funcgen_intercept = 0;
		break;

	case 0x0e:  /* vertical feedback (from lightpen interrupt) */
		result = m_vertical_feedback;
		break;

	case 0x0f:  /* horizontal feedback (from lightpen interrupt) */
		result = m_horizontal_feedback;
		break;
	}

	return result;
}


void astrocde_state::video_register_w(offs_t offset, uint8_t data)
{
	/* these are the core registers */
	switch (offset & 0xff)
	{
	case 0x00:  /* color table is in registers 0-7 */
	case 0x01:
	case 0x02:
	case 0x03:
	case 0x04:
	case 0x05:
	case 0x06:
	case 0x07:
		m_colors[offset & 7] = data;
		break;

	case 0x08:  /* mode register */
		m_video_mode = data & 1;
		break;

	case 0x09:  /* color split pixel */
		m_colorsplit = 2 * (data & 0x3f);
		m_bgdata = ((data & 0xc0) >> 6) * 0x55;
		break;

	case 0x0a:  /* vertical blank register */
		m_vblank = data;
		break;

	case 0x0b:  /* color block transfer */
		m_colors[(offset >> 8) & 7] = data;
		break;

	case 0x0c:  /* function generator */
		m_funcgen_control = data;
		m_funcgen_expand_count = 0;     /* reset flip-flop for expand mode on write to this register */
		m_funcgen_rotate_count = 0;     /* reset counter for rotate mode on write to this register */
		m_funcgen_shift_prev_data = 0;  /* reset shift buffer on write to this register */
		break;

	case 0x0d:  /* interrupt feedback */
		m_interrupt_vector = data;
		m_maincpu->set_input_line(0, CLEAR_LINE);
		break;

	case 0x0e:  /* interrupt enable and mode */
		m_interrupt_enabl = data;
		m_maincpu->set_input_line(0, CLEAR_LINE);
		break;

	case 0x0f:  /* interrupt line */
		m_interrupt_scanline = data;
		m_maincpu->set_input_line(0, CLEAR_LINE);
		break;

#ifdef UNUSED_OLD_CODE
	case 0x10:  /* master oscillator register */
	case 0x11:  /* tone A frequency register */
	case 0x12:  /* tone B frequency register */
	case 0x13:  /* tone C frequency register */
	case 0x14:  /* vibrato register */
	case 0x15:  /* tone C volume, noise modulation and MUX register */
	case 0x16:  /* tone A volume and tone B volume register */
	case 0x17:  /* noise volume register */
	case 0x18:  /* sound block transfer */
		if (m_video_config & AC_SOUND_PRESENT)
			m_astrocade_sound1->write(space, offset, data);
		break;
#endif
	}
}



/*************************************
 *
 *  Function generator
 *
 *************************************/

void astrocde_state::astrocade_funcgen_w(address_space &space, offs_t offset, uint8_t data)
{
	uint8_t prev_data;

	/* control register:
	    bit 0 = shift amount LSB
	    bit 1 = shift amount MSB
	    bit 2 = rotate
	    bit 3 = expand
	    bit 4 = OR
	    bit 5 = XOR
	    bit 6 = flop
	*/

	/* expansion */
	if (m_funcgen_control & 0x08)
	{
		m_funcgen_expand_count ^= 1;
		data >>= 4 * m_funcgen_expand_count;
		data =  (m_funcgen_expand_color[(data >> 3) & 1] << 6) |
				(m_funcgen_expand_color[(data >> 2) & 1] << 4) |
				(m_funcgen_expand_color[(data >> 1) & 1] << 2) |
				(m_funcgen_expand_color[(data >> 0) & 1] << 0);
	}
	prev_data = m_funcgen_shift_prev_data;
	m_funcgen_shift_prev_data = data;

	/* rotate or shift */
	if (m_funcgen_control & 0x04)
	{
		/* rotate */

		/* first 4 writes accumulate data for the rotate */
		if ((m_funcgen_rotate_count & 4) == 0)
		{
			m_funcgen_rotate_data[m_funcgen_rotate_count++ & 3] = data;
			return;
		}

		/* second 4 writes actually write it */
		else
		{
			uint8_t shift = 2 * (~m_funcgen_rotate_count++ & 3);
			data =  (((m_funcgen_rotate_data[3] >> shift) & 3) << 6) |
					(((m_funcgen_rotate_data[2] >> shift) & 3) << 4) |
					(((m_funcgen_rotate_data[1] >> shift) & 3) << 2) |
					(((m_funcgen_rotate_data[0] >> shift) & 3) << 0);
		}
	}
	else
	{
		/* shift */
		uint8_t shift = 2 * (m_funcgen_control & 0x03);
		data = (data >> shift) | (prev_data << (8 - shift));
	}

	/* flopping */
	if (m_funcgen_control & 0x40)
		data = (data >> 6) | ((data >> 2) & 0x0c) | ((data << 2) & 0x30) | (data << 6);

	/* OR/XOR */
	if (m_funcgen_control & 0x30)
	{
		uint8_t olddata = space.read_byte(0x4000 + offset);

		/* compute any intercepts */
		m_funcgen_intercept &= 0x0f;
		if ((olddata & 0xc0) && (data & 0xc0))
			m_funcgen_intercept |= 0x11;
		if ((olddata & 0x30) && (data & 0x30))
			m_funcgen_intercept |= 0x22;
		if ((olddata & 0x0c) && (data & 0x0c))
			m_funcgen_intercept |= 0x44;
		if ((olddata & 0x03) && (data & 0x03))
			m_funcgen_intercept |= 0x88;

		/* apply the operation */
		if (m_funcgen_control & 0x10)
			data |= olddata;
		else if (m_funcgen_control & 0x20)
			data ^= olddata;
	}

	/* write the result */
	space.write_byte(0x4000 + offset, data);
}


void astrocde_state::expand_register_w(uint8_t data)
{
	m_funcgen_expand_color[0] = data & 0x03;
	m_funcgen_expand_color[1] = (data >> 2) & 0x03;
}



/*************************************
 *
 *  Pattern board
 *
 *************************************/

inline void astrocde_state::increment_source(uint8_t curwidth, uint8_t *u13ff)
{
	/* if the flip-flop at U13 is high and mode.d2 is 1 we can increment */
	/* however, if mode.d3 is set and we're on the last byte of a row, the increment is suppressed */
	if (*u13ff && (m_pattern_mode & 0x04) != 0 && (curwidth != 0 || (m_pattern_mode & 0x08) == 0))
		m_pattern_source++;

	/* if mode.d1 is 1, toggle the flip-flop; otherwise leave it preset */
	if ((m_pattern_mode & 0x02) != 0)
		*u13ff ^= 1;
}


inline void astrocde_state::increment_dest(uint8_t curwidth)
{
	/* increment is suppressed for the last byte in a row */
	if (curwidth != 0)
	{
		/* if mode.d5 is 1, we increment */
		if ((m_pattern_mode & 0x20) != 0)
			m_pattern_dest++;

		/* otherwise, we decrement */
		else
			m_pattern_dest--;
	}
}


void astrocde_state::execute_blit()
{
	address_space &space = m_maincpu->space(AS_PROGRAM);

	/*
	    m_pattern_source = counter set U7/U16/U25/U34
	    m_pattern_dest = counter set U9/U18/U30/U39
	    m_pattern_mode = latch U21
	    m_pattern_skip = latch set U30/U39
	    m_pattern_width = latch set U32/U41
	    m_pattern_height = counter set U31/U40

	    m_pattern_mode bits:
	        d0 = direction (0 = read from src, write to dest, 1 = read from dest, write to src)
	        d1 = expand (0 = increment src each pixel, 1 = increment src every other pixel)
	        d2 = constant (0 = never increment src, 1 = normal src increment)
	        d3 = flush (0 = copy all data, 1 = copy a 0 in place of last byte of each row)
	        d4 = dest increment direction (0 = decrement dest on carry, 1 = increment dest on carry)
	        d5 = dest direction (0 = increment dest, 1 = decrement dest)
	*/

	uint8_t curwidth; /* = counter set U33/U42 */
	uint8_t u13ff;    /* = flip-flop at U13 */
	int cycles = 0;

/*  logerror("Blit: src=%04X mode=%02X dest=%04X skip=%02X width=%02X height=%02X\n",
            m_pattern_source, m_pattern_mode, m_pattern_dest, m_pattern_skip, m_pattern_width, m_pattern_height);*/

	/* flip-flop at U13 is cleared at the beginning */
	u13ff = 0;

	/* it is also forced preset if mode.d1 == 0 */
	if ((m_pattern_mode & 0x02) == 0)
		u13ff = 1;

	/* loop over height */
	do
	{
		uint16_t carry;

		/* loop over width */
		curwidth = m_pattern_width;
		do
		{
			uint16_t busaddr;
			uint8_t busdata;

			/* ----- read phase ----- */

			/* address is selected between source/dest based on mode.d0 */
			busaddr = ((m_pattern_mode & 0x01) == 0) ? m_pattern_source : m_pattern_dest;

			/* if mode.d3 is set, then the last byte fetched per row is forced to 0 */
			if (curwidth == 0 && (m_pattern_mode & 0x08) != 0)
				busdata = 0;
			else
				busdata = space.read_byte(busaddr);

			/* increment the appropriate address */
			if ((m_pattern_mode & 0x01) == 0)
				increment_source(curwidth, &u13ff);
			else
				increment_dest(curwidth);

			/* ----- write phase ----- */

			/* address is selected between source/dest based on mode.d0 */
			busaddr = ((m_pattern_mode & 0x01) != 0) ? m_pattern_source : m_pattern_dest;
			space.write_byte(busaddr, busdata);

			/* increment the appropriate address */
			if ((m_pattern_mode & 0x01) == 0)
				increment_dest(curwidth);
			else
				increment_source(curwidth, &u13ff);

			/* count 4 cycles (two read, two write) */
			cycles += 4;

		} while (curwidth-- != 0);

		/* at the end of each row, the skip value is added to the dest value */
		carry = ((m_pattern_dest & 0xff) + m_pattern_skip) & 0x100;
		m_pattern_dest = (m_pattern_dest & 0xff00) | ((m_pattern_dest + m_pattern_skip) & 0xff);

		/* carry behavior into the top byte is controlled by mode.d4 */
		if ((m_pattern_mode & 0x10) == 0)
			m_pattern_dest += carry;
		else
			m_pattern_dest -= carry ^ 0x100;

	} while (m_pattern_height-- != 0);

	/* count cycles we ran the bus */
	m_maincpu->adjust_icount(-cycles);
}


void astrocde_state::astrocade_pattern_board_w(offs_t offset, uint8_t data)
{
	switch (offset)
	{
		case 0:     /* source offset low 8 bits */
			m_pattern_source = (m_pattern_source & 0xff00) | (data << 0);
			break;

		case 1:     /* source offset upper 8 bits */
			m_pattern_source = (m_pattern_source & 0x00ff) | (data << 8);
			break;

		case 2:     /* mode control; also clears low byte of dest */
			m_pattern_mode = data & 0x3f;
			m_pattern_dest &= 0xff00;
			break;

		case 3:     /* skip value */
			m_pattern_skip = data;
			break;

		case 4:     /* dest offset upper 8 bits; also adds skip to low 8 bits */
			m_pattern_dest = ((m_pattern_dest + m_pattern_skip) & 0xff) | (data << 8);
			break;

		case 5:     /* width of blit */
			m_pattern_width = data;
			break;

		case 6:     /* height of blit and initiator */
			m_pattern_height = data;
			execute_blit();
			break;
	}
}



/*************************************
 *
 *  Sparkle/star circuit
 *
 *************************************/

/*
    Counters at U15/U16:
        On VERTDR, load 0x33 into counters at U15/U16
        On HORZDR, clock counters, stopping at overflow to 0x00 (prevents sparkle in VBLANK)

    Shift registers at U17/U12/U11:
        cleared on vertdr
        clocked at 7M (pixel clock)
        taps from bit 4, 8, 12, 16 control sparkle intensity

    Shift registers at U17/U19/U18:
        cleared on reset
        clocked at 7M (pixel clock)
        if bits 0-7 == 0xfe, a star is generated

    Both shift registers are the same with identical feedback.
    We use one array to hold both shift registers. Bits 0-3
    bits hold the intensity, and bit 4 holds whether or not
    a star is present.

    We must use independent lookups for each case. For the star
    lookup, we need to compute the pixel index relative to the
    end of VBLANK and use that (which at 455*262 is guaranteed
    to be less than RNG_PERIOD).

    For the sparkle lookup, we need to compute the pixel index
    relative to the beginning of time and use that, mod RNG_PERIOD.
*/

void astrocde_state::init_sparklestar()
{
	uint32_t shiftreg;
	int i;

	/* reset global sparkle state */
	m_sparkle[0] = m_sparkle[1] = m_sparkle[2] = m_sparkle[3] = 0;

	/* allocate memory for the sparkle/star array */
	m_sparklestar = std::make_unique<uint8_t[]>(RNG_PERIOD);

	/* generate the data for the sparkle/star array */
	for (shiftreg = i = 0; i < RNG_PERIOD; i++)
	{
		uint8_t newbit;

		/* clock the shift register */
		newbit = ((shiftreg >> 12) ^ ~shiftreg) & 1;
		shiftreg = (shiftreg >> 1) | (newbit << 16);

		/* extract the sparkle/star intensity here */
		/* this is controlled by the shift register at U17/U19/U18 */
		m_sparklestar[i] = (((shiftreg >> 4) & 1) << 3) |
							(((shiftreg >> 12) & 1) << 2) |
							(((shiftreg >> 16) & 1) << 1) |
							(((shiftreg >> 8) & 1) << 0);

		/* determine the star enable here */
		/* this is controlled by the shift register at U17/U12/U11 */
		if ((shiftreg & 0xff) == 0xfe)
			m_sparklestar[i] |= 0x10;
	}
}



/*************************************
 *
 *  16-color video board registers
 *
 *************************************/

void astrocde_state::profpac_page_select_w(uint8_t data)
{
	m_profpac_readpage = data & 3;
	m_profpac_writepage = (data >> 2) & 3;
	m_profpac_vispage = (data >> 4) & 3;
}


uint8_t astrocde_state::profpac_intercept_r()
{
	return m_profpac_intercept;
}


void astrocde_state::profpac_screenram_ctrl_w(offs_t offset, uint8_t data)
{
	switch (offset)
	{
		case 0:     /* port 0xC0 - red component */
			m_profpac_palette[data >> 4] = (m_profpac_palette[data >> 4] & ~0xf00) | ((data & 0x0f) << 8);
			break;

		case 1:     /* port 0xC1 - green component */
			m_profpac_palette[data >> 4] = (m_profpac_palette[data >> 4] & ~0x0f0) | ((data & 0x0f) << 4);
			break;

		case 2:     /* port 0xC2 - blue component */
			m_profpac_palette[data >> 4] = (m_profpac_palette[data >> 4] & ~0x00f) | ((data & 0x0f) << 0);
			break;

		case 3:     /* port 0xC3 - set 2bpp to 4bpp mapping and clear intercepts */
			m_profpac_colormap[(data >> 4) & 3] = data & 0x0f;
			m_profpac_intercept = 0x00;
			break;

		case 4:     /* port 0xC4 - which half to read on a memory access */
			m_profpac_vw = data & 0x0f; /* refresh write enable lines TBD */
			m_profpac_readshift = 2 * ((data >> 4) & 1);
			break;

		case 5:     /* port 0xC5 - write enable and write mode */
			m_profpac_writemask = ((data & 0x0f) << 12) | ((data & 0x0f) << 8) | ((data & 0x0f) << 4) | ((data & 0x0f) << 0);
			m_profpac_writemode = (data >> 4) & 0x03;
			break;
	}
}



/*************************************
 *
 *  16-color video board VRAM access
 *
 *************************************/

uint8_t astrocde_state::profpac_videoram_r(offs_t offset)
{
	uint16_t temp = m_profpac_videoram[m_profpac_readpage * 0x4000 + offset] >> m_profpac_readshift;
	return ((temp >> 6) & 0xc0) | ((temp >> 4) & 0x30) | ((temp >> 2) & 0x0c) | ((temp >> 0) & 0x03);
}


/* All this information comes from decoding the PLA at U39 on the screen ram board */
void astrocde_state::profpac_videoram_w(offs_t offset, uint8_t data)
{
	uint16_t oldbits = m_profpac_videoram[m_profpac_writepage * 0x4000 + offset];
	uint16_t newbits, result = 0;

	/* apply the 2->4 bit expansion first */
	newbits = (m_profpac_colormap[(data >> 6) & 3] << 12) |
				(m_profpac_colormap[(data >> 4) & 3] << 8) |
				(m_profpac_colormap[(data >> 2) & 3] << 4) |
				(m_profpac_colormap[(data >> 0) & 3] << 0);

	/* there are 4 write modes: overwrite, xor, overlay, or underlay */
	switch (m_profpac_writemode)
	{
		case 0:     /* normal write */
			result = newbits;
			break;

		case 1:     /* xor write */
			result = newbits ^ oldbits;
			break;

		case 2:     /* overlay write */
			result  = ((newbits & 0xf000) == 0) ? (oldbits & 0xf000) : (newbits & 0xf000);
			result |= ((newbits & 0x0f00) == 0) ? (oldbits & 0x0f00) : (newbits & 0x0f00);
			result |= ((newbits & 0x00f0) == 0) ? (oldbits & 0x00f0) : (newbits & 0x00f0);
			result |= ((newbits & 0x000f) == 0) ? (oldbits & 0x000f) : (newbits & 0x000f);
			break;

		case 3: /* underlay write */
			result  = ((oldbits & 0xf000) != 0) ? (oldbits & 0xf000) : (newbits & 0xf000);
			result |= ((oldbits & 0x0f00) != 0) ? (oldbits & 0x0f00) : (newbits & 0x0f00);
			result |= ((oldbits & 0x00f0) != 0) ? (oldbits & 0x00f0) : (newbits & 0x00f0);
			result |= ((oldbits & 0x000f) != 0) ? (oldbits & 0x000f) : (newbits & 0x000f);
			break;
	}

	/* apply the write mask and store */
	result = (result & m_profpac_writemask) | (oldbits & ~m_profpac_writemask);
	m_profpac_videoram[m_profpac_writepage * 0x4000 + offset] = result;

	/* Intercept (collision) stuff */

	/* There are 3 bits on the register, which are set by various combinations of writes */
	if (((oldbits & 0xf000) == 0x2000 && (newbits & 0x8000) == 0x8000) ||
		((oldbits & 0xf000) == 0x3000 && (newbits & 0xc000) == 0x4000) ||
		((oldbits & 0x0f00) == 0x0200 && (newbits & 0x0800) == 0x0800) ||
		((oldbits & 0x0f00) == 0x0300 && (newbits & 0x0c00) == 0x0400) ||
		((oldbits & 0x00f0) == 0x0020 && (newbits & 0x0080) == 0x0080) ||
		((oldbits & 0x00f0) == 0x0030 && (newbits & 0x00c0) == 0x0040) ||
		((oldbits & 0x000f) == 0x0002 && (newbits & 0x0008) == 0x0008) ||
		((oldbits & 0x000f) == 0x0003 && (newbits & 0x000c) == 0x0004))
		m_profpac_intercept |= 0x01;

	if (((newbits & 0xf000) != 0x0000 && (oldbits & 0xc000) == 0x4000) ||
		((newbits & 0x0f00) != 0x0000 && (oldbits & 0x0c00) == 0x0400) ||
		((newbits & 0x00f0) != 0x0000 && (oldbits & 0x00c0) == 0x0040) ||
		((newbits & 0x000f) != 0x0000 && (oldbits & 0x000c) == 0x0004))
		m_profpac_intercept |= 0x02;

	if (((newbits & 0xf000) != 0x0000 && (oldbits & 0x8000) == 0x8000) ||
		((newbits & 0x0f00) != 0x0000 && (oldbits & 0x0800) == 0x0800) ||
		((newbits & 0x00f0) != 0x0000 && (oldbits & 0x0080) == 0x0080) ||
		((newbits & 0x000f) != 0x0000 && (oldbits & 0x0008) == 0x0008))
		m_profpac_intercept |= 0x04;
}
