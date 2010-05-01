/*
 * GeeXboX Valhalla: tiny media scanner API.
 * Copyright (C) 2009 Fabien Brisset <fbrisset@gmail.com>
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
#include <stdio.h>
#include <string.h>

#include "grabber_common.h"
#include "grabber_tvdb.h"
#include "metadata.h"
#include "xml_utils.h"
#include "url_utils.h"
#include "grabber_utils.h"
#include "utils.h"
#include "osdep.h"
#include "logs.h"
#include "md5.h"

#define GRABBER_CAP_FLAGS \
  GRABBER_CAP_VIDEO

/*
 * The documentation is available on:
 *  http://thetvdb.com/wiki/index.php?title=Programmers_API
 *  http://www.thetvdb.com/api/
 */

/*
 * When defined, an API unofficial (undocumented) is used in order to found
 * a name which can be used to return something interesting in the result,
 * else most of time nothing is found with the standard search engine.
 */
#define GRABBER_TVDB_UNOFFICIAL_API

#define MAX_URL_SIZE      1024

#define TVDB_DEFAULT_LANGUAGE   "en"

#define TVDB_HOSTNAME           "thetvdb.com"
#define TVDB_IMAGES_HOSTNAME    "thetvdb.com"

#define TVDB_API_KEY            "29739E1D50A2ACA9"

/* See http://thetvdb.com/wiki/index.php?title=Programmers_API */
#define TVDB_QUERY_SEARCH       "http://%s/api/GetSeries.php?seriesname=%s"
#define TVDB_QUERY_SEARCH_NEW   "http://%s/api/GetSeriesNew.php?seriesname=%s"
#define TVDB_QUERY_INFO         "http://%s/api/%s/series/%s/%s.xml"
#define TVDB_EPISODE_INFO       "http://%s/api/%s/series/%s/default/%u/%u/%s.xml"
#define TVDB_COVERS_URL         "http://%s/banners/%s"

typedef struct grabber_tvdb_s {
  url_t *handler;
  const metadata_plist_t *pl;
} grabber_tvdb_t;

static const metadata_plist_t tvdb_pl[] = {
  { VALHALLA_METADATA_COVER,          VALHALLA_METADATA_PL_ABOVE    },
  { VALHALLA_METADATA_FAN_ART,        VALHALLA_METADATA_PL_ABOVE    },
  { NULL,                             VALHALLA_METADATA_PL_NORMAL   }
};


static void
grabber_tvdb_parse_list (file_data_t *fdata, xmlNode *node, const char *tag,
                         const char *name, const metadata_plist_t *pl)
{
  char *value, *saveptr = NULL;
  xmlChar *tmp = NULL;

  if (!fdata || !node || !tag || !name)
    return;

  tmp = vh_xml_get_prop_value_from_tree (node, tag);
  if (tmp)
  {
    value = strtok_r ((char *) tmp, "|", &saveptr);
    while (value)
    {
      vh_metadata_add_auto (&fdata->meta_grabber, name, value, pl);
      value = strtok_r (NULL, "|", &saveptr);
    }
    xmlFree (tmp);
  }
}

static void
grabber_tvdb_get_picture (file_data_t *fdata, const char *keywords,
                          xmlChar *url, const char *metadata_name,
                          const metadata_plist_t *pl)
{
  char name[1024] = { 0 };
  char complete_url[MAX_URL_SIZE] = { 0 };
  char *cover = NULL;
  valhalla_dl_t dl = VALHALLA_DL_COVER;

  if (!fdata || !url)
    return;

  if (!strcmp (metadata_name, VALHALLA_METADATA_FAN_ART))
    dl = VALHALLA_DL_FAN_ART;

  /* constructing url for download tvdb covers and fan art */
  memset (complete_url, '\0', MAX_URL_SIZE);
  snprintf (complete_url, MAX_URL_SIZE, TVDB_COVERS_URL,
            TVDB_IMAGES_HOSTNAME, (char *) url);

  snprintf (name, sizeof (name), "%s-%s", metadata_name, keywords);
  cover = vh_md5sum (name);

  vh_metadata_add_auto (&fdata->meta_grabber, metadata_name, cover, pl);
  vh_file_dl_add (&fdata->list_downloader, complete_url, cover, dl);
  free (cover);
}

