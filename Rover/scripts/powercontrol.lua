-- use params as variables
function bind_param(name)
   local p = Parameter()
   assert(p:init(name), string.format('could not find %s parameter', name))
   return p
end

-- CONSTANTS

-- MODES

local MODE_MANUAL = 0
local MODE_AUTO = 10
local MODE_GUIDED = 15

 -- BATTERY LEVELS
local BAT_FULLY_CHARGED = 16.0
local BAT_CRITICAL = 13.0


 -- STRATEGY STATES
local STATE_CRITICAL = 0
local STATE_LOST_VOLTAGE = 1
local STATE_OVERRIDE = 2
local STATE_CRUISING = 3
local STATE_STANDBY = 4 -- DO WE WANT THIS?

local state = STATE_STANDBY

-- Power if we happen to loose battery readings
local POWER_SAFE = 30.0

-- CREATE CUSTOM PARAMS
local PARAM_TABLE_KEY = 72
assert(param:add_table(PARAM_TABLE_KEY, "SOLAR_", 2), 'could not add param table')

assert(param:add_param(PARAM_TABLE_KEY, 1, 'OVERRIDE', 0), 'could not add OVERRIDE PARAM')
assert(param:add_param(PARAM_TABLE_KEY, 2, 'KP', 20), 'could not add KP PARAM')

local override = bind_param("SOLAR_OVERRIDE")
local kp = bind_param("SOLAR_KP")
-----------------------------------------

-- Check if we lost voltage/current measurements
function lost_voltage()
    local voltage = battery:voltage(0)
    return voltage < 5.0 or voltage > 18.0
end


function update_power_strategy()
    local voltage = battery:voltage(0)
    if voltage > BAT_CRITICAL then
        state = STATE_CRUISING
    elseif voltage < BAT_CRITICAL then
        state = STATE_CRITICAL
    end
    if override:get() > 0 then
        state = STATE_OVERRIDE
    end
    if lost_voltage() then
        state = STATE_LOST_VOLTAGE
    end
end

function update_critical()
    if arming:is_armed() then
        arming:disarm()
    end
end

function update_cruising()
    local voltage = battery:voltage(0)
    param:set('BAT_DES_POWER_W', math.max((voltage - BAT_CRITICAL)*kp:get(), 0))
end

function update_lost_voltage()
    param:set('BAT_DES_POWER_W', POWER_SAFE)
end

function update_override()
    param:set('BAT_DES_POWER_W', override:get())
    if override:get() <= 0 then
        state = STATE_CRUISING
    end
end

function update_standby()
    if arming:is_armed() then
        state = STATE_CRUISING
    end
end

function power_control()
    local watts = battery:current_amps(0) * battery:voltage(0)
    local deadzone = 3
    local current_throttle_limit =  param:get('MOT_THR_MAX')
    local desired_power = param:get('BAT_DES_POWER_W')
    local new_current_throttle_limit = current_throttle_limit
    local mode = vehicle:get_mode()
    if not (mode == MODE_MANUAL or mode == MODE_AUTO or mode == MODE_MANUAL) then
        return update, 1000
    end
    if watts > desired_power + deadzone then
        new_current_throttle_limit = math.max(current_throttle_limit - 1, 10)
        param:set('MOT_THR_MAX', new_current_throttle_limit)
    end
    if watts < desired_power - deadzone then
        new_current_throttle_limit = math.min(current_throttle_limit + 1, 100)
        param:set('MOT_THR_MAX', new_current_throttle_limit)

    end
end

function update() -- this is the loop which periodically runs
    update_power_strategy()
    if state == STATE_CRITICAL then
        update_critical()
    elseif state == STATE_CRUISING then
        update_cruising()
    elseif state == STATE_LOST_VOLTAGE then
        update_lost_voltage()
    elseif state == STATE_OVERRIDE then
        update_override()
    elseif state == STATE_STANDBY then
        update_standby()
    end
    power_control()
    return update, 1000 -- reschedules the loop
end

return update() -- run immediately before starting to reschedule
