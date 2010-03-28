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

#include <stdlib.h>
#include <string.h>

#include "valhalla.h"
#include "valhalla_internals.h"
#include "utils.h"
#include "metadata.h"
#include "logs.h"


static const struct {
  const char *meta;
  valhalla_meta_grp_t grp;
} metadata_group_mapping[] = {

  /* Classification */
  { VALHALLA_METADATA_CATEGORY,            VALHALLA_META_GRP_CLASSIFICATION },
  { VALHALLA_METADATA_EPISODE,             VALHALLA_META_GRP_CLASSIFICATION },
  { VALHALLA_METADATA_GENRE,               VALHALLA_META_GRP_CLASSIFICATION },
  { VALHALLA_METADATA_MPAA,                VALHALLA_META_GRP_CLASSIFICATION },
  { VALHALLA_METADATA_RUNTIME,             VALHALLA_META_GRP_CLASSIFICATION },
  { VALHALLA_METADATA_SEASON,              VALHALLA_META_GRP_CLASSIFICATION },
  { VALHALLA_METADATA_SYNOPSIS,            VALHALLA_META_GRP_CLASSIFICATION },
  { VALHALLA_METADATA_SYNOPSIS_SHOW,       VALHALLA_META_GRP_CLASSIFICATION },

  /* Commercial */
  { VALHALLA_METADATA_BUDGET,              VALHALLA_META_GRP_COMMERCIAL     },
  { VALHALLA_METADATA_COUNTRY,             VALHALLA_META_GRP_COMMERCIAL     },
  { VALHALLA_METADATA_REVENUE,             VALHALLA_META_GRP_COMMERCIAL     },
  { VALHALLA_METADATA_STUDIO,              VALHALLA_META_GRP_COMMERCIAL     },

  /* Entities */
  { VALHALLA_METADATA_ACTOR,               VALHALLA_META_GRP_ENTITIES       },
  { VALHALLA_METADATA_ARTIST,              VALHALLA_META_GRP_ENTITIES       },
  { VALHALLA_METADATA_AUTHOR,              VALHALLA_META_GRP_ENTITIES       },
  { VALHALLA_METADATA_CASTING,             VALHALLA_META_GRP_ENTITIES       },
  { VALHALLA_METADATA_COMPOSER,            VALHALLA_META_GRP_ENTITIES       },
  { VALHALLA_METADATA_CREDITS,             VALHALLA_META_GRP_ENTITIES       },
  { VALHALLA_METADATA_DIRECTOR,            VALHALLA_META_GRP_ENTITIES       },
  { VALHALLA_METADATA_DIRECTOR_PHOTO,      VALHALLA_META_GRP_ENTITIES       },
  { VALHALLA_METADATA_EDITOR,              VALHALLA_META_GRP_ENTITIES       },
  { VALHALLA_METADATA_PRODUCER,            VALHALLA_META_GRP_ENTITIES       },

  /* Miscellaneous */
  { VALHALLA_METADATA_COVER,               VALHALLA_META_GRP_MISCELLANEOUS  },
  { VALHALLA_METADATA_COVER_SEASON,        VALHALLA_META_GRP_MISCELLANEOUS  },
  { VALHALLA_METADATA_COVER_SHOW,          VALHALLA_META_GRP_MISCELLANEOUS  },
  { VALHALLA_METADATA_COVER_SHOW_HEADER,   VALHALLA_META_GRP_MISCELLANEOUS  },
  { VALHALLA_METADATA_FAN_ART,             VALHALLA_META_GRP_MISCELLANEOUS  },
  { VALHALLA_METADATA_LYRICS,              VALHALLA_META_GRP_MISCELLANEOUS  },
  { VALHALLA_METADATA_THUMBNAIL,           VALHALLA_META_GRP_MISCELLANEOUS  },

  /* Organizational */
  { VALHALLA_METADATA_TRACK,               VALHALLA_META_GRP_ORGANIZATIONAL },

  /* Personal */
  { VALHALLA_METADATA_PLAY_COUNT,          VALHALLA_META_GRP_PERSONAL       },
  { VALHALLA_METADATA_RATING,              VALHALLA_META_GRP_PERSONAL       },
  { VALHALLA_METADATA_WATCHED,             VALHALLA_META_GRP_PERSONAL       },

  /* Technical */
  { VALHALLA_METADATA_AUDIO_BITRATE,       VALHALLA_META_GRP_TECHNICAL      },
  { VALHALLA_METADATA_AUDIO_CHANNELS,      VALHALLA_META_GRP_TECHNICAL      },
  { VALHALLA_METADATA_AUDIO_CODEC,         VALHALLA_META_GRP_TECHNICAL      },
  { VALHALLA_METADATA_AUDIO_LANG,          VALHALLA_META_GRP_TECHNICAL      },
  { VALHALLA_METADATA_AUDIO_STREAMS,       VALHALLA_META_GRP_TECHNICAL      },
  { VALHALLA_METADATA_DURATION,            VALHALLA_META_GRP_TECHNICAL      },
  { VALHALLA_METADATA_FILESIZE,            VALHALLA_META_GRP_TECHNICAL      },
  { VALHALLA_METADATA_HEIGHT,              VALHALLA_META_GRP_TECHNICAL      },
  { VALHALLA_METADATA_PICTURE_ORIENTATION, VALHALLA_META_GRP_TECHNICAL      },
  { VALHALLA_METADATA_SUB_LANG,            VALHALLA_META_GRP_TECHNICAL      },
  { VALHALLA_METADATA_SUB_STREAMS,         VALHALLA_META_GRP_TECHNICAL      },
  { VALHALLA_METADATA_VIDEO_ASPECT,        VALHALLA_META_GRP_TECHNICAL      },
  { VALHALLA_METADATA_VIDEO_BITRATE,       VALHALLA_META_GRP_TECHNICAL      },
  { VALHALLA_METADATA_VIDEO_CODEC,         VALHALLA_META_GRP_TECHNICAL      },
  { VALHALLA_METADATA_VIDEO_STREAMS,       VALHALLA_META_GRP_TECHNICAL      },
  { VALHALLA_METADATA_WIDTH,               VALHALLA_META_GRP_TECHNICAL      },

  /* Temporal */
  { VALHALLA_METADATA_DATE,                VALHALLA_META_GRP_TEMPORAL       },
  { VALHALLA_METADATA_PREMIERED,           VALHALLA_META_GRP_TEMPORAL       },
  { VALHALLA_METADATA_YEAR,                VALHALLA_META_GRP_TEMPORAL       },

  /* Titles */
  { VALHALLA_METADATA_ALBUM,               VALHALLA_META_GRP_TITLES         },
  { VALHALLA_METADATA_TITLE,               VALHALLA_META_GRP_TITLES         },
  { VALHALLA_METADATA_TITLE_ALTERNATIVE,   VALHALLA_META_GRP_TITLES         },
  { VALHALLA_METADATA_TITLE_SHOW,          VALHALLA_META_GRP_TITLES         },

  { NULL,                                  VALHALLA_META_GRP_NIL            },
};

