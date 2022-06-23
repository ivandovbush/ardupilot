-- This script is an example of saying hello.  A lot.
-- Pick a random number to send back

local MODE_MANUAL = 0
local MODE_AUTO = 10
local MODE_GUIDED = 15

function bind_param(name)
   local p = Parameter()
   assert(p:init(name), string.format('could not find %s parameter', name))
   return p
end

RCMAP_THROTTLE  = bind_param("RCMAP_THROTTLE")

function update() -- this is the loop which periodically runs
    if not arming:is_armed() then
        --     arming:arm()
        param:set('MOT_THR_MAX', 5)
        return update, 1000
    end
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
    -- gcs:send_text(6,string.format("range: %.0fs ", new_current_throttle_limit))
    -- gcs:send_text(6,string.format("target: %.0f ", desired_power))
    -- gcs:send_text(6,string.format("max: %.0f ", new_current_throttle_limit))
      local tchan = rc:get_channel(RCMAP_THROTTLE:get())
      local tval = (tchan:norm_input_ignore_trim()+1.0)*0.5
    if tval > 0.47 and tval < 0.53 then
        param:set('MOT_THR_MAX', 5)
    end
    gcs:send_text(6,string.format("rc: %.2f ", tval))

  return update, 1000 -- reschedules the loop
end

return update() -- run immediately before starting to reschedule
