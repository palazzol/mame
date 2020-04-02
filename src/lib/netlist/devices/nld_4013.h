// license:GPL-2.0+
// copyright-holders:Couriersud
/*
 * nld_4013.h
 *
 *  CD4013: Dual Positive-Edge-Triggered D Flip-Flops
 *          with Set, Reset and Complementary Outputs
 *
 *          +--------------+
 *     CLR1 |1     ++    14| VCC
 *       D1 |2           13| CLR2
 *     CLK1 |3           12| D2
 *      PR1 |4    4013   11| CLK2
 *       Q1 |5           10| PR2
 *      Q1Q |6            9| Q2
 *      GND |7            8| Q2Q
 *          +--------------+
 *
 *          +-----+-----+-----+---++---+-----+
 *          | SET | RES | CLK | D || Q | QQ  |
 *          +=====+=====+=====+===++===+=====+
 *          |  1  |  0  |  X  | X || 1 |  0  |
 *          |  0  |  1  |  X  | X || 0 |  1  |
 *          |  1  |  1  |  X  | X || 1 |  1  | (*)
 *          |  0  |  0  |  R  | 1 || 1 |  0  |
 *          |  0  |  0  |  R  | 0 || 0 |  1  |
 *          |  0  |  0  |  0  | X || Q0| Q0Q |
 *          +-----+-----+-----+---++---+-----+
 *
 *  (*) This configuration is not stable, i.e. it will not persist
 *  when either the preset and or clear inputs return to their inactive (high) level
 *
 *  Q0 The output logic level of Q before the indicated input conditions were established
 *
 *  R:  0 -. 1
 *
 *  Naming conventions follow National Semiconductor datasheet
 *
 *  FIXME: Check that (*) is emulated properly
 */

#ifndef NLD_4013_H_
#define NLD_4013_H_

#include "netlist/nl_setup.h"

#define CD4013(name, cCLK, cD, cRESET, cSET)                                   \
		NET_REGISTER_DEV(CD4013, name)                                         \
		NET_CONNECT(name, GND, GND)                                            \
		NET_CONNECT(name, VCC, VCC)                                            \
		NET_CONNECT(name, CLK, cCLK)                                           \
		NET_CONNECT(name, D,  cD)                                              \
		NET_CONNECT(name, SET,  cSET)                                          \
		NET_CONNECT(name, RESET,  cRESET)

#define CD4013_DIP(name)                                                       \
		NET_REGISTER_DEV(CD4013_DIP, name)

#endif /* NLD_4013_H_ */
