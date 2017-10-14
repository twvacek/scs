/*
 * Interface for SCS signal handling.
 */

#ifndef CTRLC_H_GUARD
#define CTRLC_H_GUARD

#if CTRLC > 0

#if defined MATLAB_MEX_FILE

/* No header file available here; define the prototypes ourselves */
extern int ut_is_interrupt_pending();
extern int ut_set_interrupt_enabled(int);

#elif(defined _WIN32 || defined _WIN64 || defined _WINDLL)

/* Use Windows set_console_ctrl_handler for signal handling */
#include <windows.h>

#else

/* Use POSIX clocl_gettime() for timing on non-Windows machines */
#include <signal.h>

#endif

/* METHODS are the same for both */
void start_interrupt_listener(void);
void end_interrupt_listener(void);
int is_interrupted(void);

#else /* CTRLC = 0 */

/* No signal handling. */
#define start_interrupt_listener()
#define end_interrupt_listener()
#define is_interrupted() 0

#endif /* END IF CTRLC > 0 */

#endif /* END IFDEF __TIMER_H__ */
