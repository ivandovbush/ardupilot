#include "Sub.h"

// stabilize_init - initialise stabilize controller
bool Sub::stabilize_init()
{
    // set target altitude to zero for reporting
    pos_control.set_pos_target_z_cm(0);
    last_roll = 0;
    last_pitch = 0;
    last_pilot_heading = ahrs.yaw_sensor;
    return true;
}

void Sub::handle_attitude()
{
    uint32_t tnow = AP_HAL::millis();

    // initialize vertical speeds and acceleration
    pos_control.set_max_speed_accel_z(-get_pilot_speed_dn(), g.pilot_speed_up, g.pilot_accel_z);
    motors.set_desired_spool_state(AP_Motors::DesiredSpoolState::THROTTLE_UNLIMITED);

    // get pilot desired lean angles
    float target_roll, target_pitch, target_yaw;

    // Check if set_attitude_target_no_gps is valid
    if (tnow - sub.set_attitude_target_no_gps.last_message_ms < 5000) {
        Quaternion(
            set_attitude_target_no_gps.packet.q
        ).to_euler(
            target_roll,
            target_pitch,
            target_yaw
        );
        target_roll = 100 * degrees(target_roll);
        target_pitch = 100 * degrees(target_pitch);
        target_yaw = 100 * degrees(target_yaw);
        attitude_control.input_euler_angle_roll_pitch_yaw(target_roll, target_pitch, target_yaw, true);
    } else {
        // If we don't have a mavlink attitude target, we use the pilot's input instead
        get_pilot_desired_lean_angles(channel_roll->get_control_in(), channel_pitch->get_control_in(), target_roll, target_pitch, aparm.angle_max);
        target_yaw = get_pilot_desired_yaw_rate(channel_yaw->get_control_in());
        if (abs(target_roll) > 50 || abs(target_pitch) > 50 || abs(target_yaw) > 50) {
            last_roll = ahrs.roll_sensor;
            last_pitch = ahrs.pitch_sensor;
            last_pilot_heading = ahrs.yaw_sensor;
            last_input_ms = tnow;
            attitude_control.input_rate_bf_roll_pitch_yaw(target_roll, target_pitch, target_yaw);
        } else if (tnow < last_input_ms + 250) {
            // just brake for a few mooments so we don't bounce
            attitude_control.input_rate_bf_roll_pitch_yaw(0, 0, 0);
        } else {
            // Lock attitude
            attitude_control.input_euler_angle_roll_pitch_yaw(last_roll, last_pitch, last_pilot_heading, true);
        }
    }
}

// stabilize_run - runs the main stabilize controller
// should be called at 100hz or more
void Sub::stabilize_run()
{
    // if not armed set throttle to zero and exit immediately
    if (!motors.armed()) {
        motors.set_desired_spool_state(AP_Motors::DesiredSpoolState::GROUND_IDLE);
        attitude_control.set_throttle_out(0,true,g.throttle_filt);
        attitude_control.relax_attitude_controllers();
        last_pilot_heading = ahrs.yaw_sensor;
        last_roll = 0;
        last_pitch = 0;
        return;
    }

    handle_attitude();

    // output pilot's throttle
    attitude_control.set_throttle_out(channel_throttle->norm_input(), false, g.throttle_filt);

    //control_in is range -1000-1000
    //radio_in is raw pwm value
    motors.set_forward(channel_forward->norm_input());
    motors.set_lateral(channel_lateral->norm_input());
}
