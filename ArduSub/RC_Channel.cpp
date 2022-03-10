#include "Sub.h"

#include "RC_Channel.h"

// defining these two macros and including the RC_Channels_VarInfo
// header defines the parameter information common to all vehicle
// types
#define RC_CHANNELS_SUBCLASS RC_Channels_Sub
#define RC_CHANNEL_SUBCLASS RC_Channel_Sub

#include <RC_Channel/RC_Channels_VarInfo.h>

// note that this callback is not presently used on Plane:
int8_t RC_Channels_Sub::flight_mode_channel_number() const
{
    return 1; // sub does not have a flight mode channel
}

// do_aux_function - implement the function invoked by auxiliary switches
bool RC_Channel_Sub::do_aux_function(const aux_func_t ch_option, const AuxSwitchPos ch_flag)
{
    switch(ch_option) {
        case AUX_FUNC::ARMDISARM:
            RC_Channel::do_aux_function_armdisarm(ch_flag);
            break;

    default:
        return RC_Channel::do_aux_function(ch_option, ch_flag);
    }
    return true;
}