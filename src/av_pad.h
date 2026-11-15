/* Aftershock: a gamepad from Linux's /dev/input (src/av_pad.c). */
#ifndef AV_PAD_H
#define AV_PAD_H

/* Finds and opens the first gamepad; 0 on success, -1 when there is none. */
int av_pad_open(void);
const char *av_pad_name(void);
/* Each frame, on the engine thread. */
void av_pad_poll(int focused);
void av_pad_close(void);

#endif
