-- SPDX-License-Identifier: GPL-2.0-or-later
-- Exercise a running graphical terminal through physical input fields.
local machine = manager.machine
local ports = machine.ioport.ports
local keypad = assert(ports[":KEYMATRIX"])
local security = assert(ports[":SECMASK"])
local softkeys = assert(ports[":TERMINAL21_SOFTKEYS"])
local card = assert(ports[":CARDUI"])
local coins = assert(ports[":COINUI"])
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
local pending = {}
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
if os.getenv("COINLINE_GRAPHIC_OPEN") == "1" then cabinet(true) end
emu.register_frame_done(function()
    frame = frame + 1
    if pending[frame] then
        for _, event in ipairs(pending[frame]) do
            if event[2] == 1 then event[1]:set_value(1) else event[1]:clear_value() end
        end
        pending[frame] = nil
    end
    if frame % 180 == 0 then
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
