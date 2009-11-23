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
#include <stdio.h>
#include <string.h>

#include "grabber_common.h"
#include "grabber_tmdb.h"
#include "metadata.h"
#include "xml_utils.h"
#include "url_utils.h"
#include "grabber_utils.h"
#include "utils.h"
#include "logs.h"
#include "md5.h"

#define GRABBER_CAP_FLAGS \
  GRABBER_CAP_VIDEO

#define MAX_URL_SIZE      1024

#define TMDB_HOSTNAME     "api.themoviedb.org"

#define TMDB_API_KEY      "5401cd030990fba60e1c23d2832de62e"

#define TMDB_QUERY_SEARCH "http://%s/2.0/Movie.search?title=%s&api_key=%s"
#define TMDB_QUERY_INFO   "http://%s/2.0/Movie.getInfo?id=%s&api_key=%s"

typedef struct grabber_tmdb_s {
  url_t *handler;
} grabber_tmdb_t;


static void
grabber_tmdb_get_picture (file_data_t *fdata, const char *keywords,
                          xmlChar *url, valhalla_dl_t dl)
{
  char name[1024] = { 0 };
  char *type, *cover = NULL;

  if (!fdata || !url)
    return;

  if (dl == VALHALLA_DL_COVER)
    type = VALHALLA_METADATA_COVER;
  else if (dl == VALHALLA_DL_FAN_ART)
    type = VALHALLA_METADATA_FAN_ART;
  else
    return;

  snprintf (name, sizeof (name), "%s-%s", type, keywords);
  cover = vh_md5sum (name);

  vh_metadata_add_auto (&fdata->meta_grabber, type, cover);
  vh_file_dl_add (&fdata->list_downloader, (char *) url, cover, dl);

  free (cover);
}

static int
grabber_tmdb_get (url_t *handler, file_data_t *fdata,
                  const char *keywords, char *escaped_keywords)
{
  char url[MAX_URL_SIZE];
  url_data_t udata;

  xmlDocPtr doc;
  xmlChar *tmp;
  xmlNode *n, *node;

  int res_int = 0;

  if (!keywords || !escaped_keywords)
    return -1;

  /* proceed with TMDB search request */
  memset (url, '\0', MAX_URL_SIZE);
  snprintf (url, MAX_URL_SIZE, TMDB_QUERY_SEARCH,
            TMDB_HOSTNAME, escaped_keywords, TMDB_API_KEY);

  valhalla_log (VALHALLA_MSG_VERBOSE, "Search Request: %s", url);

  udata = vh_url_get_data (handler, url);
  if (udata.status != 0)
    return -1;

  valhalla_log (VALHALLA_MSG_VERBOSE, "Search Reply: %s", udata.buffer);

  /* parse the XML answer */
  doc = vh_get_xml_doc_from_memory (udata.buffer);
  free (udata.buffer);

  if (!doc)
    return -1;

  /* check for total number of results */
  n = xmlDocGetRootElement (doc);
  tmp = vh_get_prop_value_from_xml_tree (n, "totalResults");
  if (!tmp)
  {
    valhalla_log (VALHALLA_MSG_VERBOSE,
                  "Unable to find the item \"%s\"", escaped_keywords);
    goto error;
  }

  /* check that requested item is known on TMDB */
  if (!xmlStrcmp ((unsigned char *) tmp, (unsigned char *) "0"))
  {
    xmlFree (tmp);
    goto error;
  }

  xmlFree (tmp);

  /* get TMDB Movie ID */
  tmp = vh_get_prop_value_from_xml_tree (n, "id");
  if (!tmp)
    goto error;

  xmlFreeDoc (doc);
  doc = NULL;

  /* proceed with TMDB search request */
  memset (url, '\0', MAX_URL_SIZE);
  snprintf (url, MAX_URL_SIZE,
            TMDB_QUERY_INFO, TMDB_HOSTNAME, tmp, TMDB_API_KEY);
  xmlFree (tmp);

  valhalla_log (VALHALLA_MSG_VERBOSE, "Info Request: %s", url);

  udata = vh_url_get_data (handler, url);
  if (udata.status != 0)
    goto error;

  valhalla_log (VALHALLA_MSG_VERBOSE, "Info Reply: %s", udata.buffer);

  /* parse the XML answer */
  doc = vh_get_xml_doc_from_memory (udata.buffer);
  free (udata.buffer);
  if (!doc)
    goto error;

  n = xmlDocGetRootElement (doc);

  /* fetch movie overview description */
  vh_grabber_parse_str (fdata, n,
                        "short_overview", VALHALLA_METADATA_SYNOPSIS);

  /* fetch movie runtime (in minutes) */
  vh_grabber_parse_str (fdata, n, "runtime", VALHALLA_METADATA_RUNTIME);

  /* fetch movie year of production */
  vh_xml_search_int (n, "release", &res_int);
  if (res_int)
  {
    vh_grabber_parse_int (fdata, res_int, VALHALLA_METADATA_YEAR);
    res_int = 0;
  }

  /* fetch movie rating */
  vh_xml_search_int (n, "rating", &res_int);
  if (res_int)
  {
    vh_grabber_parse_int (fdata, res_int / 2, VALHALLA_METADATA_RATING);
    res_int = 0;
  }

  /* fetch movie budget */
  vh_grabber_parse_str (fdata, n, "budget", VALHALLA_METADATA_BUDGET);

  /* fetch movie revenue */
  vh_grabber_parse_str (fdata, n, "revenue", VALHALLA_METADATA_REVENUE);

  /* fetch movie country */
  node = vh_get_node_xml_tree (n, "country");
  if (node)
  {
    tmp = vh_get_prop_value_from_xml_tree (node, "short_name");
    if (tmp)
    {
      vh_metadata_add_auto (&fdata->meta_grabber,
                            VALHALLA_METADATA_COUNTRY, (char *) tmp);
      xmlFree (tmp);
    }
  }

  /* fetch movie categories */
  vh_grabber_parse_categories (fdata, n);

  /* fetch movie people */
  vh_grabber_parse_casting (fdata, n);

  /* fetch movie poster */
  tmp = vh_get_prop_value_from_xml_tree_by_attr (n, "poster", "size", "mid");
  if (tmp)
  {
    grabber_tmdb_get_picture (fdata, keywords, tmp, VALHALLA_DL_COVER);
    xmlFree (tmp);
  }

  /* fetch movie fan art */
  tmp = vh_get_prop_value_from_xml_tree_by_attr (n, "backdrop", "size", "mid");
  if (tmp)
  {
    grabber_tmdb_get_picture (fdata, keywords, tmp, VALHALLA_DL_FAN_ART);
    xmlFree (tmp);
  }

  xmlFreeDoc (doc);
  return 0;

 error:
  if (doc)
    xmlFreeDoc (doc);

  return -1;
}

