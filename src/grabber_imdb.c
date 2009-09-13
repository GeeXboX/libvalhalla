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
#include "grabber_imdb.h"
#include "metadata.h"
#include "xml_utils.h"
#include "url_utils.h"
#include "grabber_utils.h"
#include "utils.h"
#include "logs.h"

#define GRABBER_CAP_FLAGS \
  GRABBER_CAP_VIDEO

#define MAX_URL_SIZE             1024

#define IMDB_HOSTNAME        "www.geexbox.org/php"
#define IMDB_QUERY           "http://%s/searchMovieIMDB.php?title=%s"

typedef struct grabber_imdb_s {
  url_t *handler;
} grabber_imdb_t;


static int
grabber_imdb_get (url_t *handler, file_data_t *fdata,
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

  /* proceed with ImDB search request */
  memset (url, '\0', MAX_URL_SIZE);
  snprintf (url, MAX_URL_SIZE, IMDB_QUERY,
            IMDB_HOSTNAME, escaped_keywords);

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

  n = xmlDocGetRootElement (doc);

  /* check for total number of results */
  tmp = vh_get_prop_value_from_xml_tree (n, "totalResults");
  if (!tmp)
  {
    valhalla_log (VALHALLA_MSG_VERBOSE,
                  "Unable to find the item \"%s\"", escaped_keywords);
    goto error;
  }

  /* if there is no result goto error */
  if (!xmlStrcmp ((unsigned char *) tmp, (unsigned char *) "0"))
  {
    xmlFree (tmp);
    goto error;
  }

  xmlFree (tmp);

  /* fetch movie title */
  tmp = vh_get_prop_value_from_xml_tree (n, "title");
  if (tmp)
  {
    /* special trick to retrieve english title,
     *  used by next grabbers to find cover and backdrops.
     */
    vh_metadata_add (&fdata->meta_grabber, "title",
                     (char *) tmp, VALHALLA_META_GRP_TITLES);
    xmlFree (tmp);
  }

  /* fetch movie alternative title */
  vh_grabber_parse_str (fdata, n, "alternative_title",
                     "alternative_title", VALHALLA_META_GRP_TITLES);

  /* fetch movie overview description */
  vh_grabber_parse_str (fdata, n, "short_overview",
                     "synopsis", VALHALLA_META_GRP_CLASSIFICATION);

  /* fetch movie runtime (in minutes) */
  vh_grabber_parse_str (fdata, n, "runtime",
                     "runtime", VALHALLA_META_GRP_CLASSIFICATION);

  /* fetch movie year of production */
  vh_xml_search_year (n, "release", &res_int);
  if (res_int)
  {
    vh_grabber_parse_int (fdata, res_int, "year", VALHALLA_META_GRP_TEMPORAL);
    res_int = 0;
  }

  /* fetch movie categories */
  vh_grabber_parse_categories (fdata, n);

  /* fetch movie rating */
  vh_xml_search_int (n, "rating", &res_int);
  /* ImDB ranks from 0 to 10, we do from 0 to 5 */
  if (res_int)
  {
    vh_grabber_parse_int (fdata, res_int / 2,
                       "rating", VALHALLA_META_GRP_PERSONAL);
    res_int = 0;
  }

  /* fetch movie people */
  vh_grabber_parse_casting (fdata, n);

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
grabber_imdb_priv (void)
{
  valhalla_log (VALHALLA_MSG_VERBOSE, __FUNCTION__);

  return calloc (1, sizeof (grabber_imdb_t));
}

static int
grabber_imdb_init (void *priv)
{
  grabber_imdb_t *imdb = priv;

  valhalla_log (VALHALLA_MSG_VERBOSE, __FUNCTION__);

  if (!imdb)
    return -1;

  imdb->handler = vh_url_new ();
  return imdb->handler ? 0 : -1;
}

static void
grabber_imdb_uninit (void *priv)
{
  grabber_imdb_t *imdb = priv;

  valhalla_log (VALHALLA_MSG_VERBOSE, __FUNCTION__);

  if (!imdb)
    return;

  vh_url_free (imdb->handler);
  free (imdb);
}

static int
grabber_imdb_grab (void *priv, file_data_t *data)
{
  grabber_imdb_t *imdb = priv;
  metadata_t *tag = NULL;
  char *keywords;
  int err;

  valhalla_log (VALHALLA_MSG_VERBOSE, __FUNCTION__);

  err = vh_metadata_get (data->meta_parser, "title", 0, &tag);
  if (err)
    return -1;

  /* Format the keywords */
  keywords = vh_url_escape_string (imdb->handler, tag->value);
  if (!keywords)
    return -2;

  err = grabber_imdb_get (imdb->handler, data, tag->value, keywords);
  free (keywords);

  return err;
}

/****************************************************************************/
/* Public Grabber API                                                       */
/****************************************************************************/

/* vh_grabber_imdb_register () */
GRABBER_REGISTER (imdb,
                  GRABBER_CAP_FLAGS,
                  grabber_imdb_priv,
                  grabber_imdb_init,
                  grabber_imdb_uninit,
                  grabber_imdb_grab,
                  NULL)
