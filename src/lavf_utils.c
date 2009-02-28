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

#include <string.h>

#include "valhalla_internals.h"
#include "lavf_utils.h"

static const struct fileext_s {
  const char *fmtname;
  const char *suffix;
} g_fileext[] = {
  { "flic",                    "fli"  },
  { "flic",                    "flc"  },
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
  { "yuv4mpegpipe",            "y4m"  },
  { NULL,                      NULL   },
};

static const char *const g_id3v1_genre[] = {
  [0]   = "Blues",
  [1]   = "Classic Rock",
  [2]   = "Country",
  [3]   = "Dance",
  [4]   = "Disco",
  [5]   = "Funk",
  [6]   = "Grunge",
  [7]   = "Hip-Hop",
  [8]   = "Jazz",
  [9]   = "Metal",
  [10]  = "New Age",
  [11]  = "Oldies",
  [12]  = "Other",
  [13]  = "Pop",
  [14]  = "R&B",
  [15]  = "Rap",
  [16]  = "Reggae",
  [17]  = "Rock",
  [18]  = "Techno",
  [19]  = "Industrial",
  [20]  = "Alternative",
  [21]  = "Ska",
  [22]  = "Death Metal",
  [23]  = "Pranks",
  [24]  = "Soundtrack",
  [25]  = "Euro-Techno",
  [26]  = "Ambient",
  [27]  = "Trip-Hop",
  [28]  = "Vocal",
  [29]  = "Jazz+Funk",
  [30]  = "Fusion",
  [31]  = "Trance",
  [32]  = "Classical",
  [33]  = "Instrumental",
  [34]  = "Acid",
  [35]  = "House",
  [36]  = "Game",
  [37]  = "Sound Clip",
  [38]  = "Gospel",
  [39]  = "Noise",
  [40]  = "AlternRock",
  [41]  = "Bass",
  [42]  = "Soul",
  [43]  = "Punk",
  [44]  = "Space",
  [45]  = "Meditative",
  [46]  = "Instrumental Pop",
  [47]  = "Instrumental Rock",
  [48]  = "Ethnic",
  [49]  = "Gothic",
  [50]  = "Darkwave",
  [51]  = "Techno-Industrial",
  [52]  = "Electronic",
  [53]  = "Pop-Folk",
  [54]  = "Eurodance",
  [55]  = "Dream",
  [56]  = "Southern Rock",
  [57]  = "Comedy",
  [58]  = "Cult",
  [59]  = "Gangsta",
  [60]  = "Top 40",
  [61]  = "Christian Rap",
  [62]  = "Pop/Funk",
  [63]  = "Jungle",
  [64]  = "Native American",
  [65]  = "Cabaret",
  [66]  = "New Wave",
  [67]  = "Psychadelic",
  [68]  = "Rave",
  [69]  = "Showtunes",
  [70]  = "Trailer",
  [71]  = "Lo-Fi",
  [72]  = "Tribal",
  [73]  = "Acid Punk",
  [74]  = "Acid Jazz",
  [75]  = "Polka",
  [76]  = "Retro",
  [77]  = "Musical",
  [78]  = "Rock & Roll",
  [79]  = "Hard Rock",
  [80]  = "Folk",
  [81]  = "Folk-Rock",
  [82]  = "National Folk",
  [83]  = "Swing",
  [84]  = "Fast Fusion",
  [85]  = "Bebob",
  [86]  = "Latin",
  [87]  = "Revival",
  [88]  = "Celtic",
  [89]  = "Bluegrass",
  [90]  = "Avantgarde",
  [91]  = "Gothic Rock",
  [92]  = "Progressive Rock",
  [93]  = "Psychedelic Rock",
  [94]  = "Symphonic Rock",
  [95]  = "Slow Rock",
  [96]  = "Big Band",
  [97]  = "Chorus",
  [98]  = "Easy Listening",
  [99]  = "Acoustic",
  [100] = "Humour",
  [101] = "Speech",
  [102] = "Chanson",
  [103] = "Opera",
  [104] = "Chamber Music",
  [105] = "Sonata",
  [106] = "Symphony",
  [107] = "Booty Bass",
  [108] = "Primus",
  [109] = "Porn Groove",
  [110] = "Satire",
  [111] = "Slow Jam",
  [112] = "Club",
  [113] = "Tango",
  [114] = "Samba",
  [115] = "Folklore",
  [116] = "Ballad",
  [117] = "Power Ballad",
  [118] = "Rhythmic Soul",
  [119] = "Freestyle",
  [120] = "Duet",
  [121] = "Punk Rock",
  [122] = "Drum Solo",
  [123] = "A capella",
  [124] = "Euro-House",
  [125] = "Dance Hall",
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

const char *
lavf_utils_id3v1_genre (unsigned int id)
{
  if (id >= ARRAY_NB_ELEMENTS (g_id3v1_genre))
    return NULL;

  return g_id3v1_genre[id];
}
