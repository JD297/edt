#ifndef JD297_EDT_GETCH_H
#define JD297_EDT_GETCH_H

#define KEY_CTRL(x) ((x) & 0x1f)


#define INT_KEY_IC 50
#define INT_KEY_DC 51

#define INT_KEY_PPAGE 53
#define INT_KEY_NPAGE 54
#define INT_KEY_UP 65
#define INT_KEY_DOWN 66
#define INT_KEY_RIGHT 67
#define INT_KEY_LEFT 68
#define INT_KEY_END 70
#define INT_KEY_HOME 72
#define INT_KEY_BACKSPACE 127

#define KEY_ESC 27
#define KEY_UP 0403
#define KEY_DOWN 0402
#define KEY_LEFT 0404
#define KEY_RIGHT 0405
#define KEY_HOME 0406
#define KEY_BACKSPACE 0407
#define KEY_F(n) (KEY_F0+(n))
#define KEY_DC 0512
#define KEY_IC 0513
#define KEY_NPAGE 0522
#define KEY_PPAGE 0523
#define KEY_PRINT 0532
#define KEY_END 0550
#define KEY_RESIZE 0632

#define GETCH_BUFFER_SIZE 8

/*
 *	RETURN VALUE
 *		On success return the current key code.
 *
 *		On error, -1 is returned, and errno is set to indicate the error.
 *	ERRORS SEE:
 *		fcntl(2)
 *		read(2)
 */
int getch();

#endif
