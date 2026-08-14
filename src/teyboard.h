#ifndef TEYBOARD_H
#define TEYBOARD_H

#define KEY_NONE  (-1)  /* no key pending */
#define KEY_UP    (-2)
#define KEY_DOWN  (-3)
#define KEY_LEFT  (-4)
#define KEY_RIGHT (-5)
#define KEY_HOME  (-6)
#define KEY_END   (-7)
#define KEY_DEL   (-8)
#define KEY_INS   (-9)

void teyboard_init(void);
int  teyboard_getchar(void); /* KEY_NONE if no key pending, else a key code */

#endif
