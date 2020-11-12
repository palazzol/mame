// license:BSD-3-Clause
// copyright-holders:Nicola Salmoria, Aaron Giles, Nathan Woods
/***************************************************************************

    ui/videoopt.h

    Internal menus for video options

***************************************************************************/
#ifndef MAME_FRONTEND_UI_VIDEOOPT_H
#define MAME_FRONTEND_UI_VIDEOOPT_H

#pragma once

#include "ui/menu.h"


namespace ui {

class menu_video_targets : public menu
{
public:
	menu_video_targets(mame_ui_manager &mui, render_container &container);
	virtual ~menu_video_targets() override;

private:
	virtual void populate(float &customtop, float &custombottom) override;
	virtual void handle() override;
};


class menu_video_options : public menu
{
public:
	menu_video_options(mame_ui_manager &mui, render_container &container, render_target &target);
	virtual ~menu_video_options() override;

private:
	virtual void populate(float &customtop, float &custombottom) override;
	virtual void handle() override;

	render_target &m_target;
};

} // namespace ui

#endif  // MAME_FRONTEND_UI_VIDEOOPT_H
