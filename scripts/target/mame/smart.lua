-- license:BSD-3-Clause
-- copyright-holders:MAMEdev Team

---------------------------------------------------------------------------
--
--   smart.lua
--
--   Smart target makefile
--
---------------------------------------------------------------------------

dofile("arcade.lua")
dofile("mess.lua")

function createProjects_mame_smart(_target, _subtarget)
	project ("mame_smart")
	targetsubdir(_target .."_" .. _subtarget)
	kind (LIBTYPE)
	uuid (os.uuid("drv-mame_smart"))
	addprojectflags()
	precompiledheaders_novs()

	includedirs {
		MAME_DIR .. "src/osd",
		MAME_DIR .. "src/emu",
		MAME_DIR .. "src/devices",
		MAME_DIR .. "src/mame",
		MAME_DIR .. "src/lib",
		MAME_DIR .. "src/lib/util",
		MAME_DIR .. "src/lib/netlist",
		MAME_DIR .. "3rdparty",
		GEN_DIR  .. "mame/layout",
	}

files{
	MAME_DIR .. "src/mame/drivers/coleco.cpp",
	MAME_DIR .. "src/mame/includes/coleco.h",
	MAME_DIR .. "src/mame/machine/coleco.cpp",
	MAME_DIR .. "src/mame/machine/coleco.h",

	MAME_DIR .. "src/mame/drivers/gsz80.cpp",
	MAME_DIR .. "src/mame/drivers/gscpm.cpp",
	MAME_DIR .. "src/mame/drivers/gs6502.cpp",
	MAME_DIR .. "src/mame/drivers/gs6809.cpp",

	MAME_DIR .. "src/mame/drivers/intv.cpp",
	MAME_DIR .. "src/mame/includes/intv.h",
	MAME_DIR .. "src/mame/machine/intv.cpp",
	MAME_DIR .. "src/mame/video/intv.cpp",
	MAME_DIR .. "src/mame/video/stic.cpp",

	MAME_DIR .. "src/mame/drivers/osborne1.cpp",
	MAME_DIR .. "src/mame/includes/osborne1.h",
	MAME_DIR .. "src/mame/machine/osborne1.cpp",

	MAME_DIR .. "src/mame/drivers/phoenix.cpp",
	MAME_DIR .. "src/mame/includes/phoenix.h",
	MAME_DIR .. "src/mame/video/phoenix.cpp",
	MAME_DIR .. "src/mame/audio/phoenix.cpp",
	MAME_DIR .. "src/mame/audio/pleiads.cpp",

	MAME_DIR .. "src/mame/drivers/neogeo.cpp",
	MAME_DIR .. "src/mame/includes/neogeo.h",
	MAME_DIR .. "src/mame/video/neogeo.cpp",
	MAME_DIR .. "src/mame/drivers/neopcb.cpp",
	MAME_DIR .. "src/mame/video/neogeo_spr.cpp",
	MAME_DIR .. "src/mame/video/neogeo_spr.h",
	MAME_DIR .. "src/mame/machine/ng_memcard.cpp",
	MAME_DIR .. "src/mame/machine/ng_memcard.h",

	MAME_DIR .. "src/mame/drivers/trs80dt1.cpp",

	MAME_DIR .. "src/mame/drivers/blockade.cpp",

	MAME_DIR .. "src/mame/drivers/channelf.cpp",
	MAME_DIR .. "src/mame/includes/channelf.h",
	MAME_DIR .. "src/mame/video/channelf.cpp",
	MAME_DIR .. "src/mame/audio/channelf.cpp",

	MAME_DIR .. "src/mame/drivers/mouser.cpp",
	MAME_DIR .. "src/mame/video/mouser.cpp",

	MAME_DIR .. "src/mame/drivers/nova2001.cpp",
	MAME_DIR .. "src/mame/includes/nova2001.h",
	MAME_DIR .. "src/mame/video/nova2001.cpp",

	MAME_DIR .. "src/mame/drivers/stactics.cpp",
	MAME_DIR .. "src/mame/includes/stactics.h",
	MAME_DIR .. "src/mame/video/stactics.cpp",

	MAME_DIR .. "src/mame/drivers/starcrus.cpp",
	MAME_DIR .. "src/mame/includes/starcrus.h",
	MAME_DIR .. "src/mame/audio/nl_starcrus.cpp",
	MAME_DIR .. "src/mame/video/starcrus.cpp",

	MAME_DIR .. "src/mame/drivers/starshp1.cpp",
	MAME_DIR .. "src/mame/includes/starshp1.h",
	MAME_DIR .. "src/mame/video/starshp1.cpp",
	MAME_DIR .. "src/mame/audio/starshp1.cpp",

	MAME_DIR .. "src/mame/drivers/starwars.cpp",
	MAME_DIR .. "src/mame/includes/starwars.h",
	MAME_DIR .. "src/mame/machine/starwars.cpp",
	MAME_DIR .. "src/mame/audio/starwars.cpp",
	MAME_DIR .. "src/mame/includes/slapstic.h",
	MAME_DIR .. "src/mame/machine/slapstic.cpp",
	MAME_DIR .. "src/mame/video/avgdvg.h",
	MAME_DIR .. "src/mame/video/avgdvg.cpp",

	MAME_DIR .. "src/mame/drivers/turbo.cpp",
	MAME_DIR .. "src/mame/includes/turbo.h",
	MAME_DIR .. "src/mame/video/turbo.cpp",
	MAME_DIR .. "src/mame/audio/turbo.cpp",
	MAME_DIR .. "src/mame/machine/segacrpt_device.h",
	MAME_DIR .. "src/mame/machine/segacrpt_device.cpp",

	MAME_DIR .. "src/mame/drivers/ladybug.cpp",
	MAME_DIR .. "src/mame/includes/ladybug.h",
	MAME_DIR .. "src/mame/video/ladybug.cpp",

	MAME_DIR .. "src/mame/drivers/astrocde.cpp",
	MAME_DIR .. "src/mame/includes/astrocde.h",
	MAME_DIR .. "src/mame/video/astrocde.cpp",

	MAME_DIR .. "src/mame/drivers/chicago.cpp",

	MAME_DIR .. "src/mame/drivers/exidyttl.cpp",

	MAME_DIR .. "src/mame/audio/segag80.h",
	MAME_DIR .. "src/mame/audio/segag80.cpp",
	MAME_DIR .. "src/mame/audio/segaspeech.h",
	MAME_DIR .. "src/mame/audio/segaspeech.cpp",
	MAME_DIR .. "src/mame/audio/segausb.h",
	MAME_DIR .. "src/mame/audio/segausb.cpp",
	MAME_DIR .. "src/mame/audio/nl_astrob.cpp",
	MAME_DIR .. "src/mame/audio/nl_astrob.h",
	MAME_DIR .. "src/mame/audio/nl_elim.cpp",
	MAME_DIR .. "src/mame/audio/nl_elim.h",
	MAME_DIR .. "src/mame/audio/nl_spacfury.cpp",
	MAME_DIR .. "src/mame/audio/nl_spacfury.h",
	MAME_DIR .. "src/mame/machine/segag80.h",
	MAME_DIR .. "src/mame/machine/segag80.cpp",
	MAME_DIR .. "src/mame/video/segag80v.cpp",
	MAME_DIR .. "src/mame/includes/segag80v.h",
	MAME_DIR .. "src/mame/drivers/segag80v.cpp",

	MAME_DIR .. "src/mame/includes/zx.h",
	MAME_DIR .. "src/mame/video/zx.cpp",
	MAME_DIR .. "src/mame/machine/zx.cpp",
	MAME_DIR .. "src/mame/drivers/zx.cpp",

	MAME_DIR .. "src/mame/drivers/eva.cpp",

	MAME_DIR .. "src/mame/drivers/ti99_4x.cpp",

	MAME_DIR .. "src/mame/drivers/sstrangr.cpp",

	MAME_DIR .. "src/mame/includes/neogeo.h",
	MAME_DIR .. "src/mame/video/neogeo.cpp",
	MAME_DIR .. "src/mame/drivers/neogeo.cpp",

	MAME_DIR .. "src/mame/includes/vicdual.h",
	MAME_DIR .. "src/mame/audio/vicdual.h",
	MAME_DIR .. "src/mame/audio/vicdual.cpp",
	MAME_DIR .. "src/mame/audio/vicdual-97271p.h",
	MAME_DIR .. "src/mame/audio/vicdual-97271p.cpp",
	MAME_DIR .. "src/mame/audio/invinco.cpp",
	MAME_DIR .. "src/mame/audio/pulsar.cpp",
	MAME_DIR .. "src/mame/audio/depthch.cpp",
	MAME_DIR .. "src/mame/audio/carnival.cpp",
	MAME_DIR .. "src/mame/audio/nl_brdrline.cpp",
	MAME_DIR .. "src/mame/audio/nl_frogs.cpp",
	MAME_DIR .. "src/mame/video/vicdual.cpp",
	MAME_DIR .. "src/mame/video/vicdual-97269pb.h",
	MAME_DIR .. "src/mame/video/vicdual-97269pb.cpp",
	MAME_DIR .. "src/mame/drivers/vicdual.cpp"
}
end

function linkProjects_mame_smart(_target, _subtarget)
	links {
		"mame_smart",
	}
end