static int
grabber_tvdb_get_episode (grabber_tvdb_t *tvdb, file_data_t *fdata,
                          const char *keywords, const char *seriesid)
{
  char name[1024] = { 0 };
  const metadata_t *tag = NULL;
  int err;
  unsigned int season, episode;
  char url[MAX_URL_SIZE];
  url_data_t udata;
  xmlDocPtr doc;
  xmlChar *tmp = NULL;
  xmlNode *n;

  err = vh_metadata_get (fdata->meta_parser,
                         VALHALLA_METADATA_SEASON, 0, &tag);
  if (err)
    return -1;
  season = atoi (tag->value);

  err = vh_metadata_get (fdata->meta_parser,
                         VALHALLA_METADATA_EPISODE, 0, &tag);
  if (err)
    return -1;
  episode = atoi (tag->value);

  /* proceed with TVDB episode request */
  snprintf (url, MAX_URL_SIZE, TVDB_EPISODE_INFO,
            TVDB_HOSTNAME, TVDB_API_KEY, seriesid, season, episode,
            TVDB_DEFAULT_LANGUAGE);

  vh_log (VALHALLA_MSG_VERBOSE, "Episode Info Request: %s", url);

  udata = vh_url_get_data (tvdb->handler, url);
  if (udata.status != 0)
    goto error;

  vh_log (VALHALLA_MSG_VERBOSE, "Episode Info Reply: %s", udata.buffer);

  /* parse the XML answer */
  doc = vh_xml_get_doc_from_memory (udata.buffer);
  free (udata.buffer);
  if (!doc)
    goto error;

  n = xmlDocGetRootElement (doc);

  /* fetch tv show overview description */
  vh_grabber_parse_str (fdata, n, "Overview",
                        VALHALLA_METADATA_SYNOPSIS_SHOW, tvdb->pl);

  /* fetch tv show first air date */
  vh_grabber_parse_str (fdata, n, "FirstAired",
                        VALHALLA_METADATA_PREMIERED, tvdb->pl);

  /* fetch tv show directors */
  grabber_tvdb_parse_list (fdata, n, "Director",
                           VALHALLA_METADATA_DIRECTOR, tvdb->pl);

  /* fetch tv show guest stars */
  grabber_tvdb_parse_list (fdata, n, "GuestStars",
                           VALHALLA_METADATA_ACTOR, tvdb->pl);

  /* fetch tv show writers */
  grabber_tvdb_parse_list (fdata, n, "Writer",
                           VALHALLA_METADATA_WRITER, tvdb->pl);

  /* fetch tv show poster */
  tmp = vh_xml_get_prop_value_from_tree (n, "filename");
  if (tmp)
  {
    if (*tmp)
    {
    snprintf (name, sizeof (name), "%s-%d-%d", keywords, season, episode);
    grabber_tvdb_get_picture (fdata, name, tmp,
                              VALHALLA_METADATA_COVER_SHOW, tvdb->pl);
    }
    xmlFree (tmp);
  }

  xmlFreeDoc (doc);
  return 0;

 error:
  if (doc)
    xmlFreeDoc (doc);

  return -1;
}

