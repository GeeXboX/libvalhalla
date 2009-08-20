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
#include <ctype.h>

#include "grabber_common.h"
#include "grabber_allocine.h"
#include "metadata.h"
#include "xml_utils.h"
#include "url_utils.h"
#include "grabber_utils.h"
#include "utils.h"
#include "logs.h"

#define GRABBER_CAP_FLAGS \
  GRABBER_CAP_VIDEO

#define MAX_URL_SIZE             1024

#define ALLOCINE_HOSTNAME        "www.geexbox.org/php"
#define ALLOCINE_QUERY           "http://%s/searchMovieAllocine.php?title=%s"

typedef struct grabber_allocine_s {
  url_t *handler;
} grabber_allocine_t;

static int
grabber_allocine_get (url_t *handler, file_data_t *fdata,
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

  /* proceed with Allocine search request */
  memset (url, '\0', MAX_URL_SIZE);
  snprintf (url, MAX_URL_SIZE, ALLOCINE_QUERY,
            ALLOCINE_HOSTNAME, escaped_keywords);

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

  n = xmlDocGetRootElement (doc);

  /* check for total number of results */
  tmp = get_prop_value_from_xml_tree (n, "totalResults");
  if (!tmp)
  {
    valhalla_log (VALHALLA_MSG_VERBOSE,
                  "Unable to find the item \"%s\"", escaped_keywords);
    goto error;
  }

  /* if there is no result goto error */
  if (!xmlStrcmp ((unsigned char *) tmp, (unsigned char *) "0"))
    goto error;

  xmlFree (tmp);

  /* fetch movie title */
  tmp = get_prop_value_from_xml_tree (n, "title");
  if (tmp)
  {
    /* special trick to retrieve english title,
     *  used by next grabbers to find cover and backdrops.
     */
    metadata_add (&fdata->meta_grabber, "title",
                  (char *) tmp, VALHALLA_META_GRP_TITLES);
    xmlFree (tmp);
  }

  /* fetch movie alternative title */
  grabber_parse_str (fdata, n, "alternative_title",
                     "alternative_title", VALHALLA_META_GRP_TITLES);

  /* fetch movie overview description */
  grabber_parse_str (fdata, n, "short_overview",
                     "synopsis", VALHALLA_META_GRP_CLASSIFICATION);

  /* fetch movie runtime (in minutes) */
  grabber_parse_str (fdata, n, "runtime",
                     "runtime", VALHALLA_META_GRP_CLASSIFICATION);

  /* fetch movie year of production */
  xml_search_year (n, "release", &res_int);
  if (res_int)
  {
    grabber_parse_int (fdata, res_int, "year", VALHALLA_META_GRP_TEMPORAL);
    res_int = 0;
  }

  /* fetch movie categories */
  grabber_parse_categories (fdata, n);

  /* fetch movie rating */
  xml_search_int (n, "popularity", &res_int);
  /* allocine ranks from 0 to 4, we do from 0 to 5 */
  if (res_int)
  {
    grabber_parse_int (fdata, res_int + 1,
                       "rating", VALHALLA_META_GRP_PERSONAL);
    res_int = 0;
  }

  /* fetch movie budget */
  grabber_parse_str (fdata, n,
                     "budget", "budget", VALHALLA_META_GRP_COMMERCIAL);

  /* fetch movie people */
  grabber_parse_casting (fdata, n);

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
grabber_allocine_priv (void)
{
  valhalla_log (VALHALLA_MSG_VERBOSE, __FUNCTION__);

  return calloc (1, sizeof (grabber_allocine_t));
}

static int
grabber_allocine_init (void *priv)
{
  grabber_allocine_t *allocine = priv;

  valhalla_log (VALHALLA_MSG_VERBOSE, __FUNCTION__);

  if (!allocine)
    return -1;

  allocine->handler = url_new ();
  return allocine->handler ? 0 : -1;
}

static void
grabber_allocine_uninit (void *priv)
{
  grabber_allocine_t *allocine = priv;

  valhalla_log (VALHALLA_MSG_VERBOSE, __FUNCTION__);

  if (!allocine)
    return;

  url_free (allocine->handler);
  free (allocine);
}

static int
grabber_allocine_grab (void *priv, file_data_t *data)
{
  grabber_allocine_t *allocine = priv;
  metadata_t *tag = NULL;
  char *keywords;
  int err;

  valhalla_log (VALHALLA_MSG_VERBOSE, __FUNCTION__);

  err = metadata_get (data->meta_parser, "title", 0, &tag);
  if (err)
    return -1;

  /* Format the keywords */
  keywords = url_escape_string (allocine->handler, tag->value);
  if (!keywords)
    return -2;

  err = grabber_allocine_get (allocine->handler, data, tag->value, keywords);
  free (keywords);

  return err;
}

/****************************************************************************/
/* Public Grabber API                                                       */
/****************************************************************************/

/* grabber_allocine_register () */
GRABBER_REGISTER (allocine,
                  GRABBER_CAP_FLAGS,
                  grabber_allocine_priv,
                  grabber_allocine_init,
                  grabber_allocine_uninit,
                  grabber_allocine_grab,
                  NULL)
