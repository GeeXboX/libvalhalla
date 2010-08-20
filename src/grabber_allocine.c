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
#include "grabber_allocine.h"
#include "metadata.h"
#include "xml_utils.h"
#include "url_utils.h"
#include "grabber_utils.h"
#include "utils.h"
#include "logs.h"

#define GRABBER_CAP_FLAGS \
  GRABBER_CAP_VIDEO

#define ALLOCINE_HOSTNAME        "www.geexbox.org/php"
#define ALLOCINE_QUERY           "http://%s/searchMovieAllocine.php?title=%s"

typedef struct grabber_allocine_s {
  url_t *handler;
  const metadata_plist_t *pl;
} grabber_allocine_t;

static const metadata_plist_t allocine_pl[] = {
  { NULL,                             VALHALLA_METADATA_PL_NORMAL   }
};


static int
grabber_allocine_get (grabber_allocine_t *allocine, file_data_t *fdata,
                      const char *keywords, char *escaped_keywords)
{
  char url[MAX_URL_SIZE];
  url_data_t udata;
  int res_int = 0;

  xmlDocPtr doc;
  xmlChar *tmp = NULL;
  xmlNode *n;

  if (!keywords || !escaped_keywords)
    return -1;

  /* proceed with Allocine search request */
  snprintf (url, sizeof (url), ALLOCINE_QUERY,
            ALLOCINE_HOSTNAME, escaped_keywords);

  vh_log (VALHALLA_MSG_VERBOSE, "Search Request: %s", url);

  udata = vh_url_get_data (allocine->handler, url);
  if (udata.status != 0)
    return -1;

  vh_log (VALHALLA_MSG_VERBOSE, "Search Reply: %s", udata.buffer);

  /* parse the XML answer */
  doc = vh_xml_get_doc_from_memory (udata.buffer);
  free (udata.buffer);

  if (!doc)
    return -1;

  n = xmlDocGetRootElement (doc);

  /* check for total number of results */
  tmp = vh_xml_get_prop_value_from_tree (n, "totalResults");
  if (!tmp)
  {
    vh_log (VALHALLA_MSG_VERBOSE,
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
  tmp = vh_xml_get_prop_value_from_tree (n, "title");
  if (tmp)
  {
    /* special trick to retrieve english title,
     *  used by next grabbers to find cover and fan arts.
     */
    vh_metadata_add_auto (&fdata->meta_grabber, VALHALLA_METADATA_TITLE,
                          (char *) tmp, VALHALLA_LANG_EN, allocine->pl);
    xmlFree (tmp);
  }

  /* fetch movie alternative title */
  vh_grabber_parse_str (fdata, n, "alternative_title",
                        VALHALLA_METADATA_TITLE_ALTERNATIVE,
                        VALHALLA_LANG_FR, allocine->pl);

  /* fetch movie overview description */
  vh_grabber_parse_str (fdata, n, "short_overview", VALHALLA_METADATA_SYNOPSIS,
                        VALHALLA_LANG_FR, allocine->pl);

  /* fetch movie runtime (in minutes) */
  vh_grabber_parse_str (fdata, n, "runtime", VALHALLA_METADATA_RUNTIME,
                        VALHALLA_LANG_UNDEF, allocine->pl);

  /* fetch movie year of production */
  vh_xml_search_year (n, "release", &res_int);
  if (res_int)
  {
    vh_grabber_parse_int (fdata, res_int, VALHALLA_METADATA_YEAR, allocine->pl);
    res_int = 0;
  }

  /* fetch movie categories */
  vh_grabber_parse_categories (fdata, n, VALHALLA_LANG_FR, allocine->pl);

  /* fetch movie rating */
  vh_xml_search_int (n, "popularity", &res_int);
  /* allocine ranks from 0 to 4, we do from 0 to 5 */
  if (res_int)
  {
    vh_grabber_parse_int (fdata, res_int + 1,
                          VALHALLA_METADATA_RATING, allocine->pl);
    res_int = 0;
  }

  /* fetch movie budget */
  vh_grabber_parse_str (fdata, n, "budget", VALHALLA_METADATA_BUDGET,
                        VALHALLA_LANG_UNDEF, allocine->pl);

  /* fetch movie people */
  vh_grabber_parse_casting (fdata, n, allocine->pl);

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
  vh_log (VALHALLA_MSG_VERBOSE, __FUNCTION__);

  return calloc (1, sizeof (grabber_allocine_t));
}

static int
grabber_allocine_init (void *priv, const grabber_param_t *param)
{
  grabber_allocine_t *allocine = priv;

  vh_log (VALHALLA_MSG_VERBOSE, __FUNCTION__);

  if (!allocine)
    return -1;

  allocine->handler = vh_url_new (param->url_ctl);
  allocine->pl      = param->pl;
  return allocine->handler ? 0 : -1;
}

static void
grabber_allocine_uninit (void *priv)
{
  grabber_allocine_t *allocine = priv;

  vh_log (VALHALLA_MSG_VERBOSE, __FUNCTION__);

  if (!allocine)
    return;

  vh_url_free (allocine->handler);
  free (allocine);
}

static int
grabber_allocine_grab (void *priv, file_data_t *data)
{
  grabber_allocine_t *allocine = priv;
  const metadata_t *tag = NULL;
  char *keywords;
  int err;

  vh_log (VALHALLA_MSG_VERBOSE, __FUNCTION__);

  err = vh_metadata_get (data->meta_parser, "title", 0, &tag);
  if (err)
    return -1;

  /* Format the keywords */
  keywords = vh_url_escape_string (allocine->handler, tag->value);
  if (!keywords)
    return -2;

  err = grabber_allocine_get (allocine, data, tag->value, keywords);
  free (keywords);

  return err;
}

/****************************************************************************/
/* Public Grabber API                                                       */
/****************************************************************************/

/* vh_grabber_allocine_register () */
GRABBER_REGISTER (allocine,
                  GRABBER_CAP_FLAGS,
                  allocine_pl,
                  0,
                  grabber_allocine_priv,
                  grabber_allocine_init,
                  grabber_allocine_uninit,
                  grabber_allocine_grab,
                  NULL)
