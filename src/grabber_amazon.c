/*
 * GeeXboX Valhalla: tiny media scanner API.
 * Copyright (C) 2009 Mathieu Schroeter <mathieu@schroetersa.ch>
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
#include <time.h>

#include <libavutil/base64.h>

#include "grabber_common.h"
#include "grabber_amazon.h"
#include "metadata.h"
#include "xml_utils.h"
#include "url_utils.h"
#include "utils.h"
#include "logs.h"
#include "md5.h"
#include "hmac_sha256.h"
#include "list.h"

#define GRABBER_CAP_FLAGS \
  GRABBER_CAP_VIDEO | \
  GRABBER_CAP_AUDIO

/*
 * The documentation is available on:
 *  http://docs.amazonwebservices.com/AWSECommerceService/latest/DG/index.html
 */

#define MAX_KEYWORD_SIZE    1024
#define MAX_BUF_LEN         65535

#define MAX_LIST_DEPTH      100

#define AMAZON_HOSTNAME     "webservices.amazon.com"
#define AMAZON_ACCESS_KEY   "AKIAJ27VCT7XRCSVPROQ"
#define AMAZON_KEYSIGN      "onViRIliSg2hOg5XzxkEkFieFgB0WA0G0k19NiUT"

/* The arguments in the queries MUST BE sorted by first byte value! */
#define AMAZON_ARGS         "AWSAccessKeyId=%s"           "&" \
                            "Keywords=%s"                 "&" \
                            "Operation=ItemSearch"        "&" \
                            "SearchIndex=%s"              "&" \
                            "Service=AWSECommerceService" "&" \
                            "Timestamp=%s"
#define AMAZON_ARGS_COVER   "AWSAccessKeyId=%s"           "&" \
                            "ItemId=%s"                   "&" \
                            "Operation=ItemLookup"        "&" \
                            "ResponseGroup=Images"        "&" \
                            "Service=AWSECommerceService" "&" \
                            "Timestamp=%s"
#define AMAZON_PAGE         "/onca/xml"
#define AMAZON_SEARCH       "http://%s" AMAZON_PAGE "?"

#define AMAZON_SEARCH_MUSIC "Music"
#define AMAZON_SEARCH_MOVIE "DVD"

typedef struct grabber_amazon_s {
  url_t  *handler;
  list_t *list;
  hmac_sha256_t *hd;
  const metadata_plist_t *pl;
} grabber_amazon_t;

static const metadata_plist_t amazon_pl[] = {
  { VALHALLA_METADATA_COVER,          VALHALLA_METADATA_PL_BELOW    },
  { NULL,                             VALHALLA_METADATA_PL_NORMAL   }
};


#define BASE64_OUTSIZE(size) ((((size) + 2) / 3) * 4 + 1)

#define APPEND_STRING(dst, src, len) \
  memcpy (dst + off, src, len);      \
  off += len;

static char *
grabber_amazon_signature (hmac_sha256_t *hd, const char *args)
{
  char *src;
  size_t size, off = 0;
  uint8_t *sign;
  char *sign64, *res = NULL;

  if (!args)
    return NULL;

  /*
   * GET\n                        (4)
   * webservices.amazon.com\n     (strlen (AMAZON_HOSTNAME) + 1)
   * /onca/xml\n                  (strlen (AMAZON_PAGE) + 1)
   * AWSAccessKeyId=...           (strlen (args))
   */
  size = strlen (AMAZON_HOSTNAME) + strlen (AMAZON_PAGE) + strlen (args) + 6;

  src = calloc (1, size + 1);
  if (!src)
    return NULL;

  /* Prepare string for the signature */
  APPEND_STRING (src, "GET\n",              4)
  APPEND_STRING (src, AMAZON_HOSTNAME "\n", strlen (AMAZON_HOSTNAME) + 1)
  APPEND_STRING (src, AMAZON_PAGE "\n",     strlen (AMAZON_PAGE) + 1)
  APPEND_STRING (src, args,                 strlen (args))

  vh_log (VALHALLA_MSG_VERBOSE, "String to be signed:\n%s", src);

  /* Compute signature */
  sign = vh_hmac_sha256_compute (hd, src, size);
  free (src);
  if (!sign)
    goto out;

  sign64 = calloc (1, BASE64_OUTSIZE (size) + 1);
  if (!sign64)
    goto out;

  res =
    av_base64_encode (sign64, BASE64_OUTSIZE (size), sign, VH_HMAC_SHA256_SIZE);
  if (!res)
    free (sign64);

 out:
  vh_hmac_sha256_reset (hd);
  return res;
}

