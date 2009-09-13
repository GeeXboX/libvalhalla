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
#include "logs.h"
#include "md5.h"

#define GRABBER_CAP_FLAGS \
  GRABBER_CAP_VIDEO

#define MAX_URL_SIZE      1024

#define TVDB_DEFAULT_LANGUAGE   "en"

#define TVDB_HOSTNAME           "thetvdb.com"
#define TVDB_IMAGES_HOSTNAME    "images.thetvdb.com"

#define TVDB_API_KEY            "29739E1D50A2ACA9"

/* See http://thetvdb.com/wiki/index.php?title=Programmers_API */
#define TVDB_QUERY_SEARCH       "http://%s/api/GetSeries.php?seriesname=%s"
#define TVDB_QUERY_INFO         "http://%s/api/%s/series/%s/%s.xml"
#define TVDB_COVERS_URL         "http://%s/banners/%s"

typedef struct grabber_tvdb_s {
  url_t *handler;
} grabber_tvdb_t;

static void
grabber_tvdb_parse_genre (file_data_t *fdata, xmlChar *genre)
{
  char *category, *saveptr = NULL;

  if (!fdata || !genre)
    return;

  category = strtok_r ((char *) genre, "|", &saveptr);
  while (category)
  {
    vh_metadata_add (&fdata->meta_grabber, "category",
                     category, VALHALLA_META_GRP_CLASSIFICATION);
    category = strtok_r (NULL, "|", &saveptr);
  }
}

static void
grabber_tvdb_get_picture (file_data_t *fdata, const char *keywords,
                          xmlChar *url, valhalla_dl_t dl)
{
  char name[1024] = { 0 };
  char complete_url[MAX_URL_SIZE] = { 0 };
  char *type, *cover = NULL;

  if (!fdata || !url)
    return;

  if (dl == VALHALLA_DL_COVER)
    type = "cover";
  else if (dl == VALHALLA_DL_BACKDROP)
    type = "backdrop";
  else
    return;

  /* constructing url for download tvdb covers and backdrop */
  memset (complete_url, '\0', MAX_URL_SIZE);
  snprintf (complete_url, MAX_URL_SIZE, TVDB_COVERS_URL,
            TVDB_IMAGES_HOSTNAME, (char *) url);

  snprintf (name, sizeof (name), "%s-%s", type, keywords);
  cover = vh_md5sum (name);

  vh_metadata_add (&fdata->meta_grabber, type,
                   cover, VALHALLA_META_GRP_MISCELLANEOUS);
  vh_file_dl_add (&fdata->list_downloader, complete_url, cover, dl);
}

static int
grabber_tvdb_get (url_t *handler, file_data_t *fdata,
                  const char *keywords, char *escaped_keywords)
{
  char url[MAX_URL_SIZE];
  url_data_t udata;
  int res_int;

  xmlDocPtr doc;
  xmlChar *tmp = NULL;
  xmlNode *n;

  if (!keywords || !escaped_keywords)
    return -1;

  /* proceed with TVDB search request */
  memset (url, '\0', MAX_URL_SIZE);
  snprintf (url, MAX_URL_SIZE, TVDB_QUERY_SEARCH,
            TVDB_HOSTNAME, escaped_keywords);

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

  /* check for a known DB entry */
  n = vh_get_node_xml_tree (xmlDocGetRootElement (doc), "Series");
  if (!n)
  {
    valhalla_log (VALHALLA_MSG_VERBOSE,
                  "Unable to find the item \"%s\"", escaped_keywords);
    goto error;
  }

  /* get TVDB show id */
  tmp = vh_get_prop_value_from_xml_tree (n, "seriesid");
  if (!tmp)
    goto error;

  xmlFreeDoc (doc);
  doc = NULL;

  /* proceed with TVDB search request */
  memset (url, '\0', MAX_URL_SIZE);
  snprintf (url, MAX_URL_SIZE, TVDB_QUERY_INFO,
            TVDB_HOSTNAME, TVDB_API_KEY, tmp, TVDB_DEFAULT_LANGUAGE);
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

  /* fetch tv show overview description */
  vh_grabber_parse_str (fdata, n, "Overview",
                        "synopsis", VALHALLA_META_GRP_CLASSIFICATION);

  /* fetch tv show first air date */
  vh_xml_search_year (n, "FirstAired", &res_int);
  if (res_int)
  {
    vh_grabber_parse_int (fdata, res_int, "year", VALHALLA_META_GRP_TEMPORAL);
    res_int = 0;
  }

  /* fetch tv show categories */
  tmp = vh_get_prop_value_from_xml_tree (n, "Genre");
  if (tmp)
  {
    grabber_tvdb_parse_genre (fdata, tmp);
    xmlFree(tmp);
  }

  /* fetch tv show runtime (in minutes) */
  vh_grabber_parse_str (fdata, n, "Runtime",
                        "runtime", VALHALLA_META_GRP_CLASSIFICATION);

  /* fetch tv show poster */
  tmp = vh_get_prop_value_from_xml_tree (n, "poster");
  if (tmp)
  {
    grabber_tvdb_get_picture (fdata, keywords, tmp, VALHALLA_DL_COVER);
    xmlFree (tmp);
  }

  /* fetch tv show backdrop */
  tmp = vh_get_prop_value_from_xml_tree (n, "fanart");
  if (tmp)
  {
    grabber_tvdb_get_picture (fdata, keywords, tmp, VALHALLA_DL_BACKDROP);
    xmlFree (tmp);
  }

  xmlFreeDoc (doc);
  return 0;

 error:
  if (tmp)
    xmlFree (tmp);
  if (doc)
    xmlFreeDoc (doc);

  return -1;
}

/****************************************************************************/
/* Private Grabber API                                                      */
/****************************************************************************/

static void *
grabber_tvdb_priv (void)
{
  valhalla_log (VALHALLA_MSG_VERBOSE, __FUNCTION__);

  return calloc (1, sizeof (grabber_tvdb_t));
}

static int
grabber_tvdb_init (void *priv)
{
  grabber_tvdb_t *tvdb = priv;

  valhalla_log (VALHALLA_MSG_VERBOSE, __FUNCTION__);

  if (!tvdb)
    return -1;

  tvdb->handler = vh_url_new ();
  return tvdb->handler ? 0 : -1;
}

static void
grabber_tvdb_uninit (void *priv)
{
  grabber_tvdb_t *tvdb = priv;

  valhalla_log (VALHALLA_MSG_VERBOSE, __FUNCTION__);

  if (!tvdb)
    return;

  vh_url_free (tvdb->handler);
  free (tvdb);
}

static int
grabber_tvdb_grab (void *priv, file_data_t *data)
{
  grabber_tvdb_t *tvdb = priv;
  metadata_t *tag = NULL;
  char *keywords;
  int err;

  valhalla_log (VALHALLA_MSG_VERBOSE, __FUNCTION__);

  err = vh_metadata_get (data->meta_parser, "title", 0, &tag);
  if (err)
    return -1;

  /* Format the keywords */
  keywords = vh_url_escape_string (tvdb->handler, tag->value);
  if (!keywords)
    return -2;

  err = grabber_tvdb_get (tvdb->handler, data, tag->value, keywords);
  free (keywords);

  return err;
}

/****************************************************************************/
/* Public Grabber API                                                       */
/****************************************************************************/

/* vh_grabber_tvdb_register () */
GRABBER_REGISTER (tvdb,
                  GRABBER_CAP_FLAGS,
                  grabber_tvdb_priv,
                  grabber_tvdb_init,
                  grabber_tvdb_uninit,
                  grabber_tvdb_grab,
                  NULL)
