#include "Sub.h"
#include "stdio.h"

/*
 * control_motordetect.cpp - init and run calls for motordetect flightmode;
 *
 *  For each motor:
 *      wait until vehicle is stopped for > 500ms
 *      apply throttle up for 500ms
 *      If results are good:
 *          save direction and try the next motor.
 *      else
 *          wait until vehicle is stopped for > 500ms
 *          apply throttle down for 500ms
 *          If results are good
 *              save direction and try the next motor.
 *          If results are bad
 *              Abort!
 */

// controller states
enum md_state {
    MOTORDETECT_STOPPED,
    MOTORDETECT_SETTLING,
    MOTORDETECT_THRUSTING,
    MOTORDETECT_DETECTING,
    MOTORDETECT_DONE
};

enum direction {
    UP = 1,
    DOWN = -1
};

static uint32_t motordetect_state_time; // current time at a state
static uint8_t state = 0;
static uint8_t motordetect_current_motor = 0;
static int16_t current_direction = UP;


// althold_init - initialise althold controller
bool Sub::motordetect_init()
{
    state = MOTORDETECT_STOPPED;
    return true;
}

// althold_run - runs the althold controller
// should be called at 100hz or more
void Sub::motordetect_run()
{
    // if not armed set throttle to zero and exit immediately
    if (!motors.armed()) {
        motors.set_desired_spool_state(AP_Motors::DesiredSpoolState::GROUND_IDLE);
        for(uint8_t i = 0; i<AP_MOTORS_MAX_NUM_MOTORS; i++ ) {
            if (motors.motor_is_enabled(i)) {
                motors.output_test_num( i, 1500);
            }
        }
        state = MOTORDETECT_DONE;
        return;
    }

    switch(state) {
        // Motor detection is not running, set it up to start.
        case MOTORDETECT_STOPPED:
            current_direction = UP;
            motordetect_current_motor = 0;
            motordetect_state_time = AP_HAL::millis();
            state = MOTORDETECT_SETTLING;
            break;

        // Wait until sub stays for 500ms not spinning and leveled.
        case MOTORDETECT_SETTLING:
            for(uint8_t i = 0; i<AP_MOTORS_MAX_NUM_MOTORS; i++ ) {
                if (motors.motor_is_enabled(i)) {
                    !motors.output_test_num(i, 1500);
                }
            }
            if   (abs(ahrs.roll_sensor) > 1000 
               || abs(ahrs.roll_sensor) > 1000
               || (ahrs.get_gyro()*ahrs.get_gyro()) > 0.01) {
                   motordetect_state_time = AP_HAL::millis();
            }

            if (AP_HAL::millis() > (motordetect_state_time + 500)) {
                state = MOTORDETECT_THRUSTING;
                motordetect_state_time = AP_HAL::millis();
            }

            break;
            
        // Thrusts motor for 500ms
        case MOTORDETECT_THRUSTING:
            if (AP_HAL::millis() < (motordetect_state_time + 500) ) {
                if(!motors.output_test_num(motordetect_current_motor, 1500+300*current_direction)) {
                    state = MOTORDETECT_DONE;
                };

            } else {
                state = MOTORDETECT_DETECTING;
            }
            break;

        // Checks the result of thrusting the motor.
        // Starts again at the other direction if unable to get a good reading.
        // Fails if it is the second reading and it is still not good.
        // Set things up to test the next motor if the reading is good.
        case MOTORDETECT_DETECTING:
        {
            Vector3f gyro = ahrs.get_gyro();
            bool roll_up = gyro.x > 0.4;
            bool roll_down = gyro.x < -0.4;
            int roll = (int(roll_up) - int(roll_down))*current_direction;
            
            bool pitch_up = gyro.y > 0.4;
            bool pitch_down = gyro.y < -0.4;
            int pitch = (int(pitch_up) - int(pitch_down))*current_direction;
            
            bool yaw_up = gyro.z > 0.5;
            bool yaw_down = gyro.z < -0.5;
            int yaw = (+int(yaw_up) - int(yaw_down))*current_direction;

            Vector3f directions(roll, pitch, yaw);
            // Good read, not inverted
            if(directions == motors.get_motor_angular_factors(motordetect_current_motor)) {
                gcs().send_text(MAV_SEVERITY_INFO, "Thruster %d is ok!", motordetect_current_motor+1);
            } 
            // Good read, inverted
            else if (-directions == motors.get_motor_angular_factors(motordetect_current_motor)) {
                gcs().send_text(MAV_SEVERITY_INFO, "Thruster %d is reversed! Saving it!", motordetect_current_motor+1);
                motors.set_reversed(motordetect_current_motor, true);
            } 
            // Bad read!
            else {
                gcs().send_text(MAV_SEVERITY_INFO, "Bad thrust read, trying to push the other way...\n");
                // I we got here, we couldn't identify anything that made sense.
                // Let's try pushing the thruster the other way, maybe we are in too shallow waters
                if(current_direction == DOWN) {
                    // The reading for the second direction was also bad, we failed.
                    gcs().send_text(MAV_SEVERITY_CRITICAL, "Detection failed!");
                    state = MOTORDETECT_DONE;
                    break;
                }
                current_direction = DOWN;
                state = MOTORDETECT_SETTLING;

                break;
            }
            // If we got here, we have a decent motor reading, hooray!!
            state = MOTORDETECT_SETTLING;
            // Test the next motor, if it exists
            motordetect_current_motor++;
            current_direction = UP;
            if (!motors.motor_is_enabled(motordetect_current_motor)) {
                state = MOTORDETECT_DONE;
            }
            break;
        }
        case MOTORDETECT_DONE:
            control_mode = prev_control_mode;
            arming.disarm();
        break;

    }
}
