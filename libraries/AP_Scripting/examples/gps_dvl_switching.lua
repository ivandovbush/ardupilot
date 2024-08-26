-- This scripts handles switching between surface GPS and DVL.
-- It creates a parameter DVL_HDOP_THR, which is the maximum gps_speed_accuracy
-- we accept from the GPS to use it as a source.
-- As soon as the GPS HDOP is under a threshold for a sustained time, we switch to using its position
-- As the DVL is much noisier than the DVL, our positioning error is that of the GPS position
-- plus the eventual DVL drift
-- Tested on Sub 4.5-dev as of Dec 18 2023

local PARAM_TABLE_KEY = 90
local PARAM_TABLE_PREFIX = 'DVL_'

gcs:send_text(3, "Starting DVL/GPS integration")

-- bind a parameter to a variable
function bind_param(name)
   local p = Parameter()
   assert(p:init(name), string.format('could not find %s parameter', name))
   return p
end

-- add a parameter and bind it to a variable
function bind_add_param(name, idx, default_value)
   assert(param:add_param(PARAM_TABLE_KEY, idx, name, default_value), string.format('could not add param %s', name))
   return bind_param(PARAM_TABLE_PREFIX .. name)
end

assert(param:add_table(PARAM_TABLE_KEY, PARAM_TABLE_PREFIX, 32), 'could not add param table')

local hdop_threshold = bind_add_param('HDOP_THR', 1, 200)

-- Setup ekf sources
local ek3_src1_posxy = bind_param('EK3_SRC1_POSXY')
local ek3_src1_velxy = bind_param('EK3_SRC1_VELXY')
local ek3_src1_posz = bind_param('EK3_SRC1_POSZ')
local ek3_src1_velz = bind_param('EK3_SRC1_VELZ')

-- TODO: do not change this, just alert the user
ek3_src1_posxy:set(6)
ek3_src1_velxy:set(6)
ek3_src1_posz:set(1)
ek3_src1_velz:set(0)

local ek3_src2_posxy = bind_param('EK3_SRC2_POSXY')
local ek3_src2_velxy = bind_param('EK3_SRC2_VELXY')
local ek3_src2_posz = bind_param('EK3_SRC2_POSZ')
local ek3_src2_velz = bind_param('EK3_SRC2_VELZ')

-- TODO: do not change this, just alert the user
ek3_src2_posxy:set(3)
ek3_src2_velxy:set(6)
ek3_src2_posz:set(1)
ek3_src2_velz:set(0)

local options = bind_param('EK3_SRC_OPTIONS')
options:set(0)


local hdop_below_threshold_time = 0 -- Timer for tracking time below threshold

function update()
  local gps_hdop = gps:get_hdop(gps:primary_sensor())  -- Fetch HDOP value using get_hdop
  if gps_hdop == nil then
    gcs:send_named_float('GpsHDOP', 10000)
    hdop_below_threshold_time = 0  -- Reset timer if no HDOP value
    return update, 100
  else
    gcs:send_named_float('GpsHDOP', gps_hdop)
  end

  if gps_hdop < hdop_threshold:get() then
    -- Increment timer if HDOP is below threshold
    hdop_below_threshold_time = hdop_below_threshold_time + 100
  else
    -- Reset timer if HDOP goes above threshold
    hdop_below_threshold_time = 0
  end

  local wanted_source = 0
  if hdop_below_threshold_time >= 5000 then  -- Check if timer has reached 5 seconds
    wanted_source = 1
    if not ahrs:get_origin() and ahrs:get_location() then
        ahrs:set_origin(ahrs:get_location())
    end
  end

  if ahrs:get_posvelyaw_source_set() ~= wanted_source then
    gcs:send_text(3, string.format("Switching to source %d", wanted_source))
    ahrs:set_posvelyaw_source_set(wanted_source)
  end

  -- gcs:send_named_float('EkfSource', ahrs:get_posvelyaw_source_set())
  -- gcs:send_text(3, string.format("HDOP: %.2f, Time below threshold: %d, Source: %d", gps_hdop, hdop_below_threshold_time, ahrs:get_posvelyaw_source_set()))
  return update, 100
end

return update()
