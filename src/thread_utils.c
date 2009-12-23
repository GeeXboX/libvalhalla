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
#include <sys/resource.h>
#include <sys/syscall.h>
#include <sys/types.h>

#include "thread_utils.h"


void
vh_setpriority (int prio)
{
  /*
   * Linux creates the threads with the clone system call. The function
   * pthread_create() uses the flag CLONE_THREAD with clone(). All threads
   * are placed in a group with a shared PID, so-called TGID for thread
   * group. The function getpid() returns the TGID, and gettid() returns
   * the TID of the thread. But gettid() is a Linux specific system call.
   *  http://www.kernel.org/doc/man-pages/online/pages/man2/clone.2.html
   *  (DESCRIPTION -> CLONE_THREAD)
   *
   * A FreeBSD kernel has no clone and gettid system calls. The threads
   * are created by rfork() and every thread has its own PID which can be
   * retrieved with getpid().
   *  http://www.khmere.com/freebsd_book/html/ch03.html
   *  (ch. 3.4 and ch. 3.5)
   */
#ifdef __linux__
  pid_t pid = syscall (SYS_gettid); /* gettid() is not available with glibc */
#else
  pid_t pid = getpid ();
#endif /* __linux__ */
  setpriority (PRIO_PROCESS, pid, prio);
}
