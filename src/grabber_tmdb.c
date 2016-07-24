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
#include "url_utils.h"
#include "grabber_utils.h"
#include "json_utils.h"
#include "utils.h"
#include "logs.h"
#include "md5.h"

#define GRABBER_CAP_FLAGS \
  GRABBER_CAP_VIDEO

/*
 * The documentation is available on:
 *  http://api.themoviedb.org
 */

#define TMDB_HOSTNAME     "api.themoviedb.org"

#define TMDB_API_KEY      "5401cd030990fba60e1c23d2832de62e"

#define TMDB_QUERY_SEARCH "http://%s/3/search/movie?api_key=%s&query=%s"
#define TMDB_QUERY_INFO   "http://%s/3/movie/%d?api_key=%s"

typedef struct grabber_tmdb_s {
  url_t *handler;
  const metadata_plist_t *pl;
} grabber_tmdb_t;

static const metadata_plist_t tmdb_pl[] = {
  { VALHALLA_METADATA_COVER,          VALHALLA_METADATA_PL_HIGHER   },
  { VALHALLA_METADATA_FAN_ART,        VALHALLA_METADATA_PL_HIGHER   },
  { NULL,                             VALHALLA_METADATA_PL_HIGH     }
};

#if 0
static void
grabber_tmdb_get_picture (file_data_t *fdata, const char *keywords,
                          xmlChar *url, valhalla_dl_t dl,
                          const metadata_plist_t *pl)
{
  char name[1024] = { 0 };
  const char *type;
  char *cover = NULL;

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

  vh_metadata_add_auto (&fdata->meta_grabber,
                        type, cover, VALHALLA_LANG_UNDEF, pl);
  vh_file_dl_add (&fdata->list_downloader, (char *) url, cover, dl);

  free (cover);
}

