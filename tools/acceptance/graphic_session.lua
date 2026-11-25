-- SPDX-License-Identifier: GPL-2.0-or-later
-- Exercise a running graphical terminal through physical input fields.
local machine = manager.machine
local ports = machine.ioport.ports
local keypad = assert(ports[":KEYMATRIX"])
local security = assert(ports[":SECMASK"])
local softkeys = assert(ports[":TERMINAL21_SOFTKEYS"])
local card = assert(ports[":CARDUI"])
local coins = assert(ports[":COINUI"])
local offline_line = os.getenv("COINLINE_GRAPHIC_OFFLINE_LINE") == "1" and assert(ports[":LINECTRL"]) or nil
local screen = assert(machine.screens[":screen"])
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

local commands = assert(os.getenv("COINLINE_GRAPHIC_COMMANDS"))
local history = assert(io.open(assert(os.getenv("COINLINE_GRAPHIC_EVENTS")), "w"))
local frame, next_frame, processed = 0, 0, 0
local snapshot_interval = tonumber(os.getenv("COINLINE_GRAPHIC_SNAPSHOT_INTERVAL")) or 180
local pending = {}
local watch_path = os.getenv("COINLINE_GRAPHIC_READ_WATCH")
if watch_path then
    local cpu = assert(machine.devices[":maincpu"])
    local space = assert(cpu.spaces["program"])
    local input = assert(io.open(watch_path, "r"))
    local output = assert(io.open(assert(os.getenv("COINLINE_GRAPHIC_READ_WATCH_OUT")), "w"))
    local taps, samples, reading = {}, 0, false
    output:write("frame\tlabel\tphysical\tpc\tsnapshot\n")
    for line in input:lines() do
        local label, address, fields = line:match("^(%S+)%s+([0-9a-fA-F]+)%s*(.*)$")
        assert(label and address, "invalid read watch entry")
        local location, snapshot = tonumber(address, 16), {}
        assert(location < 0x100000, "physical watch address outside bus")
        for field in fields:gmatch("%S+") do
            local value = assert(tonumber(field, 16), "invalid physical snapshot address")
            assert(value >= 0 and value < 0x100000, "physical snapshot address outside bus")
            snapshot[#snapshot + 1] = value
        end
        assert(#taps < 32 and #snapshot <= 32, "read watch exceeds diagnostic bound")
        taps[#taps + 1] = space:install_read_tap(location, location, label, function(offset)
            if reading or samples >= 2000 or keypad:read() & 0x80000 == 0 then return end
            reading, samples = true, samples + 1
            local values = {}
            for _, addr in ipairs(snapshot) do
                values[#values + 1] = string.format("%05X:%02X", addr, space:read_u8(addr))
            end
            local pc = cpu.state["PC"]
            output:write(frame, "\t", label, "\t", string.format("%05X\t%04X\t", offset, pc and pc.value or 0),
                table.concat(values, " "), "\n")
            output:flush()
            reading = false
            -- No return value: do not modify the hardware read.
        end)
    end
    input:close()
    coinline_graphic_watch_stop = emu.add_machine_stop_notifier(function()
        for _, tap in ipairs(taps) do tap:remove() end
        output:close()
    end)
end
local function log(action, detail)
    history:write(frame, "\t", action, "\t", detail or "", "\n")
    history:flush()
end

local function schedule(at, field, value)
    pending[at] = pending[at] or {}
    table.insert(pending[at], {field, value})
end

local function pulse(field, at)
    schedule(at, field, 1)
    schedule(at + 6, field, 0)
end

local function sequence(value, label)
    local at = frame + 3
    for key in value:gmatch(".") do
        pulse(assert(keys[key], "invalid keypad key"), at)
        at = at + 26
    end
    next_frame = at
    log("input", label)
end

local function cabinet(open)
    local order = open and {1, 2} or {2, 1}
    for index, mask in ipairs(order) do
        if (security:read() & mask ~= 0) ~= open then
            pulse(assert(security:field(mask)), frame + 3 + (index - 1) * 18)
        end
    end
    next_frame = frame + 45
    log("input", open and "unlock and open" or "close and lock")
end

local function execute(line)
    local action, arg = line:match("^(%S+)%s*(.*)$")
    if not action or action:sub(1, 1) == ";" then return end
    if action == "keyseq" then
        sequence(arg, "keypad sequence")
    elseif action == "line" or action == "serial" or action == "pin" then
        local key = ({line="LINE", serial="SERIAL", pin="PIN"})[action]
        sequence(assert(os.getenv("COINLINE_ACCEPTANCE_" .. key), "missing lab settings") ..
            (action == "pin" and "" or "*"), action)
    elseif action == "ncc" then
        sequence("2130706433*", "loopback NCC number")
    elseif action == "hook" then
        assert(arg == "off" or arg == "on")
        if (keypad:read() & 0x80000 ~= 0) ~= (arg == "off") then
            pulse(assert(keypad:field(0x80000)), frame + 3)
        end
        next_frame = frame + 30
        log("input", "hook " .. arg)
    elseif action == "cabinet" then
        assert(arg == "open" or arg == "close")
        cabinet(arg == "open")
    elseif action == "mag" or action == "smart" then
        pulse(assert(card:field(action == "mag" and 1 or 2)), frame + 3)
        next_frame = frame + 30
        log("input", action)
    elseif action == "coin" then
        pulse(assert(coins:field(assert(({nickel=8, dime=16, quarter=32})[arg]))), frame + 3)
        next_frame = frame + 30
        log("input", "coin " .. arg)
    elseif action == "soft" then
        local position = assert(tonumber(arg))
        assert(position >= 1 and position <= 12 and position % 1 == 0)
        pulse(assert(softkeys:field(1 << (position - 1))), frame + 3)
        next_frame = frame + 30
        log("input", "softkey " .. arg)
    elseif action == "wait" then
        local frames = assert(tonumber(arg))
        assert(frames >= 0 and frames % 1 == 0)
        next_frame = frame + frames
    elseif action == "snapshot" then
        assert(arg:match("^[%w_-]+$"), "invalid snapshot name")
        screen:snapshot(arg .. ".png")
        log("snapshot", arg)
    elseif action == "exit" then
        log("exit")
        machine:exit()
    else
        error("unsupported command: " .. action)
    end
end

log("start")
if offline_line then offline_line:field(1):set_value(1) end
if os.getenv("COINLINE_GRAPHIC_OPEN") == "1" then cabinet(true) end
emu.register_frame_done(function()
    frame = frame + 1
    if pending[frame] then
        for _, event in ipairs(pending[frame]) do
            if event[2] == 1 then event[1]:set_value(1) else event[1]:clear_value() end
        end
        pending[frame] = nil
    end
    if snapshot_interval > 0 and frame % snapshot_interval == 0 then
        screen:snapshot("live.png")
    end
    if frame < next_frame or frame % 6 ~= 0 then return end
    local file = io.open(commands, "r")
    if not file then return end
    local index, next_line = 0, nil
    for line in file:lines() do
        index = index + 1
        if index > processed then next_line = line; break end
    end
    file:close()
    if next_line then
        processed = processed + 1
        execute(next_line)
    end
end, "graphical hardware acceptance session")
