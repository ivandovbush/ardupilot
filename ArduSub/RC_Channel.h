#pragma once

#include <RC_Channel/RC_Channel.h>
#include "mode.h"

class RC_Channel_Sub : public RC_Channel
{

public:

protected:

    void init_aux_function(const aux_func_t ch_option, const AuxSwitchPos ch_flag) override;
    bool do_aux_function(aux_func_t ch_option, AuxSwitchPos) override;

private:
    void do_aux_function_change_mode(const Mode::Number mode,
                                     const AuxSwitchPos ch_flag);
};

class RC_Channels_Sub : public RC_Channels
{
public:

    RC_Channel_Sub obj_channels[NUM_RC_CHANNELS];
    RC_Channel_Sub *channel(const uint8_t chan) override {
        if (chan >= NUM_RC_CHANNELS) {
            return nullptr;
        }
        return &obj_channels[chan];
    }

    // tell the gimbal code all is good with RC input:
    bool in_rc_failsafe() const override { return false; };

protected:

    // note that these callbacks are not presently used on Plane:
    int8_t flight_mode_channel_number() const override {return 0;};

};
