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

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "grabber_common.h"
#include "grabber_amazon.h"
#include "metadata.h"
#include "xml_utils.h"
#include "url_utils.h"
#include "utils.h"
#include "logs.h"
#include "md5.h"
#include "list.h"

#define GRABBER_CAP_FLAGS \
  GRABBER_CAP_VIDEO | \
  GRABBER_CAP_AUDIO

#define MAX_URL_SIZE        1024
#define MAX_KEYWORD_SIZE    1024
#define MAX_BUF_LEN         65535

#define AMAZON_HOSTNAME     "webservices.amazon.com"
#define AMAZON_LICENSE_KEY  "0P1862RFDFSF4KYZQNG2"

#define AMAZON_SEARCH "http://%s/onca/xml?Service=AWSECommerceService&SubscriptionId=%s&Operation=ItemSearch&Keywords=%s&SearchIndex=%s"
#define AMAZON_SEARCH_MUSIC "Music"
#define AMAZON_SEARCH_MOVIE "DVD"

#define AMAZON_SEARCH_COVER "http://%s/onca/xml?Service=AWSECommerceService&SubscriptionId=%s&Operation=ItemLookup&ItemId=%s&ResponseGroup=Images"

typedef struct grabber_amazon_s {
  url_t  *handler;
  list_t *list;
} grabber_amazon_t;


static int
grabber_amazon_cover_get (url_t *handler, char **dl_url,
                          const char *search_type,
                          const char *keywords, char *escaped_keywords)
{
  char url[MAX_URL_SIZE];
  url_data_t data;

  xmlDocPtr doc;
  xmlNode  *img;
  xmlChar  *asin, *cover_url;

  if (!search_type || !keywords || !escaped_keywords)
    return -1;

  /* 2. Prepare Amazon WebService URL for Search */
  snprintf (url, MAX_URL_SIZE, AMAZON_SEARCH,
            AMAZON_HOSTNAME, AMAZON_LICENSE_KEY, escaped_keywords, search_type);

  valhalla_log (VALHALLA_MSG_VERBOSE, "Search Request: %s", url);

  /* 3. Perform request */
  data = vh_url_get_data (handler, url);
  if (data.status)
    return -1;

  valhalla_log (VALHALLA_MSG_VERBOSE, "Search Reply: %s", data.buffer);

  /* 4. Parse the answer to get ASIN value */
  doc = vh_get_xml_doc_from_memory (data.buffer);
  free (data.buffer);

  if (!doc)
    return -1;

  asin = vh_get_prop_value_from_xml_tree (xmlDocGetRootElement (doc), "ASIN");
  xmlFreeDoc (doc);

  if (!asin)
  {
    valhalla_log (VALHALLA_MSG_VERBOSE,
                  "Unable to find the item \"%s\"", escaped_keywords);
    return -1;
  }

  valhalla_log (VALHALLA_MSG_VERBOSE, "Found Amazon ASIN: %s", asin);

  /* 5. Prepare Amazon WebService URL for Cover Search */
  snprintf (url, MAX_URL_SIZE, AMAZON_SEARCH_COVER,
            AMAZON_HOSTNAME, AMAZON_LICENSE_KEY, asin);
  xmlFree (asin);

  valhalla_log (VALHALLA_MSG_VERBOSE, "Cover Search Request: %s", url);

  /* 6. Perform request */
  data = vh_url_get_data (handler, url);
  if (data.status)
    return -1;

  valhalla_log (VALHALLA_MSG_VERBOSE, "Cover Search Reply: %s", data.buffer);

  /* 7. Parse the answer to get cover URL */
  doc = xmlReadMemory (data.buffer, data.size, NULL, NULL, 0);
  free(data.buffer);

  if (!doc)
    return -1;

  img = vh_get_node_xml_tree (xmlDocGetRootElement(doc), "LargeImage");
  if (!img)
    img = vh_get_node_xml_tree (xmlDocGetRootElement(doc), "MediumImage");
  if (!img)
    img = vh_get_node_xml_tree (xmlDocGetRootElement(doc), "SmallImage");

  if (!img)
  {
    xmlFreeDoc (doc);
    return -1;
  }

  cover_url = vh_get_prop_value_from_xml_tree (img, "URL");
  if (!cover_url)
  {
    valhalla_log (VALHALLA_MSG_VERBOSE,
                  "Unable to find the cover for %s", escaped_keywords);
    xmlFreeDoc (doc);
    return -1;
  }
  xmlFreeDoc (doc);

  *dl_url = strdup ((const char *) cover_url);
  xmlFree (cover_url);

  return 0;
}

