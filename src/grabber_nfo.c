/*
 * GeeXboX Valhalla: tiny media scanner API.
 * Copyright (C) 2009 Benjamin Zores <ben@geexbox.org>
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
#include <stdio.h>
#include <nfo.h>

#include "grabber_common.h"
#include "grabber_nfo.h"
#include "metadata.h"
#include "utils.h"
#include "logs.h"

#define GRABBER_CAP_FLAGS \
  GRABBER_CAP_VIDEO

typedef struct grabber_nfo_s {
  /* unused */
} grabber_nfo_t;

/****************************************************************************/
/* Common Grabber                                                           */
/****************************************************************************/

static void
grab_nfo_actor (nfo_actor_t *actor, file_data_t *data)
{
  const char *name, *role;

  if (!actor || !data)
    return;

  name = nfo_actor_get (actor, NFO_ACTOR_NAME);
  role = nfo_actor_get (actor, NFO_ACTOR_ROLE);

  if (name)
  {
    char str[1024] = { 0 };

    if (role && strcmp (role, ""))
      snprintf (str, sizeof (str), "%s (%s)", name, role);
    else
      snprintf (str, sizeof (str), "%s", name);
    vh_metadata_add (&data->meta_grabber,
                     "actor", str, VALHALLA_META_GRP_ENTITIES);
  }
}

