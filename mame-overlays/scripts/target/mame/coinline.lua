-- license:BSD-3-Clause
-- CoinLine Terminal Emulator — minimal MAME SUBTARGET=coinline
-- Installed into MAME tree by tools/windows/overlay-coinline-driver.ps1

CPUS["Z180"] = true
CPUS["Z80"] = true

MACHINES["Z80DAISY"] = true
MACHINES["GEN_LATCH"] = true

-- Voiceware / SPEAKER + uPD7759 (scripts/src/sound.lua gates upd7759.cpp + spkrdev.cpp)
SOUNDS["SPEAKER"] = true
SOUNDS["BEEP"] = true
SOUNDS["UPD7759"] = true

function createProjects_mame_coinline(_target, _subtarget)
	project ("mame_coinline")
	targetsubdir(_target .. "_" .. _subtarget)
	kind (LIBTYPE)
	uuid (os.uuid("drv-mame-coinline"))
	addprojectflags()
	precompiledheaders_novs()

	includedirs {
		MAME_DIR .. "src/osd",
		MAME_DIR .. "src/emu",
		MAME_DIR .. "src/devices",
		MAME_DIR .. "src/mame/shared",
		MAME_DIR .. "src/lib",
		MAME_DIR .. "src/lib/util",
		MAME_DIR .. "3rdparty",
		GEN_DIR .. "mame/layout",
	}

	files {
		MAME_DIR .. "src/mame/coinline/**.cpp",
		MAME_DIR .. "src/mame/coinline/**.h",
	}
end

function linkProjects_mame_coinline(_target, _subtarget)
	links {
		"mame_coinline",
	}
end
