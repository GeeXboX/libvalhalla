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

#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "valhalla.h"
#include "valhalla_internals.h"
#include "logs.h"
#include "utils.h"
#include "osdep.h"
#include "stats.h"

struct vh_stats_tmr_s {
  struct vh_stats_tmr_s *next;
  char *id;
  pthread_mutex_t mutex; /* for .time */
  struct timespec ts;
  struct timespec time;
};

struct vh_stats_cnt_s {
  struct vh_stats_cnt_s *next;
  char *id;
  pthread_mutex_t mutex; /* for .count */
  uint64_t count;
};

typedef struct vh_stats_grp_s {
  struct vh_stats_grp_s *next;
  char *id;
  vh_stats_tmr_t *timers;
  vh_stats_cnt_t *counters;
  void (*dump) (vh_stats_t *stats, void *data);
  void *data;
} vh_stats_grp_t;

struct vh_stats_s {
  vh_stats_grp_t *groups;
};


#define ITEM_SEARCH(it, base, _id)                              \
  for (it = base; it; it = (it)->next)                          \
    if (!strcmp ((it)->id, _id))                                \
      break;

#define ITEM_MALLOC(it, base, type)                             \
  for (it = base; it && it->next; it = it->next)                \
    ;                                                           \
  if (it)                                                       \
  {                                                             \
    (it)->next = calloc (1, sizeof (type));                     \
    it = (it)->next;                                            \
  }                                                             \
  else                                                          \
  {                                                             \
    base = calloc (1, sizeof (type));                           \
    it = base;                                                  \
  }

vh_stats_tmr_t *
vh_stats_timer_get (vh_stats_t *stats,
                    const char *grp, const char *tmr, const char *sub)
{
  char id[64];
  vh_stats_grp_t *it;
  vh_stats_tmr_t *it2;

  if (!stats || !grp || !tmr)
    return NULL;

  ITEM_SEARCH (it, stats->groups, grp)
  if (!it)
    return NULL;

  snprintf (id, sizeof (id), "%s%c%s", tmr, sub ? ':' : '\0', sub ? sub : "");
  ITEM_SEARCH (it2, it->timers, id)
  return it2;
}

vh_stats_cnt_t *
vh_stats_counter_get (vh_stats_t *stats,
                      const char *grp, const char *cnt, const char *sub)
{
  char id[64];
  vh_stats_grp_t *it;
  vh_stats_cnt_t *it2;

  if (!stats || !grp || !cnt)
    return NULL;

  ITEM_SEARCH (it, stats->groups, grp)
  if (!it)
    return NULL;

  snprintf (id, sizeof (id), "%s%c%s", cnt, sub ? ':' : '\0', sub ? sub : "");
  ITEM_SEARCH (it2, it->counters, id)
  return it2;
}

uint64_t
vh_stats_timer_read (vh_stats_tmr_t *timer)
{
  uint64_t time;

  if (!timer)
    return 0;

  pthread_mutex_lock (&timer->mutex);
  time = timer->time.tv_sec * 1000000000UL + timer->time.tv_nsec;
  pthread_mutex_unlock (&timer->mutex);
  return time;
}

uint64_t
vh_stats_counter_read (vh_stats_cnt_t *counter)
{
  uint64_t count;

  if (!counter)
    return 0;

  pthread_mutex_lock (&counter->mutex);
  count = counter->count;
  pthread_mutex_unlock (&counter->mutex);
  return count;
}

void
vh_stats_timer (vh_stats_tmr_t *timer, int start)
{
  struct timespec te, td;

  if (!timer)
    return;

  clock_gettime (CLOCK_REALTIME, start ? &timer->ts : &te);

  if (!start)
  {
    VH_TIMERSUB (&te, &timer->ts, &td);
    pthread_mutex_lock (&timer->mutex);
    VH_TIMERADD (&timer->time, &td, &timer->time);
    pthread_mutex_unlock (&timer->mutex);
  }
}