#define META_VIDEO_ADD(meta,field)                                         \
  if (nfo_video_stream_get (video, NFO_VIDEO_##field))                     \
    vh_metadata_add (&data->meta_grabber, meta,                            \
                     nfo_video_stream_get (video, NFO_VIDEO_##field),      \
                     VALHALLA_META_GRP_MISCELLANEOUS);

static void
grab_nfo_video_stream (nfo_stream_video_t *video, int id, file_data_t *data)
{
  if (!video || !data)
    return;

  META_VIDEO_ADD ("width",        WIDTH);
  META_VIDEO_ADD ("height",       HEIGHT);
  META_VIDEO_ADD ("video_codec",        CODEC);
  META_VIDEO_ADD ("duration",     DURATION);
  META_VIDEO_ADD ("video_bitrate",      BITRATE);
}

#define META_AUDIO_ADD(meta,field)                                         \
  if (nfo_audio_stream_get (audio, NFO_AUDIO_##field))                     \
    vh_metadata_add (&data->meta_grabber, meta,                            \
                     nfo_audio_stream_get (audio, NFO_AUDIO_##field),      \
                     VALHALLA_META_GRP_MISCELLANEOUS);

static void
grab_nfo_audio_stream (nfo_stream_audio_t *audio, int id, file_data_t *data)
{
  if (!audio || !data)
    return;

  META_AUDIO_ADD ("lang",     LANG);
  META_AUDIO_ADD ("audio_codec",    CODEC);
  META_AUDIO_ADD ("channels", CHANNELS);
  META_AUDIO_ADD ("audio_bitrate",  BITRATE);
}

#define META_SUB_ADD(meta,field)                                           \
  if (nfo_sub_stream_get (sub, NFO_SUB_##field))                           \
    vh_metadata_add (&data->meta_grabber, meta,                            \
                     nfo_sub_stream_get (sub, NFO_SUB_##field),            \
                     VALHALLA_META_GRP_MISCELLANEOUS);

static void
grab_nfo_sub_stream (nfo_stream_sub_t *sub, int id, file_data_t *data)
{
  if (!sub || !data)
    return;

  META_SUB_ADD ("sub_lang", LANG);
}

/****************************************************************************/
/* Movie Grabber                                                            */
/****************************************************************************/

#define META_MOVIE_ADD(meta,field)                                         \
  if (nfo_movie_get (movie, NFO_MOVIE_##field))                            \
    vh_metadata_add (&data->meta_grabber, meta,                            \
                     nfo_movie_get (movie, NFO_MOVIE_##field),             \
                     VALHALLA_META_GRP_MISCELLANEOUS);

static void
grab_nfo_movie (nfo_t *nfo, file_data_t *data)
{
  nfo_movie_t *movie;
  int i, c;

  if (!nfo || !data)
    return;

  movie = nfo_get_movie (nfo);
  if (!movie)
    return;

  META_MOVIE_ADD ("title",          TITLE);
  META_MOVIE_ADD ("original_title", ORIGINAL_TITLE);
  META_MOVIE_ADD ("rating",         RATING);
  META_MOVIE_ADD ("year",           YEAR);
  META_MOVIE_ADD ("plot",           PLOT);
  META_MOVIE_ADD ("thumb",          THUMB);
  META_MOVIE_ADD ("fanart",         FAN_ART);
  META_MOVIE_ADD ("mpaa",           MPAA);
  META_MOVIE_ADD ("playcount",      PLAY_COUNT);
  META_MOVIE_ADD ("watched",        WATCHED);
  META_MOVIE_ADD ("genre",          GENRE);
  META_MOVIE_ADD ("credits",        CREDITS);
  META_MOVIE_ADD ("director",       DIRECTOR);
  META_MOVIE_ADD ("studio",         STUDIO);

  c = nfo_movie_get_actors_count (movie);
  for (i = 0; i < c; i++)
  {
    nfo_actor_t *actor;

    actor = nfo_movie_get_actor (movie, i);
    grab_nfo_actor (actor, data);
  }

  c = nfo_movie_get_video_streams_count (movie);
  for (i = 0; i < c; i++)
  {
    nfo_stream_video_t *video;

    video = nfo_movie_get_video_stream (movie, i);
    grab_nfo_video_stream (video, i, data);
  }

  c = nfo_movie_get_audio_streams_count (movie);
  for (i = 0; i < c; i++)
  {
    nfo_stream_audio_t *audio;

    audio = nfo_movie_get_audio_stream (movie, i);
    grab_nfo_audio_stream (audio, i, data);
  }

  c = nfo_movie_get_sub_streams_count (movie);
  for (i = 0; i < c; i++)
  {
    nfo_stream_sub_t *sub;

    sub = nfo_movie_get_sub_stream (movie, i);
    grab_nfo_sub_stream (sub, i, data);
  }
}

/****************************************************************************/
/* TVShow Grabber                                                           */
/****************************************************************************/

#define META_SHOW_ADD(meta,field)                                          \
  if (nfo_tvshow_get (tvshow, NFO_TVSHOW_##field))                         \
    vh_metadata_add (&data->meta_grabber, meta,                            \
                     nfo_tvshow_get (tvshow, NFO_TVSHOW_##field),          \
                     VALHALLA_META_GRP_MISCELLANEOUS);

static void
grab_nfo_show (nfo_tvshow_t *tvshow, file_data_t *data)
{
  if (!tvshow || !data)
    return;

  META_SHOW_ADD ("title",          TITLE);
  META_SHOW_ADD ("rating",         RATING);
  META_SHOW_ADD ("plot",           PLOT);
  META_SHOW_ADD ("mpaa",           MPAA);
  META_SHOW_ADD ("watched",        WATCHED);
  META_SHOW_ADD ("genre",          GENRE);
  META_SHOW_ADD ("premiered",      PREMIERED);
  META_SHOW_ADD ("studio",         STUDIO);
  META_SHOW_ADD ("cover",          FANART);
  META_SHOW_ADD ("cover_header",   FANART_HEADER);
  META_SHOW_ADD ("cover_cover",    FANART_COVER);
}

#define META_EPISODE_ADD(meta,field)                                       \
  if (nfo_tvshow_episode_get (episode, NFO_TVSHOW_EPISODE_##field))        \
    vh_metadata_add (&data->meta_grabber, meta,                            \
                     nfo_tvshow_episode_get (episode,                      \
                                             NFO_TVSHOW_EPISODE_##field),  \
                     VALHALLA_META_GRP_MISCELLANEOUS);

static void
grab_nfo_tvshow (nfo_t *nfo, file_data_t *data)
{
  nfo_tvshow_episode_t *episode;
  nfo_tvshow_t *tvshow;
  int i, c;

  if (!nfo || !data)
    return;

  episode = nfo_get_tvshow_episode (nfo);
  if (!episode)
    return;

  META_EPISODE_ADD ("title",        TITLE);
  META_EPISODE_ADD ("rating",       RATING);
  META_EPISODE_ADD ("season",       SEASON);
  META_EPISODE_ADD ("episode",      EPISODE);
  META_EPISODE_ADD ("plot",         PLOT);
  META_EPISODE_ADD ("thumb",        THUMB);
  META_EPISODE_ADD ("cover",        FANART);
  META_EPISODE_ADD ("cover_season", FANART_SEASON);
  META_EPISODE_ADD ("playcount",    PLAY_COUNT);
  META_EPISODE_ADD ("credits",      CREDITS);
  META_EPISODE_ADD ("director",     DIRECTOR);

  tvshow = nfo_tvshow_episode_get_show (episode);
  grab_nfo_show (tvshow, data);

  c = nfo_tvshow_episode_get_actors_count (episode);
  for (i = 0; i < c; i++)
  {
    nfo_actor_t *actor;

    actor = nfo_tvshow_episode_get_actor (episode, i);
    grab_nfo_actor (actor, data);
  }

  c = nfo_tvshow_episode_get_video_streams_count (episode);
  for (i = 0; i < c; i++)
  {
    nfo_stream_video_t *video;

    video = nfo_tvshow_episode_get_video_stream (episode, i);
    grab_nfo_video_stream (video, i, data);
  }

  c = nfo_tvshow_episode_get_audio_streams_count (episode);
  for (i = 0; i < c; i++)
  {
    nfo_stream_audio_t *audio;

    audio = nfo_tvshow_episode_get_audio_stream (episode, i);
    grab_nfo_audio_stream (audio, i, data);
  }

  c = nfo_tvshow_episode_get_sub_streams_count (episode);
  for (i = 0; i < c; i++)
  {
    nfo_stream_sub_t *sub;

    sub = nfo_tvshow_episode_get_sub_stream (episode, i);
    grab_nfo_sub_stream (sub, i, data);
  }
}

/****************************************************************************/
/* Private Grabber API                                                      */
/****************************************************************************/

static void *
grabber_nfo_priv (void)
{
  valhalla_log (VALHALLA_MSG_VERBOSE, __FUNCTION__);

  return calloc (1, sizeof (grabber_nfo_t));
}

static int
grabber_nfo_init (void *priv)
{
  valhalla_log (VALHALLA_MSG_VERBOSE, __FUNCTION__);

  return 0;
}

static void
grabber_nfo_uninit (void *priv)
{
  grabber_nfo_t *nfo = priv;

  valhalla_log (VALHALLA_MSG_VERBOSE, __FUNCTION__);

  free (nfo);
}

static int
grabber_nfo_grab (void *priv, file_data_t *data)
{
  nfo_type_t type;
  nfo_t *nfo;

  valhalla_log (VALHALLA_MSG_VERBOSE, __FUNCTION__);

  nfo = nfo_init (data->file);
  if (!nfo)
    return 1;

  type = nfo_get_type (nfo);
  switch (type)
  {
  case NFO_MOVIE:
    grab_nfo_movie (nfo, data);
    break;
  case NFO_TVSHOW:
    grab_nfo_tvshow (nfo, data);
    break;
  default:
    valhalla_log (VALHALLA_MSG_VERBOSE,
                  "No NFO file available or unsupported type.\n");
    break;
  }

  nfo_free (nfo);
  return 0;
}

/****************************************************************************/
/* Public Grabber API                                                       */
/****************************************************************************/

/* vh_grabber_nfo_register () */
GRABBER_REGISTER (nfo,
                  GRABBER_CAP_FLAGS,
                  grabber_nfo_priv,
                  grabber_nfo_init,
                  grabber_nfo_uninit,
                  grabber_nfo_grab,
                  NULL)
