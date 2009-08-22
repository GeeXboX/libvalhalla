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

#include <string.h>

#include "valhalla_internals.h"
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
lavf_utils_fmtname_get (const char *suffix)
{
  const struct fileext_s *it;

  if (!suffix)
    return NULL;

  for (it = g_fileext; it->fmtname; it++)
    if (!strcasecmp (suffix, it->suffix))
      return it->fmtname;

  return suffix;
}