static int
grabber_amazon_cover_get (url_t *handler, hmac_sha256_t *hd,
                          char **dl_url, const char *search_type,
                          const char *keywords, char *escaped_keywords)
{
  char args[MAX_URL_SIZE];
  char url[MAX_URL_SIZE];
  char timestamp[32];
  char *sign64, *escaped_sign64;
  url_data_t data;

  time_t rawtime;
  struct tm timeinfo;

  xmlDocPtr doc;
  xmlNode  *img;
  xmlChar  *asin, *cover_url;

  if (!search_type || !keywords || !escaped_keywords)
    return -1;

  time (&rawtime);
  if (gmtime_r (&rawtime, &timeinfo))
    strftime (timestamp,
              sizeof (timestamp), "%Y-%m-%dT%H%%3A%M%%3A%SZ", &timeinfo);
  else
    return -1;

  /* 2. Prepare Amazon WebService URL for Search */
  snprintf (args, sizeof (args), AMAZON_ARGS, AMAZON_ACCESS_KEY,
            escaped_keywords, search_type, timestamp);

  sign64 = grabber_amazon_signature (hd, args);
  if (!sign64)
    return -1;

  escaped_sign64 = vh_url_escape_string (handler, sign64);
  free (sign64);
  if (!escaped_sign64)
    return -1;

  snprintf (url, sizeof (url),
            AMAZON_SEARCH "%s&Signature=%s",
            AMAZON_HOSTNAME, args, escaped_sign64);
  free (escaped_sign64);

  vh_log (VALHALLA_MSG_VERBOSE, "Search Request: %s", url);

  /* 3. Perform request */
  data = vh_url_get_data (handler, url);
  if (data.status)
    return -1;

  vh_log (VALHALLA_MSG_VERBOSE, "Search Reply: %s", data.buffer);

  /* 4. Parse the answer to get ASIN value */
  doc = vh_xml_get_doc_from_memory (data.buffer);
  free (data.buffer);

  if (!doc)
    return -1;

  asin = vh_xml_get_prop_value_from_tree (xmlDocGetRootElement (doc), "ASIN");
  xmlFreeDoc (doc);

  if (!asin)
  {
    vh_log (VALHALLA_MSG_VERBOSE,
            "Unable to find the item \"%s\"", escaped_keywords);
    return -1;
  }

  vh_log (VALHALLA_MSG_VERBOSE, "Found Amazon ASIN: %s", asin);

  /* 5. Prepare Amazon WebService URL for Cover Search */
  snprintf (args, sizeof (args),
            AMAZON_ARGS_COVER, AMAZON_ACCESS_KEY, asin, timestamp);
  xmlFree (asin);

  sign64 = grabber_amazon_signature (hd, args);
  if (!sign64)
    return -1;

  escaped_sign64 = vh_url_escape_string (handler, sign64);
  free (sign64);
  if (!escaped_sign64)
    return -1;

  snprintf (url, sizeof (url),
            AMAZON_SEARCH "%s&Signature=%s",
            AMAZON_HOSTNAME, args, escaped_sign64);
  free (escaped_sign64);

  vh_log (VALHALLA_MSG_VERBOSE, "Cover Search Request: %s", url);

  /* 6. Perform request */
  data = vh_url_get_data (handler, url);
  if (data.status)
    return -1;

  vh_log (VALHALLA_MSG_VERBOSE, "Cover Search Reply: %s", data.buffer);

  /* 7. Parse the answer to get cover URL */
  doc = xmlReadMemory (data.buffer, data.size, NULL, NULL, 0);
  free(data.buffer);

  if (!doc)
    return -1;

  img = vh_xml_get_node_tree (xmlDocGetRootElement(doc), "LargeImage");
  if (!img)
    img = vh_xml_get_node_tree (xmlDocGetRootElement(doc), "MediumImage");
  if (!img)
    img = vh_xml_get_node_tree (xmlDocGetRootElement(doc), "SmallImage");

  if (!img)
  {
    xmlFreeDoc (doc);
    return -1;
  }

  cover_url = vh_xml_get_prop_value_from_tree (img, "URL");
  if (!cover_url)
  {
    vh_log (VALHALLA_MSG_VERBOSE,
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

  if (!amazon->list)
    return -1;

  data = vh_list_search (amazon->list, cover, grabber_amazon_cmp_fct);
  if (data)
    return 0;

  vh_list_append (amazon->list, cover, strlen (cover) + 1);
  return -1;
}

/****************************************************************************/
/* Private Grabber API                                                      */
/****************************************************************************/

static void *
grabber_amazon_priv (void)
{
  vh_log (VALHALLA_MSG_VERBOSE, __FUNCTION__);

  return calloc (1, sizeof (grabber_amazon_t));
}

static int
grabber_amazon_init (void *priv, const grabber_param_t *param)
{
  grabber_amazon_t *amazon = priv;

  vh_log (VALHALLA_MSG_VERBOSE, __FUNCTION__);

  if (!amazon)
    return -1;

  amazon->hd = vh_hmac_sha256_new (AMAZON_KEYSIGN);
  if (!amazon->hd)
    return -1;

  amazon->list = vh_list_new (MAX_LIST_DEPTH, NULL);
  if (!amazon->list)
    return -1;

  amazon->handler = vh_url_new (param->url_ctl);
  amazon->pl      = param->pl;
  return amazon->handler ? 0 : -1;
}

static void
grabber_amazon_uninit (void *priv)
{
  grabber_amazon_t *amazon = priv;

  vh_log (VALHALLA_MSG_VERBOSE, __FUNCTION__);

  if (!amazon)
    return;

  vh_hmac_sha256_free (amazon->hd);
  if (amazon->handler)
    vh_url_free (amazon->handler);
  if (amazon->list)
    vh_list_free (amazon->list);

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
  const metadata_t *tag = NULL;

  vh_log (VALHALLA_MSG_VERBOSE, __FUNCTION__);

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
    vh_metadata_add_auto (&data->meta_grabber, VALHALLA_METADATA_COVER,
                          cover, VALHALLA_LANG_UNDEF, amazon->pl);
    free (cover);
    return 0;
  }

  switch (data->file.type)
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

  res = grabber_amazon_cover_get (amazon->handler, amazon->hd, &url,
                                  search_type, keywords, escaped_keywords);
  free (escaped_keywords);
  if (!res)
  {
    vh_metadata_add_auto (&data->meta_grabber, VALHALLA_METADATA_COVER,
                          cover, VALHALLA_LANG_UNDEF, amazon->pl);
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

  vh_log (VALHALLA_MSG_VERBOSE, __FUNCTION__);

  /* Hash cover list cleanup */
  vh_list_empty (amazon->list);
}

/****************************************************************************/
/* Public Grabber API                                                       */
/****************************************************************************/

/* vh_grabber_amazon_register () */
GRABBER_REGISTER (amazon,
                  GRABBER_CAP_FLAGS,
                  amazon_pl,
                  0,
                  grabber_amazon_priv,
                  grabber_amazon_init,
                  grabber_amazon_uninit,
                  grabber_amazon_grab,
                  grabber_amazon_loop)
