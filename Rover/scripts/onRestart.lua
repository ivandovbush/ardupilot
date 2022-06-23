  -- This script is an example of saying hello.  A lot.
  -- Pick a random number to send back

  MODE_MANUAL = 0
  MODE_HOLD = 4
  MODE_AUTO = 10
  MODE_GUIDED = 15
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
        gcs:send_text(6, 'unable to open file, giving up')
        return
      end
      io.input(file)
      local waypoint_number = io.read("*number")
      if waypoint_number then
        mission:set_current_cmd(waypoint_number)
      end
  end


  function update() -- this is the loop which periodically runs
    if not gpio:read(27) then
      if  arming:is_armed() then
        arming:disarm()
      end
      return update, 1000
    end
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
    if (vehicle:get_mode() == MODE_MANUAL or vehicle:get_mode() == MODE_HOLD) then
          gcs:send_text(6,string.format("setting to auto mode..."))
          vehicle:set_mode(MODE_AUTO)
          return update, 1000 -- reschedules the loop
      end
      if not arming:is_armed() then
          arming:arm()
          return update, 1000
      end
      local number = mission:get_current_nav_index()
      local mission_state = mission:state()

      if  mission:get_current_nav_index() == 0 then
          reload_waypoint()
      end

      if (mission:get_current_nav_index() ~= 0) then
          file = assert(io.open(persistence_file, "w"), 'Could open :' .. persistence_file)
          file:write(tostring(mission:get_current_nav_index()))
      end

      return update, 10000

  end

  return update()