# Import mavutil
from pymavlink import mavutil
from pymavlink.quaternion import QuaternionBase
import math
import time

ALT_HOLD_MODE = 2

def is_armed():
    try:
        return bool(master.wait_heartbeat().base_mode & 0b10000000)
    except:
        return False

def mode_is(mode):
    try:
        return bool(master.wait_heartbeat().custom_mode == mode)
    except:
        return False

def set_target_depth(depth):
    msg = master.mav.set_position_target_global_int_send(
        0,     
        0, 0,   
        mavutil.mavlink.MAV_FRAME_GLOBAL_INT, # frame
        0b0000111111111000,
        0,0, depth,
        0 , 0 , 0 , # x , y , z velocity in m/ s ( not used )
        0 , 0 , 0 , # x , y , z acceleration ( not supported yet , ignored in GCS Mavlink )
        0 , 0 ) # yaw , yawrate ( not supported yet , ignored in GCS Mavlink )


# Create the connection
master = mavutil.mavlink_connection('udpin:0.0.0.0:14550')
# Wait a heartbeat before sending commands
print(master.wait_heartbeat())


while not is_armed():
    master.arducopter_arm()

while not mode_is(ALT_HOLD_MODE):
    master.set_mode('ALT_HOLD')

set_target_depth(-0.5)
print("depth target set")
time.sleep(20)

pitch = yaw = 0

for i in range(500): 
    print (i)
    msg = master.mav.set_attitude_target_send(
        0,     
        0, 0,   
        1<<6, # 
        QuaternionBase([math.radians(i*3), pitch, yaw]), # -> attitude quaternion (w, x, y, z | zero-rotation is 1, 0, 0, 0)
        0, #roll rate
        0, #pitch rate
        0, 0)    # yaw rate, thrust 
    # send command to vehicle
    time.sleep(0.5)

