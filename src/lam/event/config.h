/* config.h.  Generated automatically by configure.  */
/* config.h.in.  Generated automatically from configure.in by autoheader.  */
/* Define if kqueue works correctly with pipes */
/* #undef HAVE_WORKING_KQUEUE */

/* Define to `unsigned long long' if <sys/types.h> doesn't define.  */
/* #undef u_int64_t */

/* Define to `unsigned int' if <sys/types.h> doesn't define.  */
/* #undef u_int32_t */

/* Define to `unsigned short' if <sys/types.h> doesn't define.  */
/* #undef u_int16_t */

/* Define to `unsigned char' if <sys/types.h> doesn't define.  */
/* #undef u_int8_t */

/* Define if timeradd is defined in <sys/time.h> */
#define HAVE_TIMERADD 1
#ifndef HAVE_TIMERADD
/* #undef timerclear */
/* #undef timerisset */
/* #undef timercmp */
#define	timerclear(tvp)		(tvp)->tv_sec = (tvp)->tv_usec = 0
#define	timerisset(tvp)		((tvp)->tv_sec || (tvp)->tv_usec)
#define	timercmp(tvp, uvp, cmp)						\
	(((tvp)->tv_sec == (uvp)->tv_sec) ?				\
	    ((tvp)->tv_usec cmp (uvp)->tv_usec) :			\
	    ((tvp)->tv_sec cmp (uvp)->tv_sec))
#define timeradd(tvp, uvp, vvp)						\
	do {								\
		(vvp)->tv_sec = (tvp)->tv_sec + (uvp)->tv_sec;		\
		(vvp)->tv_usec = (tvp)->tv_usec + (uvp)->tv_usec;       \
		if ((vvp)->tv_usec >= 1000000) {			\
			(vvp)->tv_sec++;				\
			(vvp)->tv_usec -= 1000000;			\
		}							\
	} while (0)
#define	timersub(tvp, uvp, vvp)						\
	do {								\
		(vvp)->tv_sec = (tvp)->tv_sec - (uvp)->tv_sec;		\
		(vvp)->tv_usec = (tvp)->tv_usec - (uvp)->tv_usec;	\
		if ((vvp)->tv_usec < 0) {				\
			(vvp)->tv_sec--;				\
			(vvp)->tv_usec += 1000000;			\
		}							\
	} while (0)
#endif /* !HAVE_TIMERADD */

/* Define if TAILQ_FOREACH is defined in <sys/queue.h> */
/* #undef HAVE_TAILQFOREACH */
#ifndef HAVE_TAILQFOREACH
#define	TAILQ_FIRST(head)		((head)->tqh_first)
#define	TAILQ_END(head)			NULL
#define	TAILQ_NEXT(elm, field)		((elm)->field.tqe_next)
#define TAILQ_FOREACH(var, head, field)					\
	for((var) = TAILQ_FIRST(head);					\
	    (var) != TAILQ_END(head);					\
	    (var) = TAILQ_NEXT(var, field))
#define	TAILQ_INSERT_BEFORE(listelm, elm, field) do {			\
	(elm)->field.tqe_prev = (listelm)->field.tqe_prev;		\
	(elm)->field.tqe_next = (listelm);				\
	*(listelm)->field.tqe_prev = (elm);				\
	(listelm)->field.tqe_prev = &(elm)->field.tqe_next;		\
} while (0)
#endif /* TAILQ_FOREACH */

/* Define if your system supports the epoll system calls */
/* #undef HAVE_EPOLL */

/* Define if you have the `epoll_ctl' function. */
/* #undef HAVE_EPOLL_CTL */

/* Define if you have the `err' function. */
#define HAVE_ERR 1

/* Define if you have the `gettimeofday' function. */
#define HAVE_GETTIMEOFDAY 1

/* Define if you have the <inttypes.h> header file. */
#define HAVE_INTTYPES_H 1

/* Define if you have the `kqueue' function. */
/* #undef HAVE_KQUEUE */

/* Define if you have the `socket' library (-lsocket). */
/* #undef HAVE_LIBSOCKET */

/* Define if you have the <memory.h> header file. */
#define HAVE_MEMORY_H 1

/* Define if you have the `poll' function. */
#define HAVE_POLL 1

/* Define if you have the <poll.h> header file. */
#define HAVE_POLL_H 1

/* Define if your system supports POSIX realtime signals */
/* #undef HAVE_RTSIG */

/* Define if you have the `select' function. */
#define HAVE_SELECT 1

/* Define if you have the <signal.h> header file. */
#define HAVE_SIGNAL_H 1

/* Define if you have the `sigtimedwait' function. */
/* #undef HAVE_SIGTIMEDWAIT */

/* Define if you have the <stdint.h> header file. */
#define HAVE_STDINT_H 1

/* Define if you have the <stdlib.h> header file. */
#define HAVE_STDLIB_H 1

/* Define if you have the <strings.h> header file. */
#define HAVE_STRINGS_H 1

/* Define if you have the <string.h> header file. */
#define HAVE_STRING_H 1

/* Define if you have the <sys/epoll.h> header file. */
#define HAVE_SYS_EPOLL_H 1

/* Define if you have the <sys/event.h> header file. */
/* #undef HAVE_SYS_EVENT_H */

/* Define if you have the <sys/queue.h> header file. */
#define HAVE_SYS_QUEUE_H 1

/* Define if you have the <sys/stat.h> header file. */
#define HAVE_SYS_STAT_H 1

/* Define if you have the <sys/time.h> header file. */
#define HAVE_SYS_TIME_H 1

/* Define if you have the <sys/types.h> header file. */
#define HAVE_SYS_TYPES_H 1

/* Define if TAILQ_FOREACH is defined in <sys/queue.h> */
/* #undef HAVE_TAILQFOREACH */

/* Define if timeradd is defined in <sys/time.h> */
#define HAVE_TIMERADD 1

/* Define if you have the <unistd.h> header file. */
#define HAVE_UNISTD_H 1

/* Define if kqueue works correctly with pipes */
/* #undef HAVE_WORKING_KQUEUE */

/* Define if realtime signals work on pipes */
/* #undef HAVE_WORKING_RTSIG */

/* Name of package */
#define PACKAGE "libevent"

/* Define if you have the ANSI C header files. */
#define STDC_HEADERS 1

/* Define if you can safely include both <sys/time.h> and <time.h>. */
#define TIME_WITH_SYS_TIME 1

/* Version number of package */
#define VERSION "0.7c"

/* Define to `int' if <sys/types.h> does not define. */
/* #undef pid_t */

/* Define to `unsigned' if <sys/types.h> does not define. */
/* #undef size_t */

/* Define to unsigned int if you dont have it */
/* #undef socklen_t */

/* Define to `unsigned short' if <sys/types.h> does not define. */
/* #undef u_int16_t */

/* Define to `unsigned int' if <sys/types.h> does not define. */
/* #undef u_int32_t */

/* Define to `unsigned long long' if <sys/types.h> does not define. */
/* #undef u_int64_t */

/* Define to `unsigned char' if <sys/types.h> does not define. */
/* #undef u_int8_t */