void
vh_stats_counter (vh_stats_cnt_t *counter, uint64_t val)
{
  if (!counter)
    return;

  pthread_mutex_lock (&counter->mutex);
  counter->count += val;
  pthread_mutex_unlock (&counter->mutex);
}

vh_stats_tmr_t *
vh_stats_grp_timer_add (vh_stats_t *stats,
                        const char *grp, const char *tmr, const char *sub)
{
  char id[64];
  vh_stats_grp_t *it;
  vh_stats_tmr_t *it2;

  if (!stats || !grp || !tmr)
    return NULL;

  it2 = vh_stats_timer_get (stats, grp, tmr, sub);
  if (it2) /* exists? */
    return it2;

  ITEM_SEARCH (it, stats->groups, grp)
  if (!it)
    return NULL;

  ITEM_MALLOC (it2, it->timers, vh_stats_tmr_t)
  if (!it2)
    return NULL;

  pthread_mutex_init (&it2->mutex, NULL);

  snprintf (id, sizeof (id), "%s%c%s", tmr, sub ? ':' : '\0', sub ? sub : "");
  it2->id = strdup (id);
  return it2;
}

vh_stats_cnt_t *
vh_stats_grp_counter_add (vh_stats_t *stats,
                          const char *grp, const char *cnt, const char *sub)
{
  char id[64];
  vh_stats_grp_t *it;
  vh_stats_cnt_t *it2;

  if (!stats || !grp || !cnt)
    return NULL;

  it2 = vh_stats_counter_get (stats, grp, cnt, sub);
  if (it2) /* exists? */
    return it2;

  ITEM_SEARCH (it, stats->groups, grp)
  if (!it)
    return NULL;

  ITEM_MALLOC (it2, it->counters, vh_stats_cnt_t)
  if (!it2)
    return NULL;

  pthread_mutex_init (&it2->mutex, NULL);

  snprintf (id, sizeof (id), "%s%c%s", cnt, sub ? ':' : '\0', sub ? sub : "");
  it2->id = strdup (id);
  return it2;
}

void
vh_stats_grp_add (vh_stats_t *stats, const char *grp,
                  void (*dump) (vh_stats_t *stats, void *data), void *data)
{
  vh_stats_grp_t *it;

  if (!stats || !grp)
    return;

  ITEM_SEARCH (it, stats->groups, grp)
  if (it)
    return; /* group exists */

  ITEM_MALLOC (it, stats->groups, vh_stats_grp_t)
  if (!it)
    return;

  it->id   = strdup (grp);
  it->dump = dump;
  it->data = data;
}

void
vh_stats_dump (vh_stats_t *stats, const char *grp)
{
  vh_stats_grp_t *it;

  if (!stats)
    return;

  if (!vh_log_test (VALHALLA_MSG_INFO))
    return;

  for (it = stats->groups; it; it = it->next)
    if (!grp || !strcmp (it->id, grp))
      if (it->dump)
        it->dump (stats, it->data);
}

void
vh_stats_debug_dump (vh_stats_t *stats)
{
  vh_stats_grp_t *it;

  if (!stats)
    return;

  if (!vh_log_test (VALHALLA_MSG_VERBOSE))
    return;

  vh_log (VALHALLA_MSG_VERBOSE, "Debug statistics dump");
  vh_log (VALHALLA_MSG_VERBOSE, "~~~~~~~~~~~~~~~~~~~~~");

  for (it = stats->groups; it; it = it->next)
  {
    vh_stats_tmr_t *itt;
    vh_stats_cnt_t *itc;
    char c = it->counters ? '|' : ' ';

    vh_log (VALHALLA_MSG_VERBOSE, "%s", it->id);
    if (it->timers)
      vh_log (VALHALLA_MSG_VERBOSE, "%c-- TIMER", it->counters ? '|' : '`');
    for (itt = it->timers; itt; itt = itt->next)
    {
      vh_log (VALHALLA_MSG_VERBOSE, "%c   %c-- %s",
              c, itt->next ? '|' : '`', itt->id);
      vh_log (VALHALLA_MSG_VERBOSE, "%c   %c   |-- sec  : %lu",
              c, itt->next ? '|' : ' ', itt->time.tv_sec);
      vh_log (VALHALLA_MSG_VERBOSE, "%c   %c   `-- nsec : %lu",
              c, itt->next ? '|' : ' ', itt->time.tv_nsec);
    }
    if (it->counters)
      vh_log (VALHALLA_MSG_VERBOSE, "`-- COUNTER");
    for (itc = it->counters; itc; itc = itc->next)
    {
      vh_log (VALHALLA_MSG_VERBOSE, "    %c-- %s",
              itc->next ? '|' : '`', itc->id);
      vh_log (VALHALLA_MSG_VERBOSE, "    %c   `-- accu : %lu",
              itc->next ? '|' : ' ', itc->count);
    }
  }
}