static xmlChar *
grabber_tmdb_parse_forimage (xmlNode *n, const char *type, const char *size)
{
  xmlNode *node;
  xmlChar *content_type = NULL;
  xmlChar *content_size = NULL;
  xmlChar *content_url  = NULL;

  node = vh_xml_get_node_tree (n, "images");
  if (!node || node->type != XML_ELEMENT_NODE)
    return NULL;

  for (node = node->children; node && !content_url; node = node->next)
  {
      content_type = vh_xml_get_attr_value_from_node (node, "type");
      content_size = vh_xml_get_attr_value_from_node (node, "size");

      if (   !xmlStrcmp (content_type, (const xmlChar *) type)
          && !xmlStrcmp (content_size, (const xmlChar *) size))
        content_url = vh_xml_get_attr_value_from_node (node, "url");

      xmlFree (content_type);
      xmlFree (content_size);
  }

  return content_url;
}
#endif /* 0 */
static int
grabber_tmdb_get (grabber_tmdb_t *tmdb, file_data_t *fdata,
                  const char *keywords, char *escaped_keywords)
{
  char url[MAX_URL_SIZE];
  url_data_t udata;

  json_object *doc;
  char *value_s;
  int value_d;
  int id;

  if (!keywords || !escaped_keywords)
    return -1;

  /* proceed with TMDB search request */
  snprintf (url, sizeof (url), TMDB_QUERY_SEARCH,
            TMDB_HOSTNAME, TMDB_API_KEY, escaped_keywords);

  vh_log (VALHALLA_MSG_VERBOSE, "Search Request: %s", url);

  udata = vh_url_get_data (tmdb->handler, url);
  if (udata.status != 0)
    return -1;

  vh_log (VALHALLA_MSG_VERBOSE, "Search Reply: %s", udata.buffer);

  /* parse the JSON answer */
  doc = json_tokener_parse (udata.buffer);
  free (udata.buffer);

  if (!doc)
    return -1;

  /* check for total number of results */
  if (vh_json_get_int (doc, "total_results") <= 0)
  {
    vh_log (VALHALLA_MSG_VERBOSE,
            "Unable to find the item \"%s\"", escaped_keywords);
    goto error;
  }

  /* get TMDB Movie ID */
  id = vh_json_get_int (doc, "results[0].id");

  /* proceed with TMDB search request */
  snprintf (url, sizeof (url),
            TMDB_QUERY_INFO, TMDB_HOSTNAME, id, TMDB_API_KEY);

  vh_log (VALHALLA_MSG_VERBOSE, "Info Request: %s", url);

  udata = vh_url_get_data (tmdb->handler, url);
  if (udata.status != 0)
    goto error;

  vh_log (VALHALLA_MSG_VERBOSE, "Info Reply: %s", udata.buffer);

  /* parse the JSON answer */
  json_object_put (doc);
  doc = json_tokener_parse (udata.buffer);
  free (udata.buffer);

  if (!doc)
    goto error;

  /* fetch movie overview description */
  value_s = vh_json_get_str (doc, "overview");
  if (value_s)
  {
    vh_metadata_add_auto (&fdata->meta_grabber, VALHALLA_METADATA_SYNOPSIS,
                          value_s, VALHALLA_LANG_EN, tmdb->pl);
    free (value_s);
  }

  /* fetch movie runtime (in minutes) */
  value_d = vh_json_get_int (doc, "runtime");
  if (value_d)
    vh_grabber_parse_int (fdata, value_d,
                          VALHALLA_METADATA_RUNTIME, tmdb->pl);

  /* fetch movie year of production */
  value_s = vh_json_get_str (doc, "release_date");
  if (value_s)
  {
    vh_metadata_add_auto (&fdata->meta_grabber, VALHALLA_METADATA_DATE,
                          value_s, VALHALLA_LANG_EN, tmdb->pl);
    free (value_s);
  }

  /* fetch movie rating */
  value_s = vh_json_get_str (doc, "vote_average");
  if (value_s)
  {
    vh_metadata_add_auto (&fdata->meta_grabber, VALHALLA_METADATA_RATING,
                          value_s, VALHALLA_LANG_EN, tmdb->pl);
    free (value_s);
  }

  /* fetch movie budget */
  value_d = vh_json_get_int (doc, "budget");
  if (value_d)
    vh_grabber_parse_int (fdata, value_d,
                          VALHALLA_METADATA_BUDGET, tmdb->pl);

  /* fetch movie revenue */
  value_d = vh_json_get_int (doc, "revenue");
  if (value_d)
    vh_grabber_parse_int (fdata, value_d,
                          VALHALLA_METADATA_REVENUE, tmdb->pl);
#if 0
  /* fetch movie country
   * <country name="..."/>
   */
  vh_grabber_parse_countries (fdata, n, VALHALLA_LANG_EN, tmdb->pl);

  /* fetch movie categories
   * <category name="..."/>
   */
  vh_grabber_parse_categories (fdata, n, VALHALLA_LANG_EN, tmdb->pl);

  /* fetch movie people
   * <person name="..." character="..." job="..."/>
   */
  vh_grabber_parse_casting (fdata, n, tmdb->pl);

  /* Fetch movie poster
   * <image type="poster" url="..." size="mid"/>
   */
  tmp = grabber_tmdb_parse_forimage (n, "poster", "mid");
  if (tmp)
  {
    grabber_tmdb_get_picture (fdata, keywords, tmp,
                              VALHALLA_DL_COVER, tmdb->pl);
    xmlFree (tmp);
  }

  /* Fetch movie fan art
   * <image type="backdrop" url="..." size="w1280"/>
   */
  tmp = grabber_tmdb_parse_forimage (n, "backdrop", "w1280");
  if (tmp)
  {
    grabber_tmdb_get_picture (fdata, keywords, tmp,
                              VALHALLA_DL_FAN_ART, tmdb->pl);
    xmlFree (tmp);
  }

  xmlFreeDoc (doc);
#endif /* 0 */
  json_object_put (doc);
  return 0;

 error:
  if (doc)
    json_object_put (doc);
  return -1;
}

/****************************************************************************/
/* Private Grabber API                                                      */
/****************************************************************************/

static void *
grabber_tmdb_priv (void)
{
  vh_log (VALHALLA_MSG_VERBOSE, __FUNCTION__);

  return calloc (1, sizeof (grabber_tmdb_t));
}

static int
grabber_tmdb_init (void *priv, const grabber_param_t *param)
{
  grabber_tmdb_t *tmdb = priv;

  vh_log (VALHALLA_MSG_VERBOSE, __FUNCTION__);

  if (!tmdb)
    return -1;

  tmdb->handler = vh_url_new (param->url_ctl);
  tmdb->pl      = param->pl;
  return tmdb->handler ? 0 : -1;
}

static void
grabber_tmdb_uninit (void *priv)
{
  grabber_tmdb_t *tmdb = priv;

  vh_log (VALHALLA_MSG_VERBOSE, __FUNCTION__);

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
  const metadata_t *tag = NULL;
  int res;

  vh_log (VALHALLA_MSG_VERBOSE, __FUNCTION__);

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

  res = grabber_tmdb_get (tmdb, data, keywords, escaped_keywords);
  free (escaped_keywords);

  return res;
}

/****************************************************************************/
/* Public Grabber API                                                       */
/****************************************************************************/

/* vh_grabber_tmdb_register () */
GRABBER_REGISTER (tmdb,
                  GRABBER_CAP_FLAGS,
                  tmdb_pl,
                  0,
                  grabber_tmdb_priv,
                  grabber_tmdb_init,
                  grabber_tmdb_uninit,
                  grabber_tmdb_grab,
                  NULL)
