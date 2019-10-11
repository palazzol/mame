// license:BSD-3-Clause
// copyright-holders:Couriersud, Olivier Galibert, R. Belmont
//============================================================
//
//  drawsdl.h - SDL software and OpenGL implementation
//
//  SDLMAME by Olivier Galibert and R. Belmont
//
//  yuvmodes by Couriersud
//
//============================================================

#pragma once

#ifndef __DRAWNEWVG__
#define __DRAWNEWVG__

class renderer_newvg : public osd_renderer
{
public:

	renderer_newvg(std::shared_ptr<osd_window> w, int extra_flags)
		: osd_renderer(w, extra_flags)
	{
		osd_printf_verbose("renderer_newvg::renderer_newvg()\n");
	}
	virtual ~renderer_newvg();

	static bool init(running_machine &machine);
	static void exit();

	virtual int create() override;
	virtual int draw(const int update) override;
	virtual int xy_to_render_target(const int x, const int y, int *xt, int *yt) override;
	virtual render_primitive_list *get_primitives() override;

};

#endif // __DRAWSDL1__
