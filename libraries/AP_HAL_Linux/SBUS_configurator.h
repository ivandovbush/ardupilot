#pragma once

namespace SBUS {
    #include <termios.h>
    #include <asm/termbits.h>
    #include <stdio.h>
    #include <asm/ioctls.h>
    #include <asm-generic/ioctls.h>

    void configure_sbus(int fd, uint32_t baudrate) {
        struct termios2 tio {};

        if (ioctl(fd, TCGETS2, &tio) != 0) {
            return;
        }
        tio.c_iflag &= ~(IGNBRK | BRKINT | PARMRK | ISTRIP | INLCR
                        | IGNCR | ICRNL | IXON);
        tio.c_iflag |= (INPCK | IGNPAR);
        tio.c_oflag &= ~OPOST;
        tio.c_lflag &= ~(ECHO | ECHONL | ICANON | ISIG | IEXTEN);
        tio.c_cflag &= ~(CSIZE | CRTSCTS | PARODD | CBAUD);
        // use BOTHER to specify speed directly in c_[io]speed member
        tio.c_cflag |= (CS8 | CSTOPB | CLOCAL | PARENB | BOTHER | CREAD);
        tio.c_ispeed = baudrate;
        tio.c_ospeed = baudrate;
        if (ioctl(fd, TCSETS2, &tio) != 0) {
            return;
        }
    }
}