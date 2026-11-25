-- SPDX-License-Identifier: GPL-2.0-or-later
-- Drive the terminal's own INSTALL through MAME input fields and VFD prompts.
local machine = manager.machine
local ports = machine.ioport.ports
local keypad = assert(ports[":KEYMATRIX"])
local security = assert(ports[":SECMASK"])
local hook = assert(keypad:field(0x80000))
local card = assert(ports[":CARDUI"]:field(1))
local smartcard = assert(ports[":CARDUI"]:field(2))
local coins = assert(ports[":COINUI"])
local keys = {}
for index, key in ipairs({"1", "2", "3", "4", "5", "6", "7", "8", "9", "*", "0", "#", "N"}) do
    keys[key] = assert(keypad:field(1 << (index - 1)))
end
keys["+"] = assert(keypad:field(0x10000))
keys["-"] = assert(keypad:field(0x20000))
keys.L = assert(keypad:field(0x40000))
for index = 1, 10 do
    keys[string.char(96 + index)] = assert(keypad:field(1 << (19 + index)))
end

local display = assert(os.getenv("COINLINE_VFD_LIVE"))
local history = assert(io.open(display .. ".events", "w"))
local phase = os.getenv("COINLINE_ACCEPTANCE_PROBE") == "1" and "boot probe" or
    os.getenv("COINLINE_ACCEPTANCE_HEADLESS") == "1" and "detached boot" or
    os.getenv("COINLINE_ACCEPTANCE_WARM") == "1" and "warm boot" or
    os.getenv("COINLINE_ACCEPTANCE_PORTABLE_STAGE") == "attached" and "portable display reconnect" or "boot"
local desk_card = os.getenv("COINLINE_ACCEPTANCE_DESK_CARD")
local test_card = os.getenv("COINLINE_ACCEPTANCE_TEST_CARD")
local phone = assert(os.getenv("COINLINE_ACCEPTANCE_LINE"))
local serial = assert(os.getenv("COINLINE_ACCEPTANCE_SERIAL"))
local pin = assert(os.getenv("COINLINE_ACCEPTANCE_PIN"))
local keypad_test = assert(os.getenv("COINLINE_ACCEPTANCE_KEYS"))
local dial = os.getenv("COINLINE_ACCEPTANCE_DIAL") or ""
local mag_call = os.getenv("COINLINE_ACCEPTANCE_MAG_CALL") == "1"
local answer_only = os.getenv("COINLINE_ACCEPTANCE_ANSWER_ONLY") == "1"
local co_log = assert(os.getenv("COINLINE_ACCEPTANCE_CO_LOG"))
local frame, delay, since, entered, coin_index = 0, 0, 0, 0, 1
local prompt = ""
local pending = {}
local denominations = {8, 16, 32}
local co_retry = false
local dial_retry = false
local later_board = os.getenv("COINLINE_MOTHERBOARD") == "later"
local coin_basic = os.getenv("COINLINE_ACCEPTANCE_COIN_BASIC") == "1"
local cold_ready_frame = later_board and 1080 or 480
local access_retry = false
local download_retry = false
local download_number_confirmed = false
local keypad_exercised = false
local smartcard_inserted = false
local coin_retried = false
local portable_open_retried = false

local watch_path = os.getenv("COINLINE_ACCEPTANCE_READ_WATCH")
if watch_path then
    local cpu = assert(machine.devices[":maincpu"])
    local space = assert(cpu.spaces["program"])
    local input = assert(io.open(watch_path, "r"))
    local output = assert(io.open(assert(os.getenv("COINLINE_ACCEPTANCE_READ_WATCH_OUT")), "w"))
    local taps = {}
    for line in input:lines() do
        local label, address = line:match("^(%S+)%s+([0-9a-fA-F]+)$")
        assert(label and address, "invalid memory watch entry")
        local location = tonumber(address, 16)
        taps[#taps + 1] = space:install_read_tap(location, location, label, function(offset, data)
            if phase == "downloading" or phase == "download follow-up" then
                local pc = cpu.state["PC"]
                output:write(frame, "\t", label, "\t", string.format("%05X\t%02X\t%04X", offset, data, pc and pc.value or 0), "\n")
                output:flush()
            end
            -- No return value: preserve the actual hardware read.
        end)
    end
    input:close()
    coinline_watch_stop_subscription = emu.add_machine_stop_notifier(function()
        for _, tap in ipairs(taps) do tap:remove() end
        output:close()
    end)
