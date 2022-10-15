#ifndef SIGLOCK_H
#define SIGLOCK_H

enum {
	SIGLOCK_INIT,
	SIGLOCK_LOCK,
	SIGLOCK_UNLOCK
};

void siglock(int act, ...);

#endif
