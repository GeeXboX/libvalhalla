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

#include <stdio.h>
#include <string.h>

#include <libavformat/avformat.h>

#include "valhalla.h"
#include "valhalla_internals.h"
#include "logs.h"
#include "lavf_utils.h"

static const struct fileext_s {
  const char *fmtname;
  const char *suffix;
} g_fileext[] = {
  { "ape",                     "apl"  },
  { "ape",                     "mac"  },
  { "daud",                    "302"  },
  { "dv",                      "dif"  },
  { "flic",                    "fli"  },
  { "flic",                    "flc"  },
  { "h264",                    "264"  },
  { "h264",                    "h26l" },
  { "image2",                  "bmp"  },
  { "image2",                  "dpx"  },
  { "image2",                  "gif"  },
  { "image2",                  "im1"  },
  { "image2",                  "im8"  },
  { "image2",                  "im24" },
  { "image2",                  "jp2"  },
  { "image2",                  "jpeg" },
  { "image2",                  "jpg"  },
  { "image2",                  "pcx"  },
  { "image2",                  "pbm"  },
  { "image2",                  "pgm"  },
  { "image2",                  "png"  },
  { "image2",                  "pnm"  },
  { "image2",                  "ppm"  },
  { "image2",                  "ptx"  },
  { "image2",                  "ras"  },
  { "image2",                  "rs"   },
  { "image2",                  "sgi"  },
  { "image2",                  "sun"  },
  { "image2",                  "tga"  },
  { "image2",                  "tif"  },
  { "image2",                  "tiff" },
  { "ingenient",               "cgi"  },
  { "matroska",                "mkv"  },
  { "mjpeg",                   "mjpg" },
  { "mov,mp4,m4a,3gp,3g2,mj2", "3g2"  },
  { "mov,mp4,m4a,3gp,3g2,mj2", "3gp"  },
  { "mov,mp4,m4a,3gp,3g2,mj2", "m4a"  },
  { "mov,mp4,m4a,3gp,3g2,mj2", "mov"  },
  { "mov,mp4,m4a,3gp,3g2,mj2", "mp4"  },
  { "mov,mp4,m4a,3gp,3g2,mj2", "mj2"  },
  { "mpeg1video",              "m1v"  },
  { "mpeg1video",              "mpeg" },
  { "mpeg1video",              "mpg"  },
  { "mp3",                     "m2a"  },
  { "mp3",                     "mp2"  },
  { "nc",                      "v"    },
  { "oma",                     "aa3"  },
  { "rawvideo",                "cif"  },
  { "rawvideo",                "qcif" },
  { "rawvideo",                "rgb"  },
  { "rawvideo",                "yuv"  },
  { "siff",                    "son"  },
  { "siff",                    "vb"   },
  { "truehd",                  "thd"  },
  { "yuv4mpegpipe",            "y4m"  },
  { NULL,                      NULL   },
};

const char *
vh_lavf_utils_fmtname_get (const char *suffix)
{
  const struct fileext_s *it;

  if (!suffix)
    return NULL;

  for (it = g_fileext; it->fmtname; it++)
    if (!strcasecmp (suffix, it->suffix))
      return it->fmtname;

  return suffix;
}

static const char *
suffix_fmt_guess (const char *file)
{
  const char *it;

  if (!file)
    return NULL;

  it = strrchr (file, '.');
  if (it)
    it++;

  return vh_lavf_utils_fmtname_get (it);
}

#define PROBE_BUF_MIN 2048
#define PROBE_BUF_MAX (1 << 20)

/*
 * This function is fully inspired of (libavformat/utils.c v52.28.0
 * "av_open_input_file()") to probe data in order to test if *ftm argument
 * is the right fmt or not.
 *
 * The original function from avformat "av_probe_input_format2()" is really
 * slow because it probes the fmt of _all_ demuxers for each probe_data
 * buffer. Here, the test is only for _one_ fmt and returns the score
 * (if > score_max) provided by fmt->probe().
 *
 * WARNING: this function depends of some internal behaviours of libavformat
 *          and can be "broken" with future versions of FFmpeg.
 */
static int
lavf_utils_probe (AVInputFormat *fmt, const char *file)
{
  FILE *fd;
  int rc = 0;
  int p_size;
  AVProbeData p_data;

  if (!fmt->read_probe)
    return 0;

  p_data.filename = file;
  p_data.buf = NULL;
  p_data.buf_size = 0;

  /* No file should be opened here. */
  if (fmt->flags & AVFMT_NOFILE)
  {
    rc = fmt->read_probe (&p_data);
    goto out;
  }

  fd = fopen (file, "r");
  if (!fd)
    return 0;

  for (p_size = PROBE_BUF_MIN; p_size <= PROBE_BUF_MAX; p_size <<= 1)
  {
    int score;
    int score_max = p_size < PROBE_BUF_MAX ? AVPROBE_SCORE_MAX / 4 : 0;

    p_data.buf = realloc (p_data.buf, p_size + AVPROBE_PADDING_SIZE);
    if (!p_data.buf)
      break;

    p_data.buf_size = fread (p_data.buf, 1, p_size, fd);
    if (p_data.buf_size != p_size) /* EOF is reached? */
      break;

    memset (p_data.buf + p_data.buf_size, 0, AVPROBE_PADDING_SIZE);

    if (fseek (fd, 0, SEEK_SET))
      break;

    score = fmt->read_probe (&p_data);
    if (score > score_max)
    {
      rc = score;
      break;
    }
  }

  fclose (fd);

 out:
  if (p_data.buf)
    free (p_data.buf);
  return rc;
}

AVFormatContext *
vh_lavf_utils_open_input_file (const char *file)
{
  int res;
  const char *name;
  AVFormatContext   *ctx;
  AVFormatParameters ap;
  AVInputFormat     *fmt = NULL;

  ctx = avformat_alloc_context ();
  if (!ctx)
    return NULL;

  ctx->flags |= AVFMT_FLAG_IGNIDX;

  /*
   * Try a format in function of the suffix.
   * We gain a lot of speed if the fmt is already the right.
   */
  name = suffix_fmt_guess (file);
  if (name)
    fmt = av_find_input_format (name);

  if (fmt)
  {
    int score = lavf_utils_probe (fmt, file);
    vh_log (VALHALLA_MSG_VERBOSE,
            "Probe score (%i) [%s] : %s", score, name, file);
    if (!score) /* Bad score? */
      fmt = NULL;
  }

  memset (&ap, 0, sizeof (ap));
  ap.prealloced_context = 1;

  res = av_open_input_file (&ctx, file, fmt, 0, &ap);
  if (res)
  {
    vh_log (VALHALLA_MSG_WARNING,
            "FFmpeg can't open file (%i) : %s", res, file);
    return NULL;
  }

  return ctx;
}
