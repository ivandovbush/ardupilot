#include "SIM_config.h"

#if AP_SIM_GPS_MAV_ENABLED

#include "SIM_GPS_MAV.h"

#include <SITL/SITL.h>
#include <AP_Common/NMEA.h>
#include <AP_HAL/AP_HAL.h>

#include <time.h>
#include <sys/time.h>

extern const AP_HAL::HAL& hal;

using namespace SITL;

/*
  send a new GPS NMEA packet
 */
void GPS_MAV::publish(const GPS_Data *d)
{
     mavlink_msg_gps_input_send(
        chan,
        AP_HAL::millis(),
        GPS_INPUT_IGNORE_FLAG_VEL_HORIZ | GPS_INPUT_IGNORE_FLAG_VEL_VERT,
        gps_time().ms,
        gps_time().week,
        5, // fix type
        d->latitude * 1.0e7,
        d->longitude * 1.0e7,
        d->altitude,
        __UINT16_MAX__,
        __UINT16_MAX__,
        0, // vn
        0, // ve
        0, // vd
        0, // speed_accuracy
        0, // horiz_accuracy
        0, // vert_accuracy
        16, // sattelites visible
        d->yaw_deg*100) // yaw
        ;

    if (_sitl->gps_hdg_enabled[instance] == SITL::SIM::GPS_HEADING_HDT) {
        nmea_printf("$GPHDT,%.2f,T", d->yaw_deg);
    }
    else if (_sitl->gps_hdg_enabled[instance] == SITL::SIM::GPS_HEADING_THS) {
        nmea_printf("$GPTHS,%.2f,%c,T", d->yaw_deg, d->have_lock ? 'A' : 'V');
    } else if (_sitl->gps_hdg_enabled[instance] == SITL::SIM::GPS_HEADING_KSXT) {
        // Unicore support
        // $KSXT,20211016083433.00,116.31296102,39.95817066,49.4911,223.57,-11.32,330.19,0.024,,1,3,28,27,,,,-0.012,0.021,0.020,,*2D
        nmea_printf("$KSXT,%04u%02u%02u%02u%02u%02u.%02u,%.8f,%.8f,%.4f,%.2f,%.2f,%.2f,%.2f,%.3f,%u,%u,%u,%u,,,,%.3f,%.3f,%.3f,,",
                    tm->tm_year+1900, tm->tm_mon+1, tm->tm_mday, tm->tm_hour, tm->tm_min, tm->tm_sec, unsigned(tv.tv_usec*1.e-4),
                    d->longitude, d->latitude,
                    d->altitude,
                    wrap_360(d->yaw_deg),
                    d->pitch_deg,
                    ground_track_deg,
                    speed_mps,
                    d->roll_deg,
                    d->have_lock?1:0, // 2=rtkfloat 3=rtkfixed,
                    3, // fixed rtk yaw solution,
                    d->have_lock?d->num_sats:3,
                    d->have_lock?d->num_sats:3,
                    d->speedE * 3.6,
                    d->speedN * 3.6,
                    -d->speedD * 3.6);
    }
}

#endif  // AP_SIM_GPS_MAV_ENABLED
