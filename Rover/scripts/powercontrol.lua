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
local PARAM_TABLE_KEY_2 = 75
local DEFAULT_WAIT_TIME = 5*60 -- 5 minutes to arm and do its thing.

assert(param:add_table(PARAM_TABLE_KEY, "SOLAR_", 3), 'could not add param table')
assert(param:add_param(PARAM_TABLE_KEY, 1, 'OVERRIDE', 0), 'could not add OVERRIDE PARAM')
assert(param:add_param(PARAM_TABLE_KEY, 2, 'KP', 20), 'could not add KP PARAM')
assert(param:add_param(PARAM_TABLE_KEY, 3, 'MAX_PERC', 30), 'could not add MA_PERC PARAM')
assert(param:add_table(PARAM_TABLE_KEY_2, "MISSION_", 1), 'could not add param table')
assert(param:add_param(PARAM_TABLE_KEY_2, 1, 'PAUSE_S', DEFAULT_WAIT_TIME), 'could not add PAUSE param')

local override = bind_param("SOLAR_OVERRIDE")
local kp = bind_param("SOLAR_KP")
local max_thrust_percentage = bind_param("SOLAR_MAX_PERC")

-- start VERY slowly
param:set('MOT_THR_MAX', 3)

persistence_file = 'last_waypoint.txt'
ARMING_SWITCH = 27

INPUT = 0
OUPUT = 1


gpio:pinMode(ARMING_SWITCH, INPUT)

gcs:send_text(6, 'Started mission persistency layer')
gcs:send_text(6, 'using file ' .. persistence_file)


