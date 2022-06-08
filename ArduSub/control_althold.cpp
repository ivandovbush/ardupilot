#include "Sub.h"


/*
 * control_althold.pde - init and run calls for althold, flight mode
 */

// althold_init - initialise althold controller
bool Sub::althold_init()
{
    if(!control_check_barometer()) {
        return false;
    }

    // initialize vertical maximum speeds and acceleration
    // sets the maximum speed up and down returned by position controller
    pos_control.set_max_speed_accel_z(-get_pilot_speed_dn(), g.pilot_speed_up, g.pilot_accel_z);
    pos_control.set_correction_speed_accel_z(-get_pilot_speed_dn(), g.pilot_speed_up, g.pilot_accel_z);

    // initialise position and desired velocity
    pos_control.init_z_controller();

    last_pilot_heading = ahrs.yaw_sensor;

    return true;
}

// althold_run - runs the althold controller
// should be called at 100hz or more
void Sub::althold_run()
{
    // When unarmed, disable motors and stabilization
    if (!motors.armed()) {
        motors.set_desired_spool_state(AP_Motors::DesiredSpoolState::GROUND_IDLE);
        // Sub vehicles do not stabilize roll/pitch/yaw when not auto-armed (i.e. on the ground, pilot has never raised throttle)
        attitude_control.set_throttle_out(0.5,true,g.throttle_filt);
        attitude_control.relax_attitude_controllers();
        pos_control.relax_z_controller(motors.get_throttle_hover());
        last_roll = 0;
        last_pitch = 0;
        last_pilot_heading = ahrs.yaw_sensor;
        return;
    }


    handle_attitude();

    control_depth();

    motors.set_forward(channel_forward->norm_input());
    motors.set_lateral(channel_lateral->norm_input());
}

void Sub::control_depth() {
    float target_climb_rate_cm_s = get_pilot_desired_climb_rate(channel_throttle->get_control_in());
    target_climb_rate_cm_s = constrain_float(target_climb_rate_cm_s, -get_pilot_speed_dn(), g.pilot_speed_up);

    // desired_climb_rate returns 0 when within the deadzone.
    //we allow full control to the pilot, but as soon as there's no input, we handle being at surface/bottom
    if (fabsf(target_climb_rate_cm_s) < 0.05f)  {
        if (ap.at_surface) {
            pos_control.set_pos_target_z_cm(MIN(pos_control.get_pos_target_z_cm(), g.surface_depth - 5.0f)); // set target to 5 cm below surface level
        } else if (ap.at_bottom) {
            pos_control.set_pos_target_z_cm(MAX(inertial_nav.get_position_z_up_cm() + 10.0f, pos_control.get_pos_target_z_cm())); // set target to 10 cm above bottom
        }
    }

    pos_control.set_pos_target_z_from_climb_rate_cm(target_climb_rate_cm_s);
    pos_control.update_z_controller();

}