static int
grabber_amazon_cmp_fct (const void *tocmp, const void *data)
{
  if (!tocmp || !data)
    return -1;

  return strcmp (tocmp, data);
}

static int
grabber_amazon_check (grabber_amazon_t *amazon, const char *cover)
{
  char *data = NULL;

  if (amazon->list)
  {
    data = vh_list_search (amazon->list, cover, grabber_amazon_cmp_fct);
    if (data)
      return 0;
  }

  vh_list_append (&amazon->list, cover, strlen (cover) + 1);
  return -1;
}

/****************************************************************************/
/* Private Grabber API                                                      */
/****************************************************************************/

static void *
grabber_amazon_priv (void)
{
  valhalla_log (VALHALLA_MSG_VERBOSE, __FUNCTION__);

  return calloc (1, sizeof (grabber_amazon_t));
}

static int
grabber_amazon_init (void *priv)
{
  grabber_amazon_t *amazon = priv;

  valhalla_log (VALHALLA_MSG_VERBOSE, __FUNCTION__);

  if (!amazon)
    return -1;

  amazon->handler = vh_url_new ();
  return amazon->handler ? 0 : -1;
}

static void
grabber_amazon_uninit (void *priv)
{
  grabber_amazon_t *amazon = priv;

  valhalla_log (VALHALLA_MSG_VERBOSE, __FUNCTION__);

  if (!amazon)
    return;

  if (amazon->handler)
    vh_url_free (amazon->handler);
  if (amazon->list)
    vh_list_free (amazon->list, NULL);

  free (amazon);
}

static int
grabber_amazon_grab (void *priv, file_data_t *data)
{
  grabber_amazon_t *amazon = priv;
  int res;
  char *escaped_keywords;
  const char *keywords;
  const char *search_type = NULL;
  char *cover, *url = NULL;
  metadata_t *tag = NULL;

  valhalla_log (VALHALLA_MSG_VERBOSE, __FUNCTION__);

  /*
   * Try with the album's name, or with the title.
   */
  if (   !vh_metadata_get (data->meta_parser, "album", 0, &tag)
      || !vh_metadata_get (data->meta_parser, "title", 0, &tag))
    keywords = tag->value;
  else
    return -1;

  cover = vh_md5sum (keywords);
  /*
   * Check if these keywords were already used for retrieving a cover.
   * If yes, then only the association on the available cover is added.
   * Otherwise, the cover will be searched on Amazon.
   *
   * NOTE: if a video file uses the same keywords as an audio file, then
   *       the same cover is used because data->type is ignored for the
   *       checking.
   */
  res = grabber_amazon_check (amazon, cover);
  if (!res)
  {
    vh_metadata_add_auto (&data->meta_grabber, VALHALLA_METADATA_COVER, cover);
    free (cover);
    return 0;
  }

  switch (data->type)
  {
  default:
  case VALHALLA_FILE_TYPE_AUDIO:
    search_type = AMAZON_SEARCH_MUSIC;
    break;

  case VALHALLA_FILE_TYPE_VIDEO:
    search_type = AMAZON_SEARCH_MOVIE;
    break;
  }

  /* Format the keywords */
  escaped_keywords = vh_url_escape_string (amazon->handler, keywords);
  if (!escaped_keywords)
  {
    free (cover);
    return -2;
  }

  res = grabber_amazon_cover_get (amazon->handler, &url,
                                  search_type, keywords, escaped_keywords);
  free (escaped_keywords);
  if (!res)
  {
    vh_metadata_add_auto (&data->meta_grabber, VALHALLA_METADATA_COVER, cover);
    vh_file_dl_add (&data->list_downloader, url, cover, VALHALLA_DL_COVER);
    free (url);
  }
  free (cover);

  return res;
}

static void
grabber_amazon_loop (void *priv)
{
  grabber_amazon_t *amazon = priv;

  valhalla_log (VALHALLA_MSG_VERBOSE, __FUNCTION__);

  /* Hash cover list cleanup */
  VH_LIST_FREE (amazon->list, NULL);
}

/****************************************************************************/
/* Public Grabber API                                                       */
/****************************************************************************/

/* vh_grabber_amazon_register () */
GRABBER_REGISTER (amazon,
                  GRABBER_CAP_FLAGS,
                  grabber_amazon_priv,
                  grabber_amazon_init,
                  grabber_amazon_uninit,
                  grabber_amazon_grab,
                  grabber_amazon_loop)
