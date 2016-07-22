/*
 * GeeXboX Valhalla: tiny media scanner API.
 * Copyright (C) 2009 Mathieu Schroeter <mathieu@schroetersa.ch>
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

#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>

#include "grabber_common.h"
#include "grabber_ffmpeg.h"
#include "grabber_utils.h"
#include "lavf_utils.h"
#include "metadata.h"
#include "logs.h"

#define GRABBER_CAP_FLAGS \
  GRABBER_CAP_AUDIO | \
  GRABBER_CAP_VIDEO

typedef struct grabber_ffmpeg_s {
  const metadata_plist_t *pl;
} grabber_ffmpeg_t;

static const metadata_plist_t ffmpeg_pl[] = {
  { NULL,                             VALHALLA_METADATA_PL_HIGHEST  }
};


static const char *
grabber_ffmpeg_codec_name (enum AVCodecID id)
{
  AVCodec *avc = NULL;

  while ((avc = av_codec_next (avc)))
    if (avc->id == id)
      return avc->long_name ? avc->long_name : avc->name;

  return NULL;
}

static int
grabber_ffmpeg_properties_get (grabber_ffmpeg_t *ffmpeg,
                               AVFormatContext *ctx, file_data_t *data)
{
  int res;
  unsigned int i;
  unsigned int audio_streams = 0, video_streams = 0, sub_streams = 0;

  res = avformat_find_stream_info (ctx, NULL);
  if (res < 0)
  {
    vh_log (VALHALLA_MSG_VERBOSE,
            "FFmpeg can't find stream info: %s", data->file.path);
    return -1;
  }

  /*
   * The duration is in microsecond. We save in millisecond in order to
   * have the same unit as libplayer.
   */
  if (data->file.type != VALHALLA_FILE_TYPE_IMAGE && ctx->duration)
    vh_grabber_parse_int64 (data, ROUNDED_DIV (ctx->duration, 1000),
                            VALHALLA_METADATA_DURATION, ffmpeg->pl);

  for (i = 0; i < ctx->nb_streams; i++)
  {
    float value;
    const char *name;
    AVStream *st = ctx->streams[i];
    AVCodecContext *codec = st->codec;

    switch (codec->codec_type)
    {
    case AVMEDIA_TYPE_AUDIO:
      audio_streams++;
      name = grabber_ffmpeg_codec_name (codec->codec_id);
      if (name)
        vh_metadata_add_auto (&data->meta_grabber,
                              VALHALLA_METADATA_AUDIO_CODEC,
                              name, VALHALLA_LANG_UNDEF, ffmpeg->pl);
      vh_grabber_parse_int (data, codec->channels,
                            VALHALLA_METADATA_AUDIO_CHANNELS, ffmpeg->pl);
      if (codec->bit_rate)
        vh_grabber_parse_int (data, codec->bit_rate,
                              VALHALLA_METADATA_AUDIO_BITRATE, ffmpeg->pl);
      break;

    case AVMEDIA_TYPE_VIDEO:
      /* Common part (image + video) */
      video_streams++;
      name = grabber_ffmpeg_codec_name (codec->codec_id);
      if (name)
        vh_metadata_add_auto (&data->meta_grabber,
                              VALHALLA_METADATA_VIDEO_CODEC,
                              name, VALHALLA_LANG_UNDEF, ffmpeg->pl);
      vh_grabber_parse_int (data, codec->width,
                            VALHALLA_METADATA_WIDTH, ffmpeg->pl);
      vh_grabber_parse_int (data, codec->height,
                            VALHALLA_METADATA_HEIGHT, ffmpeg->pl);

      /* Only for video */
      if (data->file.type == VALHALLA_FILE_TYPE_IMAGE)
        break;

      if (codec->bit_rate)
        vh_grabber_parse_int (data, codec->bit_rate,
                              VALHALLA_METADATA_VIDEO_BITRATE, ffmpeg->pl);

      if (st->sample_aspect_ratio.num)
        value = codec->width * st->sample_aspect_ratio.num
                / (float) (codec->height * st->sample_aspect_ratio.den);
      else
        value = codec->width * codec->sample_aspect_ratio.num
                / (float) (codec->height * codec->sample_aspect_ratio.den);
      /*
       * Save in integer with a ratio of 10000 like the constant
       * PLAYER_VIDEO_ASPECT_RATIO_MULT with libplayer (player.h).
       */
      vh_grabber_parse_int (data, (int) (value * 10000.0),
                            VALHALLA_METADATA_VIDEO_ASPECT, ffmpeg->pl);
      break;

    case AVMEDIA_TYPE_SUBTITLE:
      sub_streams++;
      break;

    default:
      break;
    }
  }

  if (audio_streams)
    vh_grabber_parse_int (data, audio_streams,
                          VALHALLA_METADATA_AUDIO_STREAMS, ffmpeg->pl);
  if (video_streams)
    vh_grabber_parse_int (data, video_streams,
                          VALHALLA_METADATA_VIDEO_STREAMS, ffmpeg->pl);
  if (sub_streams)
    vh_grabber_parse_int (data, sub_streams,
                          VALHALLA_METADATA_SUB_STREAMS, ffmpeg->pl);
  return 0;
}

/****************************************************************************/
/* Private Grabber API                                                      */
/****************************************************************************/

static void *
grabber_ffmpeg_priv (void)
{
  vh_log (VALHALLA_MSG_VERBOSE, __FUNCTION__);

  return calloc (1, sizeof (grabber_ffmpeg_t));
}

static int
grabber_ffmpeg_init (void *priv, const grabber_param_t *param)
{
  grabber_ffmpeg_t *ffmpeg = priv;

  vh_log (VALHALLA_MSG_VERBOSE, __FUNCTION__);

  if (!ffmpeg)
    return -1;

  ffmpeg->pl = param->pl;
  return 0;
}

static void
grabber_ffmpeg_uninit (void *priv)
{
  grabber_ffmpeg_t *ffmpeg = priv;

  vh_log (VALHALLA_MSG_VERBOSE, __FUNCTION__);

  if (!ffmpeg)
    return;

  free (ffmpeg);
}

static int
grabber_ffmpeg_grab (void *priv, file_data_t *data)
{
  grabber_ffmpeg_t *ffmpeg = priv;
  int res;
  AVFormatContext *ctx;

  vh_log (VALHALLA_MSG_VERBOSE, __FUNCTION__);

  ctx = vh_lavf_utils_open_input_file (data->file.path);
  if (!ctx)
    return -1;

  res = grabber_ffmpeg_properties_get (ffmpeg, ctx, data);
  /* TODO: res = grabber_ffmpeg_snapshot (ctx, data, pos); */

  avformat_close_input (&ctx);
  return res;
}

/****************************************************************************/
/* Public Grabber API                                                       */
/****************************************************************************/

/* vh_grabber_ffmpeg_register () */
GRABBER_REGISTER (ffmpeg,
                  GRABBER_CAP_FLAGS,
                  ffmpeg_pl,
                  0,
                  grabber_ffmpeg_priv,
                  grabber_ffmpeg_init,
                  grabber_ffmpeg_uninit,
                  grabber_ffmpeg_grab,
                  NULL)
