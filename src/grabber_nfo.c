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
    vh_metadata_add_auto (&data->meta_grabber, VALHALLA_METADATA_ACTOR, str);
  }
}

#define META_VIDEO_ADD(meta, field)                                        \
  if (nfo_video_stream_get (video, NFO_VIDEO_##field))                     \
    vh_metadata_add_auto (&data->meta_grabber, VALHALLA_METADATA_##meta,   \
                     nfo_video_stream_get (video, NFO_VIDEO_##field));     \

static void
grab_nfo_video_stream (nfo_stream_video_t *video, file_data_t *data)
{
  if (!video || !data)
    return;

  META_VIDEO_ADD (WIDTH,         WIDTH);
  META_VIDEO_ADD (HEIGHT,        HEIGHT);
  META_VIDEO_ADD (VIDEO_CODEC,   CODEC);
  META_VIDEO_ADD (DURATION,      DURATION);
  META_VIDEO_ADD (VIDEO_BITRATE, BITRATE);
}

#define META_AUDIO_ADD(meta, field)                                        \
  if (nfo_audio_stream_get (audio, NFO_AUDIO_##field))                     \
    vh_metadata_add_auto (&data->meta_grabber, VALHALLA_METADATA_##meta,   \
                     nfo_audio_stream_get (audio, NFO_AUDIO_##field));     \

static void
grab_nfo_audio_stream (nfo_stream_audio_t *audio, file_data_t *data)
{
  if (!audio || !data)
    return;

  META_AUDIO_ADD (AUDIO_LANG,     LANG);
  META_AUDIO_ADD (AUDIO_CODEC,    CODEC);
  META_AUDIO_ADD (AUDIO_CHANNELS, CHANNELS);
  META_AUDIO_ADD (AUDIO_BITRATE,  BITRATE);
}

#define META_SUB_ADD(meta, field)                                          \
  if (nfo_sub_stream_get (sub, NFO_SUB_##field))                           \
    vh_metadata_add_auto (&data->meta_grabber, VALHALLA_METADATA_##meta,   \
                     nfo_sub_stream_get (sub, NFO_SUB_##field));           \

static void
grab_nfo_sub_stream (nfo_stream_sub_t *sub, file_data_t *data)
{
  if (!sub || !data)
    return;

  META_SUB_ADD (SUB_LANG, LANG);
}

/****************************************************************************/
/* Movie Grabber                                                            */
/****************************************************************************/

#define META_MOVIE_ADD(meta, field)                                        \
  if (nfo_movie_get (movie, NFO_MOVIE_##field))                            \
    vh_metadata_add_auto (&data->meta_grabber, VALHALLA_METADATA_##meta,   \
                     nfo_movie_get (movie, NFO_MOVIE_##field));            \

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

  META_MOVIE_ADD (TITLE,             TITLE);
  META_MOVIE_ADD (TITLE_ALTERNATIVE, ORIGINAL_TITLE);
  META_MOVIE_ADD (RATING,            RATING);
  META_MOVIE_ADD (YEAR,              YEAR);
  META_MOVIE_ADD (SYNOPSIS,          PLOT);
  META_MOVIE_ADD (THUMBNAIL,         THUMB);
  META_MOVIE_ADD (FAN_ART,           FAN_ART);
  META_MOVIE_ADD (MPAA,              MPAA);
  META_MOVIE_ADD (PLAY_COUNT,        PLAY_COUNT);
  META_MOVIE_ADD (WATCHED,           WATCHED);
  META_MOVIE_ADD (GENRE,             GENRE);
  META_MOVIE_ADD (CREDITS,           CREDITS);
  META_MOVIE_ADD (DIRECTOR,          DIRECTOR);
  META_MOVIE_ADD (STUDIO,            STUDIO);

  if (nfo_movie_get (movie, NFO_MOVIE_RATING))
  {
    char str[16], *rating;

    rating = nfo_movie_get (movie, NFO_MOVIE_RATING);
    snprintf (rating, sizeof (rating), "%d", atoi (rating) / 2);
    vh_metadata_add_auto (&data->meta_grabber, VALHALLA_METADATA_RATING, str);
  }

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
    grab_nfo_video_stream (video, data);
  }

  c = nfo_movie_get_audio_streams_count (movie);
  for (i = 0; i < c; i++)
  {
    nfo_stream_audio_t *audio;

    audio = nfo_movie_get_audio_stream (movie, i);
    grab_nfo_audio_stream (audio, data);
  }

  c = nfo_movie_get_sub_streams_count (movie);
  for (i = 0; i < c; i++)
  {
    nfo_stream_sub_t *sub;

    sub = nfo_movie_get_sub_stream (movie, i);
    grab_nfo_sub_stream (sub, data);
  }
}

/****************************************************************************/
/* TVShow Grabber                                                           */
/****************************************************************************/

#define META_SHOW_ADD(meta, field)                                         \
  if (nfo_tvshow_get (tvshow, NFO_TVSHOW_##field))                         \
    vh_metadata_add_auto (&data->meta_grabber, VALHALLA_METADATA_##meta,   \
                     nfo_tvshow_get (tvshow, NFO_TVSHOW_##field));         \

static void
grab_nfo_show (nfo_tvshow_t *tvshow, file_data_t *data)
{
  if (!tvshow || !data)
    return;

  META_SHOW_ADD (TITLE,             TITLE);
  META_SHOW_ADD (SYNOPSIS_SHOW,     PLOT);
  META_SHOW_ADD (PREMIERED,         PREMIERED);
  META_SHOW_ADD (STUDIO,            STUDIO);
  META_SHOW_ADD (FAN_ART   ,        FANART);
  META_SHOW_ADD (COVER_SHOW_HEADER, FANART_HEADER);
  META_SHOW_ADD (COVER_SHOW,        FANART_COVER);
}

#define META_EPISODE_ADD(meta, field)                                      \
  if (nfo_tvshow_episode_get (episode, NFO_TVSHOW_EPISODE_##field))        \
    vh_metadata_add_auto (&data->meta_grabber, VALHALLA_METADATA_##meta,   \
                     nfo_tvshow_episode_get (episode,                      \
                                             NFO_TVSHOW_EPISODE_##field)); \

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

  META_EPISODE_ADD (TITLE,        TITLE);
  META_EPISODE_ADD (RATING,       RATING);
  META_EPISODE_ADD (SEASON,       SEASON);
  META_EPISODE_ADD (EPISODE,      EPISODE);
  META_EPISODE_ADD (SYNOPSIS,     PLOT);
  META_EPISODE_ADD (THUMBNAIL,    THUMB);
  META_EPISODE_ADD (FAN_ART,      FANART);
  META_EPISODE_ADD (COVER_SEASON, FANART_SEASON);
  META_EPISODE_ADD (PLAY_COUNT,   PLAY_COUNT);
  META_EPISODE_ADD (CREDITS,      CREDITS);
  META_EPISODE_ADD (DIRECTOR,     DIRECTOR);

  if (nfo_tvshow_episode_get (episode, NFO_MOVIE_RATING))
  {
    char str[16], *rating;

    rating = nfo_tvshow_episode_get (episode, NFO_MOVIE_RATING);
    snprintf (rating, sizeof (rating), "%d", atoi (rating) / 2);
    vh_metadata_add_auto (&data->meta_grabber, VALHALLA_METADATA_RATING, str);
  }

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
    grab_nfo_video_stream (video, data);
  }

  c = nfo_tvshow_episode_get_audio_streams_count (episode);
  for (i = 0; i < c; i++)
  {
    nfo_stream_audio_t *audio;

    audio = nfo_tvshow_episode_get_audio_stream (episode, i);
    grab_nfo_audio_stream (audio, data);
  }

  c = nfo_tvshow_episode_get_sub_streams_count (episode);
  for (i = 0; i < c; i++)
  {
    nfo_stream_sub_t *sub;

    sub = nfo_tvshow_episode_get_sub_stream (episode, i);
    grab_nfo_sub_stream (sub, data);
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
grabber_nfo_init (vh_unused void *priv)
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
grabber_nfo_grab (vh_unused void *priv, file_data_t *data)
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
                  "No NFO file available or unsupported type.");
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
