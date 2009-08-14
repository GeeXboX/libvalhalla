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
 * Foundation, Inc, 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */

#include <stdlib.h>

#include "grabber_common.h"
#include "grabber_dummy.h"
#include "metadata.h"
#include "logs.h"

#define GRABBER_CAP_FLAGS \
  GRABBER_CAP_AUDIO | \
  GRABBER_CAP_VIDEO | \
  GRABBER_CAP_IMAGE

typedef struct grabber_dummy_s {
  int dummy;
} grabber_dummy_t;


/******************************************************************************/
/* Private Grabber API                                                        */
/******************************************************************************/

static void *
grabber_dummy_priv (void)
{
  grabber_dummy_t *dummy;

  valhalla_log (VALHALLA_MSG_VERBOSE, __FUNCTION__);

  dummy = calloc (1, sizeof (grabber_dummy_t));
  if (!dummy)
    return NULL;

  dummy->dummy = 1;

  return dummy;
}

static int
grabber_dummy_init (void *priv)
{
  grabber_dummy_t *dummy = priv;

  valhalla_log (VALHALLA_MSG_VERBOSE, __FUNCTION__);

  if (!dummy)
    return -1;

  return 0;
}

static void
grabber_dummy_uninit (void *priv)
{
  grabber_dummy_t *dummy = priv;

  valhalla_log (VALHALLA_MSG_VERBOSE, __FUNCTION__);

  if (!dummy)
    return;

  free (dummy);
}

static int
grabber_dummy_grab (void *priv, file_data_t *data)
{
  valhalla_log (VALHALLA_MSG_VERBOSE, __FUNCTION__);

  return 0;
}

static void
grabber_dummy_loop (void *priv)
{
  valhalla_log (VALHALLA_MSG_VERBOSE, __FUNCTION__);

  return;
}

/******************************************************************************/
/* Public Grabber API                                                         */
/******************************************************************************/

/* grabber_dummy_register () */
GRABBER_REGISTER (dummy,
                  GRABBER_CAP_FLAGS,
                  grabber_dummy_priv,
                  grabber_dummy_init,
                  grabber_dummy_uninit,
                  grabber_dummy_grab,
                  grabber_dummy_loop)
