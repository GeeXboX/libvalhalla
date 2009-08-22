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

#define _GNU_SOURCE
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <getopt.h>
#include <sys/time.h>

#include "valhalla.h"


#define TESTVALHALLA_HELP \
  "test-valhalla for libvalhalla-" LIBVALHALLA_VERSION_STR "\n" \
  "\n" \
  "Usage: test-valhalla [options ...] paths ...\n" \
  "\n" \
  "Options:\n" \
  " -h --help               this help\n" \
  " -v --verbose            increase verbosity\n" \
  " -l --loop               number of loops\n" \
  " -t --timewait           time to wait between loops [sec]\n" \
  " -a --priority           priority for the threads\n" \
  " -d --database           path for the database\n" \
  " -f --download           path for the downloader destination\n" \
  " -c --commit-int         commits interval\n" \
  " -p --parser             number of parsers\n" \
  " -k --keyword            keyword for the decrapifier\n" \
  " -s --suffix             file suffix (extension)\n" \
  "\n" \
  "Example:\n" \
  " $ test-valhalla -l 2 -t 5 -d ./mydb.db -p 1 -a 15 -s ogg -s mp3 /home/foobar/music\n" \
  "\n" \
  "Default values are loop=1, timewait=0, database=./valhalla.db,\n" \
  "                   commit-int=128, parser=2, priority=19,\n" \
  "                   suffix=ogg,mp3,m4a,flac,wav,wma\n" \
  "                          avi,mkv,mpg,wmv,mov\n" \
  "                          bmp,gif,jpeg,jpg,png,tga,tif,tiff\n" \
  "\n"

#define SUFFIX_MAX 16
#define KEYWORD_MAX 16

int
main (int argc, char **argv)
{
  int rc, i;
  int loop_nb = 1, loop_wait = 0;
  int priority = 19, commit = 128;
  valhalla_t *handle;
  valhalla_verb_t verbosity = VALHALLA_MSG_WARNING;
  const char *database = "./valhalla.db";
  const char *download = NULL;
#ifdef USE_GRABBER
  const char *grabber = NULL;
#endif /* USE_GRABBER */
  int parser_nb = 2, sid = 0, kid = 0;
  const char *suffix[SUFFIX_MAX];
  const char *keyword[KEYWORD_MAX];
  struct timeval tvs, tve;
  long long diff;

  int c, index;
  const char *const short_options = "hvl:t:a:d:f:c:p:k:s:";
  const struct option long_options[] = {
    { "help",       no_argument,       0, 'h'  },
    { "verbose",    no_argument,       0, 'v'  },
    { "loop",       required_argument, 0, 'l'  },
    { "timewait",   required_argument, 0, 't'  },
    { "priority",   required_argument, 0, 'a'  },
    { "database",   required_argument, 0, 'd'  },
    { "download",   required_argument, 0, 'f'  },
    { "commit-int", required_argument, 0, 'c'  },
    { "parser",     required_argument, 0, 'p'  },
    { "keyword",    required_argument, 0, 'k'  },
    { "suffix",     required_argument, 0, 's'  },
    { NULL,         0,                 0, '\0' },
  };

  for (;;)
  {
    c = getopt_long (argc, argv, short_options, long_options, &index);

    if (c == EOF)
      break;

    switch (c)
    {
    case 0:
      break;

    case '?':
    case 'h':
      printf (TESTVALHALLA_HELP);
      return 0;

    case 'v':
      if (verbosity == VALHALLA_MSG_WARNING)
        verbosity = VALHALLA_MSG_INFO;
      else
        verbosity = VALHALLA_MSG_VERBOSE;
      break;

    case 'l':
      loop_nb = atoi (optarg);
      break;

    case 't':
      loop_wait = atoi (optarg);
      break;

    case 'a':
      priority = atoi (optarg);
      break;

    case 'd':
      database = optarg;
      break;

    case 'f':
      download = optarg;
      break;

    case 'c':
      commit = atoi (optarg);
      break;

    case 'p':
      parser_nb = atoi (optarg);
      break;

    case 's':
      if (sid < SUFFIX_MAX)
        suffix[sid++] = optarg;
      break;

    case 'k':
      if (kid < KEYWORD_MAX)
        keyword[kid++] = optarg;
      break;

    default:
      printf (TESTVALHALLA_HELP);
      return -1;
    }
  }

  valhalla_verbosity (verbosity);

  handle = valhalla_init (database, parser_nb, kid, commit);
  if (!handle)
    return -1;

  for (i = 0; i < sid; i++)
  {
    valhalla_suffix_add (handle, suffix[i]);
    printf ("Add suffix: %s\n", suffix[i]);
  }

  if (!sid)
  {
    /* audio */
    valhalla_suffix_add (handle, "ogg");
    valhalla_suffix_add (handle, "mp3");
    valhalla_suffix_add (handle, "m4a");
    valhalla_suffix_add (handle, "flac");
    valhalla_suffix_add (handle, "wav");
    valhalla_suffix_add (handle, "wma");

    /* video */
    valhalla_suffix_add (handle, "avi");
    valhalla_suffix_add (handle, "mkv");
    valhalla_suffix_add (handle, "mpg");
    valhalla_suffix_add (handle, "wmv");
    valhalla_suffix_add (handle, "mov");

    /* image */
    valhalla_suffix_add (handle, "bmp");
    valhalla_suffix_add (handle, "gif");
    valhalla_suffix_add (handle, "jpeg");
    valhalla_suffix_add (handle, "jpg");
    valhalla_suffix_add (handle, "png");
    valhalla_suffix_add (handle, "tga");
    valhalla_suffix_add (handle, "tif");
    valhalla_suffix_add (handle, "tiff");

    printf ("Default suffixes: ogg,mp3,m4a,flac,wav,wma\n"
            "                  avi,mkv,mpg,wmv,mov\n"
            "                  bmp,gif,jpeg,jpg,png,tga,tif,tiff\n");
  }

  for (i = 0; i < kid; i++)
  {
    valhalla_bl_keyword_add (handle, keyword[i]);
    printf ("Add keyword in the blacklist: %s\n", keyword[i]);
  }

  if (download)
  {
    printf ("Destination directory for downloaded files: %s\n", download);
    valhalla_download_dest_set (handle, VALHALLA_DL_DEFAULT, download);
  }

  while (optind < argc)
  {
    valhalla_path_add (handle, argv[optind], 1);
    printf ("Add path: %s\n", argv[optind++]);
  }

  printf ("Run: parser=%i loop=%i wait=%i priority=%i commit-int=%i\n",
          parser_nb, loop_nb, loop_wait, priority, commit);

#ifdef USE_GRABBER
  printf ("Grabbers available:\n");
  while ((grabber = valhalla_grabber_list_get (handle, grabber)))
    printf ("  %s\n", grabber);
#endif /* USE_GRABBER */

  gettimeofday (&tvs, NULL);

  rc = valhalla_run (handle, loop_nb, loop_wait, priority);
  if (rc)
  {
    fprintf (stderr, "Error code: %i\n", rc);
    valhalla_uninit (handle);
    return -1;
  }

  valhalla_wait (handle);

  gettimeofday (&tve, NULL);

  valhalla_uninit (handle);

  diff = (tve.tv_sec - tvs.tv_sec) * 1000000L + tve.tv_usec - tvs.tv_usec;
  printf ("Time elapsing: %f msec, %f sec\n", diff / 1000.0, diff / 1000000.0);

  return 0;
}