/****************************************************************************/
/* Private Grabber API                                                      */
/****************************************************************************/

static void *
grabber_tmdb_priv (void)
{
  valhalla_log (VALHALLA_MSG_VERBOSE, __FUNCTION__);

  return calloc (1, sizeof (grabber_tmdb_t));
}

static int
grabber_tmdb_init (void *priv)
{
  grabber_tmdb_t *tmdb = priv;

  valhalla_log (VALHALLA_MSG_VERBOSE, __FUNCTION__);

  if (!tmdb)
    return -1;

  tmdb->handler = vh_url_new ();
  return tmdb->handler ? 0 : -1;
}

static void
grabber_tmdb_uninit (void *priv)
{
  grabber_tmdb_t *tmdb = priv;

  valhalla_log (VALHALLA_MSG_VERBOSE, __FUNCTION__);

  if (!tmdb)
    return;

  vh_url_free (tmdb->handler);
  free (tmdb);
}

static int
grabber_tmdb_grab (void *priv, file_data_t *data)
{
  grabber_tmdb_t *tmdb = priv;
  char *escaped_keywords;
  const char *keywords;
  metadata_t *tag = NULL;
  int res;

  valhalla_log (VALHALLA_MSG_VERBOSE, __FUNCTION__);

  /*
   * Try with the video's title.
   */
  if (!vh_metadata_get (data->meta_parser, "title", 0, &tag))
    keywords = tag->value;
  else
    return -1;

  /* Format the keywords */
  escaped_keywords = vh_url_escape_string (tmdb->handler, keywords);
  if (!escaped_keywords)
    return -2;

  res = grabber_tmdb_get (tmdb->handler, data, keywords, escaped_keywords);
  free (escaped_keywords);

  return res;
}

/****************************************************************************/
/* Public Grabber API                                                       */
/****************************************************************************/

/* vh_grabber_tmdb_register () */
GRABBER_REGISTER (tmdb,
                  GRABBER_CAP_FLAGS,
                  grabber_tmdb_priv,
                  grabber_tmdb_init,
                  grabber_tmdb_uninit,
                  grabber_tmdb_grab,
                  NULL)
