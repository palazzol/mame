// license:BSD-3-Clause
// copyright-holders:Frank Palazzolo
//============================================================
//
//  renderer_newvg.cpp - SDL software and OpenGL implementation
//
//  SDLMAME by Olivier Galibert and R. Belmont
//
//  yuvmodes by Couriersud
//
//============================================================

// standard C headers
#include <math.h>
#include <stdio.h>

// MAME headers
#include "emu.h"
#include "ui/uimain.h"
#include "rendersw.hxx"

// OSD headers
#include "modules/osdwindow.h"
//#include "osdsdl.h"
//#include "window.h"

#include "drawnewvg.h"
//#include "modules/monitor/monitor_module.h"

//============================================================
//  DEBUGGING
//============================================================

//============================================================
//  CONSTANTS
//============================================================

//============================================================
//  PROTOTYPES
//============================================================

// Static declarations

bool renderer_newvg::init(running_machine &machine)
{
	osd_printf_verbose("renderer_newvg::init()\n");
	return false;
}

void renderer_newvg::exit()
{
	osd_printf_verbose("renderer_newvg::exit()\n");
}

int renderer_newvg::create()
{
	osd_printf_verbose("renderer_newvg::create()\n");
	return 0;
}

//============================================================
//  DESTRUCTOR
//============================================================

renderer_newvg::~renderer_newvg()
{
	osd_printf_verbose("renderer_newvg::~renderer_newvg()\n");
}

//============================================================
//  renderer_newvg::xy_to_render_target
//============================================================

int renderer_newvg::xy_to_render_target(int x, int y, int *xt, int *yt)
{
	//*xt = x - m_last_hofs;
	//*yt = y - m_last_vofs;
	//if (*xt<0 || *xt >= m_blit_dim.width())
	//	return 0;
	//if (*yt<0 || *yt >= m_blit_dim.height())
	//	return 0;
	osd_printf_verbose("renderer_newvg::xy_to_render_target()\n");
	return 1;
}

//============================================================
//  renderer_newvg::draw
//============================================================

int renderer_newvg::draw(int update)
{
	osd_printf_verbose("\nrenderer_newvg::draw()\n");

	auto win = assert_window();

	osd_dim wdim = win->get_size();

	if (has_flags(FI_CHANGED))
	{
		//destroy_all_textures();
		//m_width = wdim.width();
		//m_height = wdim.height();
		//m_blittimer = 3;
		//m_init_context = 1;
		clear_flags(FI_CHANGED);
	}

	osd_printf_verbose("Size=(%d,%d)\n",wdim.width(),wdim.height());

	win->m_primlist->acquire_lock();

	// now draw
	for (render_primitive &prim : *win->m_primlist)
	{
		switch (prim.type)
		{
			case render_primitive::LINE:
				osd_printf_verbose("LINE: Color=(%f,%f,%f,%f), Bounds=(%f,%f) - (%f,%f)\n",
					prim.color.r, prim.color.g, prim.color.b, prim.color.a,
					prim.bounds.x0, prim.bounds.y0, prim.bounds.x1, prim.bounds.y1);
			break;
			case render_primitive::QUAD:
				osd_printf_verbose("QUAD: ...\n");
			break;
			default:
			break;
		}
	}

	win->m_primlist->release_lock();

	return 0;
}

render_primitive_list *renderer_newvg::get_primitives()
{
	osd_printf_verbose("renderer_newvg::get_primitives()\n");

	auto win = try_getwindow();
	if (win == nullptr)
		return nullptr;

	osd_dim nd = win->get_size();
	//if (nd != m_blit_dim)
	//{
	//	m_blit_dim = nd;
	//	notify_changed();
	//}
	win->target()->set_bounds(nd.width(), nd.height(), win->pixel_aspect());
	return &win->target()->get_primitives();
}
