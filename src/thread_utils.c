/*
 * GeeXboX Valhalla: tiny media scanner API.
 * Copyright (C) 2009 Mathieu Schroeter <mathieu.schroeter@gamesover.ch>
 *
 * This file is part of libvalhalla.
 *
 * libvalhalla is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * libvalhalla is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with libvalhalla; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */

#include <unistd.h>
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#undef WIN32_LEAN_AND_MEAN
#else
#include <sys/resource.h>
#include <sys/syscall.h>
#include <sys/types.h>
#endif /* !_WIN32 */

#include "thread_utils.h"


int
vh_setpriority (int prio)
{
#ifdef _WIN32
  HANDLE hThread;
  const int thread_prio[] = {
    THREAD_PRIORITY_TIME_CRITICAL,
    THREAD_PRIORITY_HIGHEST,
    THREAD_PRIORITY_ABOVE_NORMAL,
    THREAD_PRIORITY_NORMAL,
    THREAD_PRIORITY_BELOW_NORMAL,
    THREAD_PRIORITY_LOWEST,
    THREAD_PRIORITY_IDLE
  };

  /*
   * Input values must be 0 for NORMAL priority, 3 for IDLE and -3
   * for TIME_CRITICAL.
   */
  prio += 3;
  if (prio > 6)
    prio = 6; /* IDLE */
  else if (prio < 0)
    prio = 0; /* TIME_CRITICAL */

  hThread = GetCurrentThread ();
  SetThreadPriority (hThread, thread_prio[prio]);
  return (int) GetCurrentThreadId ();
#else
#ifdef __linux__
  /*
   * Linux creates the threads with the clone system call. The function
   * pthread_create() uses the flag CLONE_THREAD with clone(). All threads
   * are placed in a group with a shared PID, so-called TGID for thread
   * group. The function getpid() returns the TGID, and gettid() returns
   * the TID of the thread. But gettid() is a Linux specific system call.
   *  http://www.kernel.org/doc/man-pages/online/pages/man2/clone.2.html
   *  (DESCRIPTION -> CLONE_THREAD)
   */
  pid_t pid = syscall (SYS_gettid); /* gettid() is not available with glibc */
#else
  /*
   * A FreeBSD kernel has no clone and gettid system calls. The threads
   * are created by rfork() and every thread has its own PID which can be
   * retrieved with getpid().
   *  http://www.khmere.com/freebsd_book/html/ch03.html
   *  (ch. 3.4 and ch. 3.5)
   */
  pid_t pid = getpid ();
#endif /* !__linux__ */
  setpriority (PRIO_PROCESS, pid, prio);
  return (int) pid;
#endif /* !_WIN32 */
}
