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
 * Foundation, Inc, 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "grabber_common.h"
#include "grabber_tvrage.h"
#include "metadata.h"
#include "xml_utils.h"
#include "url_utils.h"
#include "grabber_utils.h"
#include "utils.h"
#include "logs.h"

#define GRABBER_CAP_FLAGS \
  GRABBER_CAP_VIDEO

#define MAX_URL_SIZE      1024

#define TVRAGE_HOSTNAME           "services.tvrage.com"

/* See http://services.tvrage.com/index.php?page=public for the feeds */
#define TVRAGE_QUERY_SEARCH       "http://%s/feeds/search.php?show=%s"
#define TVRAGE_QUERY_INFO         "http://%s/feeds/full_show_info.php?sid=%s"

typedef struct grabber_tvrage_s {
  url_t *handler;
} grabber_tvrage_t;

static int
grabber_tvrage_get (url_t *handler, file_data_t *fdata,
                    const char *keywords, char *escaped_keywords)
{
  char url[MAX_URL_SIZE];
  url_data_t udata;
  int i;

  xmlDocPtr doc;
  xmlChar *tmp = NULL;
  xmlNode *n, *node;

  if (!keywords || !escaped_keywords)
    return -1;

  /* proceed with TVRage search request */
  memset (url, '\0', MAX_URL_SIZE);
  snprintf (url, MAX_URL_SIZE, TVRAGE_QUERY_SEARCH,
            TVRAGE_HOSTNAME, escaped_keywords);

  valhalla_log (VALHALLA_MSG_VERBOSE, "Search Request: %s", url);

  udata = url_get_data (handler, url);
  if (udata.status != 0)
    return -1;

  valhalla_log (VALHALLA_MSG_VERBOSE, "Search Reply: %s", udata.buffer);

  /* parse the XML answer */
  doc = get_xml_doc_from_memory (udata.buffer);
  free (udata.buffer);

  if (!doc)
    return -1;

  /* check for a known DB entry */
  n = get_node_xml_tree (xmlDocGetRootElement (doc), "show");
  if (!n)
  {
    valhalla_log (VALHALLA_MSG_VERBOSE,
                  "Unable to find the item \"%s\"", escaped_keywords);
    goto error;
  }

  /* get TVRage show id */
  tmp = get_prop_value_from_xml_tree (n, "showid");
  if (!tmp)
    goto error;

  xmlFreeDoc (doc);
  doc = NULL;

  /* proceed with TVRage search request */
  memset (url, '\0', MAX_URL_SIZE);
  snprintf (url, MAX_URL_SIZE,
            TVRAGE_QUERY_INFO, TVRAGE_HOSTNAME, tmp);
  xmlFree (tmp);

  valhalla_log (VALHALLA_MSG_VERBOSE, "Info Request: %s", url);

  udata = url_get_data (handler, url);
  if (udata.status != 0)
    goto error;

  valhalla_log (VALHALLA_MSG_VERBOSE, "Info Reply: %s", udata.buffer);

  /* parse the XML answer */
  doc = get_xml_doc_from_memory (udata.buffer);
  free (udata.buffer);
  if (!doc)
    goto error;

  n = xmlDocGetRootElement (doc);

  /* fetch tv show french title (to be extended to language param) */
  tmp = get_prop_value_from_xml_tree_by_attr (n, "aka", "country", "FR");
  if (tmp)
  {
    metadata_add (&fdata->meta_grabber, "alternative_title",
                  (char *) tmp, VALHALLA_META_GRP_TITLES);
    xmlFree (tmp);
  }

  /* fetch tv show country */
  grabber_parse_str (fdata, n, "origin_country",
                     "country", VALHALLA_META_GRP_COMMERCIAL);

  /* fetch tv show studio */
  grabber_parse_str (fdata, n, "network",
                     "studio", VALHALLA_META_GRP_COMMERCIAL);

  /* fetch tv show runtime (in minutes) */
  grabber_parse_str (fdata, n, "runtime",
                     "runtime", VALHALLA_META_GRP_CLASSIFICATION);

  /* fetch movie categories */
  node = get_node_xml_tree (n, "genre");
  for (i = 0; i < 5; i++)
  {
    if (!node)
      break;

    tmp = get_prop_value_from_xml_tree (node, "genre");
    if (tmp)
    {
      metadata_add (&fdata->meta_grabber, "category",
                    (char *) tmp, VALHALLA_META_GRP_CLASSIFICATION);
      xmlFree (tmp);
    }
    node = node->next;
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
grabber_tvrage_priv (void)
{
  valhalla_log (VALHALLA_MSG_VERBOSE, __FUNCTION__);

  return calloc (1, sizeof (grabber_tvrage_t));
}

static int
grabber_tvrage_init (void *priv)
{
  grabber_tvrage_t *tvrage = priv;

  valhalla_log (VALHALLA_MSG_VERBOSE, __FUNCTION__);

  if (!tvrage)
    return -1;

  tvrage->handler = url_new ();
  return tvrage->handler ? 0 : -1;
}

static void
grabber_tvrage_uninit (void *priv)
{
  grabber_tvrage_t *tvrage = priv;

  valhalla_log (VALHALLA_MSG_VERBOSE, __FUNCTION__);

  if (!tvrage)
    return;

  url_free (tvrage->handler);
  free (tvrage);
}

static int
grabber_tvrage_grab (void *priv, file_data_t *data)
{
  grabber_tvrage_t *tvrage = priv;
  metadata_t *tag = NULL;
  char *keywords;
  int err;

  valhalla_log (VALHALLA_MSG_VERBOSE, __FUNCTION__);

  err = metadata_get (data->meta_parser, "title", 0, &tag);
  if (err)
    return -1;

  /* Format the keywords */
  keywords = url_escape_string (tvrage->handler, tag->value);
  if (!keywords)
    return -2;

  err = grabber_tvrage_get (tvrage->handler, data, tag->value, keywords);
  free (keywords);

  return err;
}

/****************************************************************************/
/* Public Grabber API                                                       */
/****************************************************************************/

/* grabber_tvrage_register () */
GRABBER_REGISTER (tvrage,
                  GRABBER_CAP_FLAGS,
                  grabber_tvrage_priv,
                  grabber_tvrage_init,
                  grabber_tvrage_uninit,
                  grabber_tvrage_grab,
                  NULL)
