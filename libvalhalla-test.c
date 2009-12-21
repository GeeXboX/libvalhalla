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
#include <unistd.h>
#include <getopt.h>
#include <time.h>

#include "valhalla.h"
#include "utils.h"    /* for VH_TIMERSUB and vh_clock_gettime */

#define APPNAME "libvalhalla-test"

#define TESTVALHALLA_HELP \
  APPNAME " for libvalhalla-" LIBVALHALLA_VERSION_STR "\n" \
  "\n" \
  "Usage: " APPNAME " [options ...] paths ...\n" \
  "\n" \
  "Options:\n" \
  " -h --help               this help\n" \
  " -v --verbose            increase verbosity\n" \
  " -l --loop               number of loops\n" \
  " -t --timewait           time to wait between loops [sec]\n" \
  " -m --timelimit          time limit [ms] for the scanning\n" \
  " -a --priority           priority for the threads\n" \
  " -d --database           path for the database\n" \
  " -f --download           path for the downloader destination\n" \
  " -c --commit-int         commits interval\n" \
  " -p --parser             number of parsers\n" \
  " -n --decrap             enable decrapifier (for title metadata)\n" \
  " -k --keyword            keyword for the decrapifier\n" \
  " -s --suffix             file suffix (extension)\n" \
  " -g --grabber            grabber to be used\n" \
  " -i --no-grabber         disable all grabbers\n" \
  " -j --stats              dump all the statistics\n" \
  " -q --metadata-cb        enable the metadata callback\n" \
  "\n" \
  "Example:\n" \
  " $ " APPNAME " -l 2 -t 5 -d ./mydb.db -p 1 -a 15 -s ogg -s mp3 /home/foobar/music\n" \
  "\n" \
  "Default values are loop=1, timewait=0, database=./valhalla.db,\n" \
  "                   commit-int=128, parser=2, priority=19,\n" \
  "                   suffix=flac,m4a,mp3,ogg,wav,wma\n" \
  "                          avi,mkv,mov,mpg,wmv\n" \
  "                          bmp,gif,jpeg,jpg,png,tga,tif,tiff\n" \
  "\n"

#define SUFFIX_MAX 16
#define KEYWORD_MAX 16
#define GRABBER_MAX 16

static void
eventgl_cb (valhalla_event_gl_t e, void *data)
{
  (void) data;
  printf ("Global event: %u\n", e);
}

static void
eventmd_cb (valhalla_event_md_t e, const char *id,
            const valhalla_file_t *file,
            const valhalla_metadata_t *md, void *data)
{
  (void) data;
  printf ("Metadata event\n");
  printf ("  File     : %s (%lu) (%u)\n", file->path, file->mtime, file->type);
  switch (e)
  {
  case VALHALLA_EVENTMD_PARSER:
    printf ("  Parser\n");
    break;
  case VALHALLA_EVENTMD_GRABBER:
    printf ("  Grabber  : %s\n", id);
    break;
  }
  printf ("  Name     : %s\n  Value    : %s\n  Group    : %u\n",
          md->name, md->value, md->group);
}