end

local function log(event, detail)
    history:write(frame, "\t", event, "\t", detail or "", "\n")
    history:flush()
end

local function change(next_phase)
    phase, entered = next_phase, frame
    log("phase", phase)
end

local function finish(status, reason)
    log(status, phase .. ": " .. reason)
    machine:exit()
end

local function press(sequence, label)
    log("input", label or sequence)
    local at = frame + 3
    for key in sequence:gmatch(".") do
        local field = assert(keys[key], "Unknown key " .. key)
        pending[at] = {field, 1}
        pending[at + 6] = {field, 0}
        at = at + 26
    end
    delay = at
end

local function pulse(field)
    field:set_value(1)
    pending[frame + 6] = pending[frame + 6] or {}
    local release = pending[frame + 6]
    release[#release + 1] = field
end

local function set_hook(lift)
    if (keypad:read() & 0x80000 ~= 0) ~= lift then pulse(hook) end
    log("input", lift and "lift handset" or "replace handset")
end

local function set_cabinet(open)
    for _, mask in ipairs({1, 2}) do
        if (security:read() & mask ~= 0) ~= open then pulse(assert(security:field(mask))) end
    end
    log("input", open and "unlock and open" or "close and lock")
end

local function has(text)
    return prompt:find(text, 1, true) ~= nil
end

local function co_log_contains(text)
    local file = io.open(co_log, "r")
    if not file then return false end
    local data = file:read("*a") or ""
    file:close()
    return data:find(text, 1, true) ~= nil
end

local function co_hook_state()
    local file = io.open(co_log, "r")
    if not file then return nil end
    local state
    for hook_state in (file:read("*a") or ""):gmatch("jack H([01])") do state = hook_state end
    file:close()
    return state
end

local function keypad_advanced()
    if keypad_exercised then
        log("observed", "keypad check advanced")
        keypad_exercised = false
    end
end

local function select_card(fixture)
    if fixture then
        local input = assert(io.open(fixture, "rb"))
        local output = assert(io.open(assert(os.getenv("COINLINE_MAG_CARD")), "wb"))
        output:write(input:read("*a"))
        output:close()
        input:close()
    end
end

log("phase", phase)
if (later_board and phase == "boot") or phase == "portable display reconnect" then
    set_cabinet(true)
end
emu.register_frame_done(function()
    frame = frame + 1
    local event = pending[frame]
    if event then
        if type(event[2]) == "number" then
            event[1]:set_value(event[2])
        else
            for _, field in ipairs(event) do field:clear_value() end
        end
        pending[frame] = nil
    end
    if frame % 3 ~= 0 then return end
    local file = io.open(display, "r")
    if file then
        local current = (file:read("*a") or ""):lower():gsub("[\r\n]+", " | ")
        file:close()
        if current ~= prompt then
            prompt, since = current, frame
            log("vfd", prompt)
        end
    end
    local download_limit = later_board and 36000 or 10800
    local limit = phase == "downloading" and download_limit or phase == "warm settle" and 3600
        or phase == "warm boot" and mag_call and 18000
        or phase == "call answer" and mag_call and 10800
        or phase == "warm boot" and later_board and 7200
        or phase == "peripherals" and coin_index > 1 and 4800 or 2400
    if frame - entered > limit then return finish("timeout", "waiting for next terminal prompt") end
    if frame < delay or frame - since < 12 then return end
    local idle = has("please lift") or has("lift receiver")
    local unavailable = has("not in service") or has("out of service")
    local completed = prompt:match("completed%s*:%s*(%d%d)")
    local failed = prompt:match("failed%s*:%s*(%d%d)")
    if phase == "download follow-up" then
        if has("fix number?") and has("#=n") and not download_number_confirmed then
            download_number_confirmed = true
            press("#", "keep the existing NCC number")
        elseif has("press * to start ncc") then
            press("*")
            change("downloading")
        elseif has("download in progress") or has("please wait") then
            change("downloading")
        elseif failed and failed ~= "34" then
            return finish("failed", "firmware status " .. failed)
        end
        return
    end
    if later_board and phase == "downloading" and failed == "34" and not download_retry
            and has("press * to try again") then
        download_retry = true
        log("observed", "firmware requested further table updates")
        press("*", "retry remaining table updates")
        change("download follow-up")
        return
    end
    if failed then return finish("failed", "firmware status " .. failed) end
    if phase == "detached boot" and frame >= 1200 and co_hook_state() == "0" then
        set_hook(true)
        delay = frame + 180
        change("detached dial")
    elseif phase == "detached dial" and co_hook_state() == "1" then
        press(dial, "dial local destination with portable display removed")
        change("detached answer")
    elseif phase == "detached answer" and co_log_contains("answered outbound") then
        log("observed", "local CO answered")
        set_hook(false)
        delay = frame + 120
        change("detached hangup")
    elseif phase == "detached hangup" and co_hook_state() == "0" then
        return finish("passed", "local call completed with portable display removed")
    elseif phase == "boot probe" and frame >= 480 and prompt:find("[%w\128-\255]") then
        if has("not responding") then return finish("failed", "startup reported an unresponsive device") end
        return finish("passed", "stable startup display")
    elseif phase == "warm boot" and idle then
        log("observed", "in service after restart")
        if dial == "" then return finish("passed", "warm service prompt") end
        -- Let the terminal finish any automatic NCC maintenance on-hook.
        delay = frame + 1800
        change("warm settle")
    elseif (phase == "warm settle" or phase == "dial retry") and idle then
        set_hook(true)
        delay = frame + 150
        change("dial")
    elseif phase == "dial" and (has("dial") or has("insert card")) then
        if mag_call then
            pulse(card)
            log("input", "present synthetic magnetic card for local call")
            change("call card")
        else
            press(dial, "dial local destination")
            change("call answer")
        end
    elseif phase == "call card" and has("thank you") and has("dial") then
        log("observed", "magnetic card accepted by terminal")
        press(dial, "dial configured local card-test destination")
        change("call answer")
    elseif phase == "call card" and (has("not an accepted card") or has("card rejected") or has("cannot read card")) then
        return finish("failed", "firmware rejected synthetic magnetic card")
    elseif phase == "dial" and idle and frame - entered >= 600 and not dial_retry then
        dial_retry = true
        set_hook(false)
        delay = frame + 120
        change("dial retry")
    elseif phase == "call answer" and co_log_contains("answered outbound") then
        log("observed", "local CO answered")
        if mag_call then
            delay = frame + 600
            change("card call connected")
        else
            set_hook(false)
            change("call hangup")
        end
    elseif phase == "call answer" and mag_call and (has("no access") or has("not authorized") or has("card rejected")) then
        return finish("failed", "firmware declined the configured synthetic call")
    elseif phase == "card call connected" then
        set_hook(false)
        change("call hangup")
    elseif phase == "call hangup" and idle then
        if mag_call then
            log("observed", "returned to service after magnetic-card call")
            delay = frame + 1800
            change("card maintenance wait")
        else
            return finish("passed", "returned to service after call")
        end
    elseif phase == "card maintenance wait" and idle then
        return finish("passed", "local card call returned to service; host transaction evidence separate")
    elseif phase == "boot" and frame >= cold_ready_frame and has("use # to install") then
        if not co_log_contains("jack H0") then return end
        log("observed", "cold boot display")
        press("#", "start INSTALL from the startup menu")
        change("install start")
    elseif phase == "boot" and frame >= cold_ready_frame and has("enter pin") then
        if not co_log_contains("jack H0") then return end
        log("observed", "cold boot display")
        change("pin")
    elseif phase == "boot" and frame >= cold_ready_frame
            and (unavailable or has("not installed")) then
        if not co_log_contains("jack H0") then return end
        log("observed", "cold boot display")
        set_hook(true)
        delay = frame + 48
        change("lifted")
    elseif phase == "boot" and coin_basic and frame >= cold_ready_frame
            and not prompt:find("[%w\128-\255]") then
        if not co_log_contains("jack H0") then return end
        set_hook(true)
        delay = frame + 48
        change("portable display lifted")
    elseif phase == "portable display lifted" then
        set_hook(false)
        delay = frame + 48
        change("portable display access")
    elseif phase == "portable display access" then
        -- Coin Basic without a display accepts Craft access and PIN before unlock.
        -- The virtual portable VFD is connected to the standard display port.
        press("2727378" .. pin .. "*", "portable display Craft access and PIN")
        change("portable display unlock")
    elseif phase == "portable display unlock" then
        set_cabinet(true)
        delay = frame + 120
        change("portable display power cycle")
    elseif phase == "portable display power cycle" then
        -- End the powered session so the next process starts only with the
        -- battery-backed state written by the terminal.
        return finish("power-cycle", "attach portable display while powered off")
    elseif phase == "portable display reconnect" and frame >= cold_ready_frame
            and co_log_contains("jack H0") then
        -- The internal display permits the housing to close with the lock open.
        if prompt:find("[%w\128-\255]") then log("observed", "cold boot display") end
        if (security:read() & 2) ~= 0 then pulse(assert(security:field(2))) end
        log("input", "close housing, leave lock open")
        delay = frame + 120
        change("install")
    elseif phase == "install" and coin_basic and not access_retry and frame - entered >= 480
            and (unavailable or not prompt:find("[%w\128-\255]")) then
        access_retry = true
        set_cabinet(false)
        delay = frame + 120
        change("portable display reentry")
    elseif phase == "portable display reentry" then
        press("2727378" .. pin .. "*", "re-enter Craft after reconnecting portable display")
        change("portable display reunlock")
    elseif phase == "portable display reunlock" then
        if has("use key") or has("open the terminal") then
            log("observed", "Craft accepted access input")
        end
        if (security:read() & 1) == 0 then pulse(assert(security:field(1))) end
        log("input", "unlock with internal display attached")
        change("install")
    elseif phase == "install" and coin_basic and access_retry and not portable_open_retried
            and has("open the terminal") and frame - entered >= 120 then
        portable_open_retried = true
        set_cabinet(true)
        change("install")
    elseif phase == "lifted" then
        set_hook(false)
        delay = frame + 48
        change("access")
    elseif phase == "access" and has("use # to install") then
        press("#", "start INSTALL from the startup menu")
        change("install start")
    elseif phase == "access" and (unavailable or has("not installed")
            or (access_retry and not prompt:find("[%w\128-\255]"))) then
        press("2727378", "Craft access")
        change("pin")
    elseif (phase == "pin" or phase == "install start" or phase == "access") and has("enter pin") then
        log("observed", "Craft accepted access input")
        press(pin .. "*", "Craft PIN")
        change("unlock")
    elseif later_board and not access_retry and (phase == "pin" or phase == "install start")
            and frame - entered >= 720 and (unavailable or not prompt:find("[%w\128-\255]")) then
        access_retry = true
        set_cabinet(true)
        delay = frame + 120
        change("access")
    elseif phase == "unlock" and has("insert") and has("key card") then
        if not desk_card then return finish("blocked", "terminal requests a Desk key-card fixture") end
        select_card(desk_card)
        pulse(card)
        log("input", "insert and remove Desk key card")
        change("install")
    elseif phase == "unlock" and (has("use key") or has("open the terminal")) then
        set_cabinet(true)
        change("install")
    elseif (phase == "unlock" or phase == "install" or phase == "install start") and has("press # to continue") then
        press("#", "acknowledge the Craft version banner")
        change("install menu")
    elseif (phase == "unlock" or phase == "install" or phase == "install menu") and has("use # to install") then
        press("#")
        change("co check")
    elseif (phase == "install menu" or phase == "install start") and has("line check") and has("go offhook") then
        change("co check")
    elseif phase == "co check" and has("go offhook") and frame - since >= 330 then
        set_hook(true)
        change("co result")
    elseif phase == "co result" and has("go offhook") and frame - entered >= 420 and not co_retry then
        co_retry = true
        set_hook(false)
        delay = frame + 75
        change("co check")
    elseif (phase == "co result" or phase == "answer result") and completed == "00" then
        log("observed", phase .. " completed 00")
        press("*")
        change(phase == "co result" and "co hangup" or "answer hangup")
    elseif (phase == "co hangup" or phase == "answer hangup") and has("back onhook") then
        set_hook(false)
        change(phase == "co hangup" and "line number" or "download")
    elseif phase == "line number" and (has("line tel") or has("telephone")) then
        press(phone .. "*", "terminal line number")
        change("serial")
    elseif phase == "serial" and has("serial") then
        press(serial .. "*", "terminal serial number")
        change("ncc")
    elseif phase == "ncc" and (has("ncc") or has("datapac") or has("manager")) then
        press("2130706433*", "loopback NCC number")
        change("answer check")
    elseif phase == "answer check" and has("go offhook") then
        set_hook(true)
        change("answer result")
    elseif (phase == "download" or phase == "answer check") and has("start ncc") and has("download") then
        if answer_only then
            return finish("passed", "answer supervision completed; NCC download not started")
        end
        press("*")
        change("downloading")
    elseif phase == "downloading" and completed and has("continue") then
        log("observed", "download completed " .. completed)
        press("*")
        change("keypad")
    elseif phase == "keypad" and has("offhook") and (has("press") or has("buttons")) then
        set_hook(true)
        delay = frame + 36
        change("keypad test")
    elseif phase == "keypad test" and has("onhook") then
        press(keypad_test)
        keypad_exercised = true
        change("keypad hangup")
    elseif phase == "keypad hangup" then
        set_hook(false)
        change("peripherals")
    elseif (phase == "keypad" or phase == "peripherals") and has("insert") and has("smart card") then
        keypad_advanced()
        pulse(smartcard)
        smartcard_inserted = true
        log("input", "insert test smart card")
        change("smart card result")
    elseif phase == "smart card result" and has("remove") and has("card") and smartcard_inserted then
        pulse(smartcard)
        smartcard_inserted = false
        log("input", "remove test smart card")
    elseif phase == "smart card result" and has("continue") then
        log("observed", has("fail") and "smart card check failed" or "smart card check advanced")
        if smartcard_inserted then
            pulse(smartcard)
            smartcard_inserted = false
            log("input", "remove test smart card")
        end
        press("*")
        change("peripherals")
    elseif (phase == "keypad" or phase == "peripherals") and has("insert") and has("card") then
        keypad_advanced()
        select_card(test_card)
        pulse(card)
        log("input", "insert and remove test card")
        change("card result")
    elseif phase == "card result" and (has("not an accepted card") or has("card rejected")) then
        return finish("failed", "firmware rejected the test card")
    elseif phase == "card result" and has("continue") then
        log("observed", has("fail") and "card check failed" or "card check advanced")
        press("*")
        change("peripherals")
    elseif (phase == "peripherals" or phase == "keypad") and has("deposit") and has("coin") then
        if later_board and has("calibration") and frame - since < 360 then return end
        keypad_advanced()
        local slot = denominations[coin_index]
        if not slot then return finish("failed", "coin prompt persists after all denominations") end
        pulse(assert(coins:field(slot)))
        log("input", ({"nickel", "dime", "quarter"})[coin_index])
        coin_index = coin_index + 1
        coin_retried = false
        change("coin settling")
    elseif phase == "coin settling" and not (has("deposit") and has("coin")) then
        change("peripherals")
    elseif phase == "coin settling" and later_board and not coin_retried and frame - entered >= 240 then
        -- A coin inserted before acceptance starts may fall into the return.
        -- One further physical insertion is permitted while the same prompt stays visible.
        pulse(assert(coins:field(denominations[coin_index - 1])))
        coin_retried = true
        log("input", "repeat coin insertion at unchanged prompt")
    elseif phase == "peripherals" and completed == "00" and has("continue") then
        if coin_index > 1 then log("observed", "coin check completed 00") end
        press("*")
        change("close terminal")
    elseif phase == "peripherals" and has("install is complete") and has("#=exit") then
        keypad_advanced()
        log("observed", "INSTALL complete")
        press("#")
        change("service")
    elseif phase ~= "service" and has("install") and has("complete") and has("close") then
        keypad_advanced()
        log("observed", "INSTALL complete")
        set_cabinet(false)
        change("service")
    elseif phase == "service" and idle then
        return finish("passed", "installed terminal in service")
    end
end, "firmware acceptance")
