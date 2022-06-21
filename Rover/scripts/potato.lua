-- This script is an example of saying hello.  A lot.
-- Pick a random number to send back

function update() -- this is the loop which periodically runs
    local amps = battery:current_amps(0)
    local target_amps = 3.0
    local deadzone = 0.5
    local current_throttle_limit =  param:get('SERVO1_MAX') - 1500
    local new_current_throttle_limit = current_throttle_limit
    local mode = vehicle:get_mode()
    if not (mode == 15) then    -- if guided mode
        return update, 1000
    end
    if amps > target_amps + deadzone then
        new_current_throttle_limit = math.max(current_throttle_limit * 0.9, 100)
        param:set('SERVO1_MAX', 1500 + new_current_throttle_limit)
        param:set('SERVO1_MIN', 1500 - new_current_throttle_limit)
        param:set('SERVO2_MAX', 1500 + new_current_throttle_limit)
        param:set('SERVO2_MIN', 1500 - new_current_throttle_limit)
    end
    if amps < target_amps - deadzone then
        new_current_throttle_limit = math.min(current_throttle_limit * 1.1, 500)
        param:set('SERVO1_MAX', 1500 + new_current_throttle_limit)
        param:set('SERVO1_MIN', 1500 - new_current_throttle_limit)
        param:set('SERVO2_MAX', 1500 + new_current_throttle_limit)
        param:set('SERVO2_MIN', 1500 - new_current_throttle_limit)
    end
    -- gcs:send_text(6,string.format("range: %.0fs ", new_current_throttle_limit))
    gcs:send_text(6,string.format("range: %.0f ", desired_power))

  return update, 1000 -- reschedules the loop
end

return update() -- run immediately before starting to reschedule
