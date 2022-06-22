-- This script is an example of saying hello.  A lot.
-- Pick a random number to send back

function update() -- this is the loop which periodically runs
    local watts = battery:current_amps(0) * battery:voltage(0)
    local deadzone = 0.1
    local current_throttle_limit =  param:get('MOT_THR_MAX')
    local desired_power =  param:get('BAT_DES_POWER_W')
    local new_current_throttle_limit = current_throttle_limit
    local mode = vehicle:get_mode()
    if not (mode == 15 or mode == 10 ) then    -- if guided mode
        return update, 1000
    end
    if watts > desired_power + deadzone then
        new_current_throttle_limit = math.max(current_throttle_limit - 1, 30)
        param:set('MOT_THR_MAX', new_current_throttle_limit)
    end
    if watts < desired_power - deadzone then
        new_current_throttle_limit = math.min(current_throttle_limit + 1, 100)
        param:set('MOT_THR_MAX', new_current_throttle_limit)

    end
    -- gcs:send_text(6,string.format("range: %.0fs ", new_current_throttle_limit))
    gcs:send_text(6,string.format("target: %.0f ", desired_power))
    gcs:send_text(6,string.format("max: %.0f ", new_current_throttle_limit))

  return update, 1000 -- reschedules the loop
end

return update() -- run immediately before starting to reschedule