static char *
grabber_tvdb_search (url_t *handler, const char *escaped_keywords,
                     const char *query, const char *item, const char *value)
{
  char url[MAX_URL_SIZE];
  url_data_t udata;
  char *res = NULL;

  xmlDocPtr doc;
  xmlChar *tmp = NULL;
  xmlNode *n;

  /* proceed with TVDB search request */
  memset (url, '\0', MAX_URL_SIZE);
  snprintf (url, MAX_URL_SIZE, query, TVDB_HOSTNAME, escaped_keywords);

  vh_log (VALHALLA_MSG_VERBOSE, "Search Request: %s", url);

  udata = vh_url_get_data (handler, url);
  if (udata.status != 0)
    return NULL;

  vh_log (VALHALLA_MSG_VERBOSE, "Search Reply: %s", udata.buffer);

  /* parse the XML answer */
  doc = vh_xml_get_doc_from_memory (udata.buffer);
  free (udata.buffer);

  if (!doc)
    return NULL;

  /* check for a known DB entry */
  n = vh_xml_get_node_tree (xmlDocGetRootElement (doc), item);
  if (!n)
  {
    vh_log (VALHALLA_MSG_VERBOSE,
            "Unable to find the item \"%s\"", escaped_keywords);
  }
  else
  {
    /* get value */
    tmp = vh_xml_get_prop_value_from_tree (n, value);
    if (tmp)
    {
      res = strdup ((const char *) tmp);
      xmlFree (tmp);
    }
  }

  xmlFreeDoc (doc);
  return res;
}

static int
grabber_tvdb_get (grabber_tvdb_t *tvdb, file_data_t *fdata,
                  char *orig_keywords, char *escaped_keywords)
{
  char url[MAX_URL_SIZE];
  url_data_t udata;
  int res_int = 0;
  char *title, *seriesid, *keywords;
#ifdef GRABBER_TVDB_UNOFFICIAL_API
  char *tmp2;
#endif /* GRABBER_TVDB_UNOFFICIAL_API */

  xmlDocPtr doc = NULL;
  xmlChar *tmp = NULL;
  xmlNode *n;

  if (!escaped_keywords)
    return -1;

#ifdef GRABBER_TVDB_UNOFFICIAL_API
  (void) orig_keywords;

  /* search the exact name for a movie */
  tmp2 = grabber_tvdb_search (tvdb->handler, escaped_keywords,
                              TVDB_QUERY_SEARCH_NEW, "Series", "SeriesName");
  if (!tmp2)
    return -1;

  keywords = strdup (tmp2);
  if (!keywords)
  {
    free (tmp2);
    return -1;
  }
  title = vh_url_escape_string (tvdb->handler, tmp2);
  free (tmp2);
#else
  if (!orig_keywords)
    return -1;

  keywords = strdup (orig_keywords);
  if (!keywords)
    return -1;
  title = strdup (escaped_keywords);
#endif /* GRABBER_TVDB_UNOFFICIAL_API */

  if (!title)
    goto error;

  seriesid = grabber_tvdb_search (tvdb->handler, title,
                                  TVDB_QUERY_SEARCH, "Series", "seriesid");
  free (title);
  if (!seriesid)
    goto error;

  /* proceed with TVDB search request */
  memset (url, '\0', MAX_URL_SIZE);
  snprintf (url, MAX_URL_SIZE, TVDB_QUERY_INFO,
            TVDB_HOSTNAME, TVDB_API_KEY, seriesid, TVDB_DEFAULT_LANGUAGE);

  vh_log (VALHALLA_MSG_VERBOSE, "Info Request: %s", url);

  udata = vh_url_get_data (tvdb->handler, url);
  if (udata.status != 0)
    goto error;

  vh_log (VALHALLA_MSG_VERBOSE, "Info Reply: %s", udata.buffer);

  /* parse the XML answer */
  doc = vh_xml_get_doc_from_memory (udata.buffer);
  free (udata.buffer);
  if (!doc)
    goto error;

  n = xmlDocGetRootElement (doc);

  /* fetch tv show overview description */
  vh_grabber_parse_str (fdata, n, "Overview",
                        VALHALLA_METADATA_SYNOPSIS, tvdb->pl);

  /* fetch tv show first air date */
  vh_xml_search_year (n, "FirstAired", &res_int);
  if (res_int)
  {
    vh_grabber_parse_int (fdata, res_int, VALHALLA_METADATA_YEAR, tvdb->pl);
    res_int = 0;
  }

  /* fetch tv show categories */
  grabber_tvdb_parse_list (fdata, n, "Genre",
                           VALHALLA_METADATA_CATEGORY, tvdb->pl);

  /* fetch tv show actors */
  grabber_tvdb_parse_list (fdata, n, "Actors",
                           VALHALLA_METADATA_ACTOR, tvdb->pl);

  /* fetch tv show runtime (in minutes) */
  vh_grabber_parse_str (fdata, n, "Runtime",
                        VALHALLA_METADATA_RUNTIME, tvdb->pl);

  /* fetch tv show content rating */
  vh_grabber_parse_str (fdata, n, "ContentRating",
                        VALHALLA_METADATA_MPAA, tvdb->pl);

  /* fetch tv show poster */
  tmp = vh_xml_get_prop_value_from_tree (n, "poster");
  if (tmp)
  {
    if (*tmp)
    grabber_tvdb_get_picture (fdata, keywords, tmp,
                              VALHALLA_METADATA_COVER, tvdb->pl);
    xmlFree (tmp);
  }

  /* fetch tv show fan art */
  tmp = vh_xml_get_prop_value_from_tree (n, "fanart");
  if (tmp)
  {
    if (*tmp)
    grabber_tvdb_get_picture (fdata, keywords, tmp,
                              VALHALLA_METADATA_FAN_ART, tvdb->pl);
    xmlFree (tmp);
  }

  grabber_tvdb_get_episode (tvdb, fdata, keywords, seriesid);

  xmlFreeDoc (doc);
  free (keywords);
  free (seriesid);
  return 0;

 error:
  if (doc)
    xmlFreeDoc (doc);
  free (keywords);
  free (seriesid);

  return -1;
}