int
main (int argc, char **argv)
{
  int rc, i;
  int loop_nb = 1, loop_wait = 0, time_limit = 0;
  int priority = 0, commit = 128, decrap = 0;
  valhalla_t *handle;
  valhalla_init_param_t param;
  valhalla_verb_t verbosity = VALHALLA_MSG_WARNING;
  const char *database = "./valhalla.db";
  const char *download = NULL;
#ifdef USE_GRABBER
  const char *grabber = NULL;
#endif /* USE_GRABBER */
  int parser_nb = 2, sid = 0, kid = 0, gid = 0;
  const char *suffix[SUFFIX_MAX];
  const char *keyword[KEYWORD_MAX];
  const char *grabbers[GRABBER_MAX];
  int nograbber = 0, stats = 0, metadata_cb = 0;
  struct timespec tss, tse, tsd;
  const char *group = NULL;

  int c, index;
  const char *const short_options = "hvl:t:m:a:d:f:c:p:nk:s:g:ijq";
  const struct option long_options[] = {
    { "help",        no_argument,       0, 'h'  },
    { "verbose",     no_argument,       0, 'v'  },
    { "loop",        required_argument, 0, 'l'  },
    { "timewait",    required_argument, 0, 't'  },
    { "timelimit",   required_argument, 0, 'm'  },
    { "priority",    required_argument, 0, 'a'  },
    { "database",    required_argument, 0, 'd'  },
    { "download",    required_argument, 0, 'f'  },
    { "commit-int",  required_argument, 0, 'c'  },
    { "parser",      required_argument, 0, 'p'  },
    { "decrap",      no_argument,       0, 'n'  },
    { "keyword",     required_argument, 0, 'k'  },
    { "suffix",      required_argument, 0, 's'  },
    { "grabber",     required_argument, 0, 'g'  },
    { "no-grabber",  no_argument,       0, 'i'  },
    { "stats",       no_argument,       0, 'j'  },
    { "metadata-cb", no_argument,       0, 'q'  },
    { NULL,          0,                 0, '\0' },
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

    case 'm':
      time_limit = atoi (optarg);
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
    case 'n':
      decrap = 1;
      break;

    case 'g':
      if (gid < GRABBER_MAX)
        grabbers[gid++] = optarg;
      break;

    case 'i':
      nograbber = 1;
      break;

    case 'j':
      stats = 1;
      break;

    case 'q':
      metadata_cb = 1;
      break;

    default:
      printf (TESTVALHALLA_HELP);
      return -1;
    }
  }

  valhalla_verbosity (verbosity);

  memset (&param, 0, sizeof (param));
  param.parser_nb   = parser_nb;
  param.commit_int  = commit;
  param.decrapifier = decrap;
  param.gl_cb       = eventgl_cb;
  param.md_cb       = metadata_cb ? eventmd_cb : NULL;

  handle = valhalla_init (database, &param);
  if (!handle)
    return -1;

  for (i = 0; i < sid; i++)
  {
    valhalla_config_set (handle, SCANNER_SUFFIX, suffix[i]);
    printf ("Add suffix: %s\n", suffix[i]);
  }

  if (!sid)
  {
    /* audio */
    valhalla_config_set (handle, SCANNER_SUFFIX, "flac");
    valhalla_config_set (handle, SCANNER_SUFFIX, "m4a");
    valhalla_config_set (handle, SCANNER_SUFFIX, "mp3");
    valhalla_config_set (handle, SCANNER_SUFFIX, "ogg");
    valhalla_config_set (handle, SCANNER_SUFFIX, "wav");
    valhalla_config_set (handle, SCANNER_SUFFIX, "wma");

    /* video */
    valhalla_config_set (handle, SCANNER_SUFFIX, "avi");
    valhalla_config_set (handle, SCANNER_SUFFIX, "mkv");
    valhalla_config_set (handle, SCANNER_SUFFIX, "mov");
    valhalla_config_set (handle, SCANNER_SUFFIX, "mpg");
    valhalla_config_set (handle, SCANNER_SUFFIX, "wmv");

    /* image */
    valhalla_config_set (handle, SCANNER_SUFFIX, "bmp");
    valhalla_config_set (handle, SCANNER_SUFFIX, "gif");
    valhalla_config_set (handle, SCANNER_SUFFIX, "jpeg");
    valhalla_config_set (handle, SCANNER_SUFFIX, "jpg");
    valhalla_config_set (handle, SCANNER_SUFFIX, "png");
    valhalla_config_set (handle, SCANNER_SUFFIX, "tga");
    valhalla_config_set (handle, SCANNER_SUFFIX, "tif");
    valhalla_config_set (handle, SCANNER_SUFFIX, "tiff");

    printf ("Default suffixes: flac,m4a,mp3,ogg,wav,wma\n"
            "                  avi,mkv,mov,mpg,wmv\n"
            "                  bmp,gif,jpeg,jpg,png,tga,tif,tiff\n");
  }

  for (i = 0; i < kid; i++)
  {
    valhalla_config_set (handle, PARSER_KEYWORD, keyword[i]);
    printf ("Add keyword in the blacklist: %s\n", keyword[i]);
  }

  if (download)
  {
    printf ("Destination directory for downloaded files: %s\n", download);
    valhalla_config_set (handle,
                         DOWNLOADER_DEST, download, VALHALLA_DL_DEFAULT);
  }

  while (optind < argc)
  {
    valhalla_config_set (handle, SCANNER_PATH, argv[optind], 1);
    printf ("Add path: %s\n", argv[optind++]);
  }

  printf ("Run: parser=%i loop=%i wait=%i priority=%i commit-int=%i\n",
          parser_nb, loop_nb, loop_wait, priority, commit);

#ifdef USE_GRABBER
  printf ("Grabbers available:\n");
  while ((grabber = valhalla_grabber_next (handle, grabber)))
  {
    if (nograbber)
      valhalla_config_set (handle, GRABBER_STATE, grabber, 0);
    else if (gid == 0) /* no grabber has been specified */
      printf ("  %s\n", grabber);
    else
    {
      valhalla_config_set (handle, GRABBER_STATE, grabber, 0);
      for (i = 0; i < gid; i++)
      {
        if (!strcmp (grabber, grabbers[i]))
        {
          valhalla_config_set (handle, GRABBER_STATE, grabber, 1);
          printf ("  %s\n", grabber);
          break;
        }
      }
    }
  }
#endif /* USE_GRABBER */

  vh_clock_gettime (CLOCK_REALTIME, &tss);

  rc = valhalla_run (handle, loop_nb, loop_wait, priority);
  if (rc)
  {
    fprintf (stderr, "Error code: %i\n", rc);
    valhalla_uninit (handle);
    return -1;
  }

  if (!time_limit)
    valhalla_wait (handle);
  else
    usleep (time_limit * 1000);

  vh_clock_gettime (CLOCK_REALTIME, &tse);

  /* statistics */
  printf ("Statistics dump: %s\n", !stats ? "(ignored)" : "");
  while (stats && (group = valhalla_stats_group_next (handle, group)))
  {
    unsigned long val;
    const char *item = NULL;

    printf (" %s\n", group);
    printf (" - counters\n");
    do
    {
      val =
        valhalla_stats_read_next (handle, group, VALHALLA_STATS_COUNTER, &item);
      if (item)
        printf ("   - %-20s %lu\n", item, val);
    }
    while (item);

    item = NULL;
    printf (" - timers\n");
    do
    {
      val =
        valhalla_stats_read_next (handle, group, VALHALLA_STATS_TIMER, &item);
      if (item)
        printf ("   - %-20s %lu\n", item, val);
    }
    while (item);
  }

  valhalla_uninit (handle);

  VH_TIMERSUB (&tse, &tss, &tsd);
  printf ("Time : %f sec\n", tsd.tv_sec + tsd.tv_nsec / 1000000000.0);

  return 0;
}