static const char *const metadata_group_str[] = {
  [VALHALLA_META_GRP_NIL]             = "null",
  [VALHALLA_META_GRP_CLASSIFICATION]  = "classification",
  [VALHALLA_META_GRP_COMMERCIAL]      = "commercial",
  [VALHALLA_META_GRP_CONTACT]         = "contact",
  [VALHALLA_META_GRP_ENTITIES]        = "entities",
  [VALHALLA_META_GRP_IDENTIFIER]      = "identifier",
  [VALHALLA_META_GRP_LEGAL]           = "legal",
  [VALHALLA_META_GRP_MISCELLANEOUS]   = "miscellaneous",
  [VALHALLA_META_GRP_MUSICAL]         = "musical",
  [VALHALLA_META_GRP_ORGANIZATIONAL]  = "organizational",
  [VALHALLA_META_GRP_PERSONAL]        = "personal",
  [VALHALLA_META_GRP_SPACIAL]         = "spacial",
  [VALHALLA_META_GRP_TECHNICAL]       = "technical",
  [VALHALLA_META_GRP_TEMPORAL]        = "temporal",
  [VALHALLA_META_GRP_TITLES]          = "titles",
};

const size_t vh_metadata_group_size =
  ARRAY_NB_ELEMENTS (metadata_group_str);


const char *
vh_metadata_group_str (valhalla_meta_grp_t group)
{
  if (group >= vh_metadata_group_size)
    return NULL;

  return metadata_group_str[group];
}

int
vh_metadata_get (const metadata_t *meta,
                 const char *name, int flags, const metadata_t **tag)
{
  if (!meta || !tag || !name)
    return -1;

  if (*tag)
    meta = (*tag)->next;

  for (; meta; meta = meta->next)
    if (flags & METADATA_IGNORE_SUFFIX
        ? !strncmp (name, meta->name, strlen (name))
        : !strcmp (name, meta->name))
      break;

  if (!meta)
    return -1;

  *tag = meta;
  return 0;
}

void
vh_metadata_dup (metadata_t **dst, const metadata_t *src)
{
  metadata_t *st = NULL, *tmp = NULL;

  if (!dst || !src)
    return;

  for (; src; src = src->next)
  {
    if (tmp)
    {
      tmp->next = calloc (1, sizeof (metadata_t));
      tmp = tmp->next;
    }
    else
      tmp = calloc (1, sizeof (metadata_t));

    if (!tmp)
      goto err;

    tmp->name     = strdup (src->name);
    tmp->value    = strdup (src->value);
    tmp->group    = src->group;
    tmp->priority = src->priority;

    if (!st)
      st = tmp;
  }

  *dst = st;
  return;

 err:
  vh_metadata_free (st);
  *dst = NULL;
}