/****************************************************************************/
/* Private Grabber API                                                      */
/****************************************************************************/

static void *
grabber_tvdb_priv (void)
{
  vh_log (VALHALLA_MSG_VERBOSE, __FUNCTION__);

  return calloc (1, sizeof (grabber_tvdb_t));
}

static int
grabber_tvdb_init (void *priv, const grabber_param_t *param)
{
  grabber_tvdb_t *tvdb = priv;

  vh_log (VALHALLA_MSG_VERBOSE, __FUNCTION__);

  if (!tvdb)
    return -1;

  tvdb->handler = vh_url_new (param->url_ctl);
  tvdb->pl      = param->pl;
  return tvdb->handler ? 0 : -1;
}

static void
grabber_tvdb_uninit (void *priv)
{
  grabber_tvdb_t *tvdb = priv;

  vh_log (VALHALLA_MSG_VERBOSE, __FUNCTION__);

  if (!tvdb)
    return;

  vh_url_free (tvdb->handler);
  free (tvdb);
}

static int
grabber_tvdb_grab (void *priv, file_data_t *data)
{
  grabber_tvdb_t *tvdb = priv;
  const metadata_t *tag = NULL;
  char *keywords;
  int err;

  vh_log (VALHALLA_MSG_VERBOSE, __FUNCTION__);

  err = vh_metadata_get (data->meta_parser, "title", 0, &tag);
  if (err)
    return -1;

  /* Format the keywords */
  keywords = vh_url_escape_string (tvdb->handler, tag->value);
  if (!keywords)
    return -2;

  err = grabber_tvdb_get (tvdb, data, tag->value, keywords);
  free (keywords);

  return err;
}

/****************************************************************************/
/* Public Grabber API                                                       */
/****************************************************************************/

/* vh_grabber_tvdb_register () */
GRABBER_REGISTER (tvdb,
                  GRABBER_CAP_FLAGS,
                  tvdb_pl,
                  0,
                  grabber_tvdb_priv,
                  grabber_tvdb_init,
                  grabber_tvdb_uninit,
                  grabber_tvdb_grab,
                  NULL)