function reload_waypoint()
    file = assert(io.open(persistence_file, "r"), 'Could open :' .. persistence_file)
    if not file then -- file doesnt exist yet?
      gcs:send_text(0, 'unable to open file, giving up')
      return
    end
    io.input(file)
    local waypoint_number = io.read("*number")
    if waypoint_number then
      if mission:get_current_nav_index() ~= waypoint_number then
        -- pcall is a try/catch wrapper for lua.
        -- This ensures the code doesn`t hang if the waypoint saved is invalid
        pcall(mission:set_current_cmd(waypoint_number))
        gcs:send_text(0, string.format("loaded waypoint: %.0f", waypoint_number))
      end
    else
      gcs:send_text(0, 'unable to read waypoint, giving up')
    end
end


function mission_control() -- this is the loop which periodically runs
    -- MISSION_PAUSE_S pauses the mission_control() loop for the specified number of seconds
    -- This does not change the flight mode, neither disarms the vehicle.
    -- It does allow the user to change mode/disarm until it kicks back in.
    local should_pause = param:get('MISSION_PAUSE_S')
    param:set('MISSION_PAUSE_S', math.min(should_pause, 86400))-- do not allow it to go above 1 day
    if should_pause > 0 then
      param:set('MISSION_PAUSE_S', should_pause - 1) -- decrease 1 second at a time
      gcs:send_text(6,string.format("waiting for more %.0f seconds", should_pause))
      return update, 1000
    end

    -- If the switch connected to leak detector bus is on the off position, DISARM
    if not gpio:read(27) then
      if  arming:is_armed() then
        gcs:send_text(0, 'Disarming via onboard switch')
        arming:disarm()
      end
      return update, 1000
    end

    -- if the switch is in the on position, and we are not in the CRITICAL state, ARM
    if gpio:read(27) and not arming:is_armed() and not (state == STATE_CRITICAL)  then
      gcs:send_text(6,string.format("attempting to arm"))
      arming:arm()
      return update, 3000
    end

    -- If the vehicle is in guided mode, Resume the mission once waypoint is reached
    if (vehicle:get_mode() == MODE_GUIDED) then
      local curr_loc = ahrs:get_location()
      local target_loc = vehicle:get_target_location()
      if not target_loc then
        gcs:send_text(6,string.format("bad target_loc!!!"))
      end
      if curr_loc and target_loc then
        local dist_m = curr_loc:get_distance(target_loc)
        gcs:send_text(6,string.format("distance: %.0f ", dist_m))
        if (dist_m < 10.0) then
          reload_waypoint()
          vehicle:set_mode(MODE_AUTO)
        end
      end
    end

    -- ALWAYS go back to AUTO
    if (vehicle:get_mode() ~= MODE_AUTO) then
          gcs:send_text(6,string.format("setting to auto mode..."))
          vehicle:set_mode(MODE_AUTO)
          return update, 1000 -- reschedules the loop
    end

    -- Save mission state to persistence file
    local number = mission:get_current_nav_index()
    local mission_state = mission:state()

    if mission:get_current_nav_index() <= 1 then
       reload_waypoint()
    end
    if (mission:get_current_nav_index() ~= 0) then
        file = assert(io.open(persistence_file, "w"), 'Could open :' .. persistence_file)
        file:write(tostring(mission:get_current_nav_index()))
    end
    return update, 10000
  end

function lost_voltage()
    -- Check if we lost voltage/current measurements
    local voltage = battery:voltage(0)
    return voltage < 5.0 or voltage > 18.0
end

function is_upside_down()
    -- Check if the vehicle iss upside down
    local roll = math.deg(ahrs:get_roll())
    return math.abs(roll) > 90
end

function update_power_strategy()
    -- Changes strategy/state
    -- In priority order, high to low:
    --   - CRITICAL means something is wrong, disarm immediately
    --   - OVERRIDE means users is taking control, use the user setpoint for desired power consumption
    --   - LOST_VOLTAGE means our adc is bad, and we use a conservative power consumption target
    --   - CRUISING is the default state, and uses a linear power consumption target
    local voltage = battery:voltage(0)
    if voltage > BAT_CRITICAL then
        state = STATE_CRUISING
    elseif voltage < BAT_CRITICAL then
        state = STATE_CRITICAL
    end
    if lost_voltage() then
        state = STATE_LOST_VOLTAGE
    end
    if override:get() > 0 then
        state = STATE_OVERRIDE
    end
    if is_upside_down() then
        state = STATE_CRITICAL
    end
end

function update_critical()
    -- CRITICAL state function, just disarms.
    if arming:is_armed() then
        arming:disarm()
    end
end

function update_cruising()
    -- CRUISING state function, outputs linear to voltage - BAT_CRITICAL
    local voltage = battery:voltage(0)
    param:set('BAT_DES_POWER_W', math.max((voltage - BAT_CRITICAL)*kp:get(), 0))
end

function update_lost_voltage()
    -- LOST_VOLTAGE state function, outputs conservative power
    param:set('BAT_DES_POWER_W', POWER_SAFE)
end

function update_override()
    -- OVERRIDE state function, outputs the power set by the user
    param:set('BAT_DES_POWER_W', override:get())
    if override:get() <= 0 then
        state = STATE_CRUISING
    end
end

function update_standby()
    -- STANDBY state, runs before arming in CRUISING
    if arming:is_armed() then
        state = STATE_CRUISING
    end
end

function limit_thrust_to(percentage)
    -- LIMITS thrust using MOT_THR_MAX and SERVOx_MIN/MAX for yaw
    param:set('MOT_THR_MAX', percentage)
    -- ALSO limit SERVO1 and SERVO3 ranges to reduce power when yawing
    pwm_percent = math.max(percentage/100.0, 0.1)
    param:set('SERVO1_MAX',  1500 + pwm_percent * 400)
    param:set('SERVO1_MIN',  1500 - pwm_percent * 400)
    param:set('SERVO3_MAX',  1500 + pwm_percent * 400)
    param:set('SERVO3_MIN',  1500 - pwm_percent * 400)
end

function power_control()
    -- Uses a simple controller to reach a target power consumption on the whole system
    local watts = battery:current_amps(0) * battery:voltage(0)
    local deadzone = 3
    local current_throttle_limit =  param:get('MOT_THR_MAX')
    local desired_power = param:get('BAT_DES_POWER_W')
    local new_current_throttle_limit = current_throttle_limit
    if watts > desired_power + deadzone then
        new_current_throttle_limit = math.max(current_throttle_limit - 1, 0)
        limit_thrust_to(new_current_throttle_limit)
    end
    if watts < desired_power - deadzone then
        new_current_throttle_limit = math.min(current_throttle_limit + 1, max_thrust_percentage:get())
        limit_thrust_to(new_current_throttle_limit)
    end
    -- Always reset to 1 when disarmed
    if not arming:is_armed() then
        limit_thrust_to(1)
    end
    -- Always reset to 1 when in AUTO but mission is completed
    if vehicle:get_mode() == MODE_AUTO then
        if mission:state() == mission.MISSION_COMPLETE or mission:state() == mission.MISSION_STOPPED then
            limit_thrust_to(1)
        end
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
    if state ~= STATE_CRITICAL then
        mission_control()
    end
    return update, 1000 -- reschedules the loop
end

return update() -- run immediately before starting to reschedule