void
vh_metadata_free (metadata_t *meta)
{
  metadata_t *tmp;

  while (meta)
  {
    free (meta->name);
    free (meta->value);
    tmp = meta->next;
    free (meta);
    meta = tmp;
  }
}

void
vh_metadata_add (metadata_t **meta,
                 const char *name, const char *value,
                 valhalla_meta_grp_t group, valhalla_metadata_pl_t priority)
{
  metadata_t *it;

  if (!meta || !name || !value || !*value)
    return;

  if (!*meta)
  {
    *meta = calloc (1, sizeof (metadata_t));
    it = *meta;
  }
  else
  {
    for (it = *meta; it->next; it = it->next)
      ;
    it->next = calloc (1, sizeof (metadata_t));
    it = it->next;
  }

  if (!it)
    return;

  it->name  = strdup (name);
  vh_strtolower (it->name);

  it->value    = strdup (value);
  it->group    = group;
  it->priority = priority;

  vh_log (VALHALLA_MSG_VERBOSE,
          "Adding new metadata '%s' with value '%s'.", it->name, it->value);
}

static inline valhalla_metadata_pl_t
metadata_priority_get (const char *name, const metadata_plist_t *pl)
{
  for (; pl->metadata; pl++)
    if (!strcasecmp (name, pl->metadata))
      return pl->priority;
  return pl->priority;
}

void
vh_metadata_add_auto (metadata_t **meta,
                      const char *name, const char *value,
                      const metadata_plist_t *pl)
{
  unsigned int i;
  valhalla_meta_grp_t grp;
  valhalla_metadata_pl_t priority;

  if (!meta || !name || !value || !*value)
    return;

  grp = VALHALLA_META_GRP_MISCELLANEOUS;
  for (i = 0; metadata_group_mapping[i].meta; i++)
    if (!strcasecmp (name, metadata_group_mapping[i].meta))
    {
      grp = metadata_group_mapping[i].grp;
      break;
    }

  priority =
    pl ? metadata_priority_get (name, pl) : VALHALLA_METADATA_PL_NORMAL;

  vh_metadata_add (meta, name, value, grp, priority);
}

void
vh_metadata_plist_dump (const metadata_plist_t *pl)
{
  if (!vh_log_test (VALHALLA_MSG_VERBOSE))
    return;

  vh_log (VALHALLA_MSG_VERBOSE, "  _________________________");
  for (; pl->metadata; pl++)
    vh_log (VALHALLA_MSG_VERBOSE,
            " | %-15s | %5i |", pl->metadata, pl->priority);
  vh_log (VALHALLA_MSG_VERBOSE, " | *               | %5i |", pl->priority);
  vh_log (VALHALLA_MSG_VERBOSE, " |_________________|_______|");
}

valhalla_metadata_pl_t
vh_metadata_plist_read (metadata_plist_t *pl, const char **metadata)
{
  metadata_plist_t *it;

  if (!pl || !metadata)
    return 0;

  for (it = pl; it->metadata; it++)
    if (*metadata && !strcmp (it->metadata, *metadata))
    {
      *metadata = (it + 1)->metadata;
      return it->priority;
    }

  if (*metadata)
    return 0;

  *metadata = pl->metadata;
  return it->priority;
}

void
vh_metadata_plist_set (metadata_plist_t **pl,
                       const char *metadata, valhalla_metadata_pl_t p)
{
  size_t size;
  metadata_plist_t *it, *n, *f = NULL, *d = NULL;

  if (!pl || !*pl)
    return;

  if (!METADATA_PL_CHECK (p))
    return;

  /* search the entry (if exists) in the list */
  for (it = *pl; it->metadata; it++)
    if (!f && metadata && !strcmp (it->metadata, metadata))
      f = it;
  d = it; /* default entry */

  /* is the default priority? */
  if (d->priority == p)
  {
    if (f) /* f is equal to d and must be removed */
    {
      /*      _____________
       *     |_____*pl_____|        | addresses
       *     |______f______| <-.    v
       * .-> |_____________| --'
       * '-- |_____________| <-.
       *     |_ _ _ d _ _ _| --'
       */
      for (it = f + 1; it <= d; it++)
        *(it - 1) = *it;

      /* trim d */
      n = realloc (*pl, (d - *pl) * sizeof (metadata_plist_t));
      if (n)
        *pl = n;
    }
    return;
  }

  /* new default priority */
  if (!metadata)
  {
    d->priority = p;
    return;
  }

  /* f exists */
  if (f)
  {
    f->priority = p;
    return;
  }

  /* new entry before the default priority */
  size = d - *pl + 1;
  n = realloc (*pl, (size + 1) * sizeof (metadata_plist_t));
  if (!n)
    return;

  /*      _____________
   *     |_____*pl_____|        | addresses
   *     |_____________|        v
   *     |_____________|
   *     |_____________|
   *  => |______d______| --.
   *     |_ _ _ _ _ _ _| <-'
   */
  d = n + size - 1;
  *(d + 1) = *d;
  d->metadata = metadata;
  d->priority = p;
  *pl = n;
}
