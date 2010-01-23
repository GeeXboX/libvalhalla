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


inline void
vh_setpriority (int prio)
{
  /*
   * According to POSIX, getpid() returns the PID of the main process, then all
   * threads return the same PID if the scope is PTHREAD_SCOPE_SYSTEM. It is
   * the reason why gettid() is necessary.
   * With BSD, gettid() is not a valid system call. The threads are created with
   * PTHREAD_SCOPE_PROCESS, then all threads have an unique PID which can be
   * got with getpid().
   *
   * NOTE: the scope PTHREAD_SCOPE_PROCESS is not supported by the Linux kernel.
   */
#ifdef __linux__
  pid_t pid = syscall (SYS_gettid); /* gettid() is not available with glibc */
#else
  pid_t pid = getpid ();
#endif
  setpriority (PRIO_PROCESS, pid, prio);
}
