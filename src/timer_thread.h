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

#ifndef TIMER_THREAD_H
#define TIMER_THREAD_H

#include <inttypes.h>

typedef struct timer_thread_s timer_thread_t;

void vh_timer_thread_sleep (timer_thread_t *timer, uint16_t timeout);
void vh_timer_thread_stop (timer_thread_t *timer);
void vh_timer_thread_start (timer_thread_t *timer);
void vh_timer_thread_delete (timer_thread_t *timer);
timer_thread_t *vh_timer_thread_create (void);

#endif /* TIMER_THREAD_H */
