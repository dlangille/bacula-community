/*
   Bacula(R) - The Network Backup Solution

   Copyright (C) 2000-2022 Kern Sibbald

   The original author of Bacula is Kern Sibbald, with contributions
   from many others, a complete list can be found in the file AUTHORS.

   You may use this file and others of this release according to the
   license defined in the LICENSE file, which includes the Affero General
   Public License, v3.0 ("AGPLv3") and some additional permissions and
   terms pursuant to its AGPLv3 Section 7.

   This notice must be preserved when any source code is
   conveyed and/or propagated.

   Bacula(R) is a registered trademark of Kern Sibbald.
*/
/*
 *
 * Kern Sibbald, July MMIV
 *
 */

#ifndef BERRNO_H
#define BERRNO_H

/*
 * Extra bits set to interpret errno value differently from errno
 */
#ifdef HAVE_WIN32
#define b_errno_win32  (1<<29)        /* user reserved bit */
#define b_errno_WSA    (1<<26)
#else
#define b_errno_win32  0              /* On Unix/Linix system */
#endif
#define b_errno_exit   (1<<28)        /* child exited, exit code returned */
#define b_errno_signal (1<<27)        /* child died, signal code returned */
#define b_bacula_errno (1<<26)        /* internal Bacula error
                                         (e.g. script to be run is out of allowed paths) */


/* Set 'bacula error' bit flag for each Bacula-specific error */
static const int berr_not_allowed_path = 1 | b_bacula_errno;
static const int berr_not_allowed_char = 2 | b_bacula_errno;

/* Helper struct to map error code to message */
struct berror_msg {
   const int berrno;
   const char *msg;
};

static struct berror_msg berror_msgs_map[] = {
   { berr_not_allowed_path,       "Not in allowed paths" },
   { berr_not_allowed_char,       "Contains not allowed character" },
};

const int berror_msgs_map_size = sizeof(berror_msgs_map) / sizeof(berror_msg);

/*
 * A more generalized way of handling errno that works with Unix, Windows,
 *  and with Bacula bpipes.
 *
 * It works by picking up errno and creating a memory pool buffer
 *  for editing the message. strerror() does the actual editing, and
 *  it is thread safe.
 *
 * If bit 29 in m_berrno is set then it is a Win32 error, and we
 *  must do a GetLastError() to get the error code for formatting.
 * If bit 29 in m_berrno is not set, then it is a Unix errno.
 *
 */
class berrno : public SMARTALLOC {
   POOLMEM *m_buf;
   int m_berrno;
   void format_win32_message();
public:
   berrno(int pool=PM_EMSG);
   ~berrno();
   const char *bstrerror();
   const char *bstrerror(int errnum);
   void set_errno(int errnum);
   int code() { return m_berrno & ~(b_errno_exit|b_errno_signal); }
   int code(int stat) { return stat & ~(b_errno_exit|b_errno_signal); }
   const char *get_berr_msg();
};

/* Constructor */
inline berrno::berrno(int pool)
{
   m_berrno = errno;
   m_buf = get_pool_memory(pool);
   *m_buf = 0;
   errno = m_berrno;
}

inline berrno::~berrno()
{
   free_pool_memory(m_buf);
}

inline const char *berrno::bstrerror(int errnum)
{
   m_berrno = errnum;
   return berrno::bstrerror();
}


inline void berrno::set_errno(int errnum)
{
   m_berrno = errnum;
}

#endif /* BERRNO_H */