const char *
vh_stats_group_next (vh_stats_t *stats, const char *id)
{
  vh_stats_grp_t *it;

  vh_log (VALHALLA_MSG_VERBOSE, __FUNCTION__);

  if (!stats)
    return NULL;

  if (!id) /* first group? */
    return stats->groups ? stats->groups->id : NULL;

  ITEM_SEARCH (it, stats->groups, id)
  if (!it)
    return NULL;

  it = it->next;
  return it ? it->id : NULL;
}

uint64_t
vh_stats_read_next (vh_stats_t *stats, const char *id,
                    valhalla_stats_type_t type, const char **item)
{
  vh_stats_tmr_t *tmr;
  vh_stats_cnt_t *cnt;
  vh_stats_grp_t *it;

  vh_log (VALHALLA_MSG_VERBOSE, __FUNCTION__);

  if (!stats || !id || !item)
    return 0;

  ITEM_SEARCH (it, stats->groups, id)
  if (!it)
    return 0;

  switch (type)
  {
  case VALHALLA_STATS_TIMER:
    for (tmr = it->timers; tmr; tmr = tmr->next)
    {
      if (!*item)
      {
        *item = tmr->id;
        return vh_stats_timer_read (tmr);
      }
      else if (!strcmp (tmr->id, *item))
      {
        *item = NULL;
        tmr = tmr->next;
        if (!tmr)
          break;
        *item = tmr->id;
        return vh_stats_timer_read (tmr);
      }
    }
    break;

  case VALHALLA_STATS_COUNTER:
    for (cnt = it->counters; cnt; cnt = cnt->next)
    {
      if (!*item)
      {
        *item = cnt->id;
        return vh_stats_counter_read (cnt);
      }
      else if (!strcmp (cnt->id, *item))
      {
        *item = NULL;
        cnt = cnt->next;
        if (!cnt)
          break;
        *item = cnt->id;
        return vh_stats_counter_read (cnt);
      }
    }
    break;
  }

  return 0;
}

vh_stats_t *
vh_stats_new (void)
{
  return calloc (1, sizeof (vh_stats_t));
}

static void
stats_timer_free (vh_stats_tmr_t *timers)
{
  vh_stats_tmr_t *tmp;

  for (; timers; timers = tmp)
  {
    tmp = timers->next;
    free (timers->id);
    pthread_mutex_destroy (&timers->mutex);
    free (timers);
  }
}

static void
stats_counter_free (vh_stats_cnt_t *counters)
{
  vh_stats_cnt_t *tmp;

  for (; counters; counters = tmp)
  {
    tmp = counters->next;
    free (counters->id);
    pthread_mutex_destroy (&counters->mutex);
    free (counters);
  }
}

static void
stats_grp_free (vh_stats_grp_t *groups)
{
  vh_stats_grp_t *tmp;

  for (; groups; groups = tmp)
  {
    tmp = groups->next;
    free (groups->id);
    stats_timer_free (groups->timers);
    stats_counter_free (groups->counters);
    free (groups);
  }
}

void
vh_stats_free (vh_stats_t *stats)
{
  if (!stats)
    return;

  stats_grp_free (stats->groups);
  free (stats);
}
