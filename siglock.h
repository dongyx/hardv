#ifndef SIGLOCK_H
#define SIGLOCK_H

enum {
	SIGLOCK_INIT,
	SIGLOCK_LOCK,
	SIGLOCK_UNLOCK
};

int siglock(int act, ...);

#endif
