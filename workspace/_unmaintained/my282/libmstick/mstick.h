#ifndef __mstick_h__
#define __mstick_h__

void Stick_init(void);
void Stick_quit(void);
void Stick_get(int* x, int* y); // -32768 thru 32767

#endif  // __mstick_h__
