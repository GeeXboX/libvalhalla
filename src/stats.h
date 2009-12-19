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

#ifndef VALHALLA_STATS_H
#define VALHALLA_STATS_H

typedef struct vh_stats_s vh_stats_t;
typedef struct vh_stats_tmr_s vh_stats_tmr_t;
typedef struct vh_stats_cnt_s vh_stats_cnt_t;


vh_stats_t *vh_stats_new (void);
void vh_stats_free (vh_stats_t *stats);
void vh_stats_grp_add (vh_stats_t *stats, const char *grp,
                       void (*dump) (vh_stats_t *stats, void *data),
                       void *data);
vh_stats_tmr_t *vh_stats_grp_timer_add (vh_stats_t *stats, const char *grp,
                                        const char *tmr, const char *sub);
vh_stats_cnt_t *vh_stats_grp_counter_add (vh_stats_t *stats, const char *grp,
                                          const char *cnt, const char *sub);

vh_stats_tmr_t *vh_stats_timer_get (vh_stats_t *stats, const char *grp,
                                    const char *tmr, const char *sub);
vh_stats_cnt_t *vh_stats_counter_get (vh_stats_t *stats, const char *grp,
                                      const char *cnt, const char *sub);
unsigned long int vh_stats_timer_read (vh_stats_tmr_t *timer);
unsigned long int vh_stats_counter_read (vh_stats_cnt_t *counter);
void vh_stats_timer (vh_stats_tmr_t *timer, int start);
void vh_stats_counter (vh_stats_cnt_t *counter, unsigned long int val);

void vh_stats_dump (vh_stats_t *stats, const char *grp);
void vh_stats_debug_dump (vh_stats_t *stats);

#ifdef VALHALLA_H
const char *vh_stats_group_next (vh_stats_t *stats, const char *id);
unsigned long vh_stats_read_next (vh_stats_t *stats, const char *id,
                                  valhalla_stats_type_t type,
                                  const char **item);
#endif /* VALHALLA_H */

#define VH_STATS_TIMER_START(s)    vh_stats_timer (s, 1)
#define VH_STATS_TIMER_STOP(s)     vh_stats_timer (s, 0)
#define VH_STATS_COUNTER_INC(s)    vh_stats_counter (s, 1)
#define VH_STATS_COUNTER_ACC(s, v) vh_stats_counter (s, v)

#endif /* VALHALLA_STATS_H */
