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
#include "grabber_tmdb.h"
#include "metadata.h"
#include "xml_utils.h"
#include "url_utils.h"
#include "utils.h"
#include "logs.h"
#include "md5.h"
#include "list.h"

#define GRABBER_CAP_FLAGS \
  GRABBER_CAP_COVER

#define MAX_URL_SIZE      1024

#define PATH_BACKDROPS    "backdrops"
#define PATH_COVERS       "covers"

#define TMDB_HOSTNAME     "api.themoviedb.org"

#define TMDB_API_KEY      "5401cd030990fba60e1c23d2832de62e"

#define TMDB_QUERY_SEARCH "http://%s/2.0/Movie.search?title=%s&api_key=%s"
#define TMDB_QUERY_INFO   "http://%s/2.0/Movie.getInfo?id=%s&api_key=%s"

#define tmdb_parse(nd,tag,name,group)                             \
  xml_search_str (nd, tag, &res_str);                             \
  if (res_str)                                                    \
  {                                                               \
    metadata_add (&fdata->meta_grabber, name, res_str, group);    \
    free (res_str);                                               \
    res_str = NULL;                                               \
  }

typedef struct grabber_tmdb_s {
  url_t  *handler;
  list_t *list;
} grabber_tmdb_t;

static const struct {
  const char *tag;
  const char *name;
} tmdb_person_mapping[] = {
  { "actor",                        "actor"                       },
  { "author",                       "author"                      },
  { "casting",                      "casting"                     },
  { "director",                     "director"                    },
  { "director_of_photography",      "director_of_photography"     },
  { "editor",                       "editor"                      },
  { "original_music_composer",      "composer"                    },
  { "producer",                     "producer"                    },
  { NULL,                           NULL                          }
};

static void
grabber_tmdb_add_person (file_data_t *fdata, xmlNode *node, const char *cat)
{
  char *name = NULL, *role = NULL;

  if (!fdata || !node || !cat)
    return;

  xml_search_str (node, "name", &name);
  xml_search_str (node, "role", &role);

  if (name)
  {
    char str[128] = { 0 };

    if (role && strcmp (role, ""))
      snprintf (str, sizeof (str), "%s (%s)", name, role);
    else
      snprintf (str, sizeof (str), "%s", name);
    free (name);
    metadata_add (&fdata->meta_grabber, cat, str, VALHALLA_META_GRP_ENTITIES);
  }

  if (role)
    free (role);
}

static void
grabber_tmdb_get_picture (file_data_t *fdata, const char *keywords,
                          xmlChar *url, valhalla_dl_t dl)
{
  char name[1024] = { 0 };
  char *type, *cover = NULL;

  if (!fdata || !url)
    return;

  if (dl == VALHALLA_DL_COVER)
    type = "cover";
  else if (dl == VALHALLA_DL_BACKDROP)
    type = "backdrop";
  else
    return;

  snprintf (name, sizeof (name), "%s-%s", type, keywords);
  cover = md5sum (name);

  metadata_add (&fdata->meta_grabber, type,
                cover, VALHALLA_META_GRP_MISCELLANEOUS);
  file_dl_add (&fdata->list_downloader, (char *) url, cover, dl);
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

  char *res_str;
  int res_int;
  int i;

  if (!keywords || !escaped_keywords)
    return -1;

  /* proceed with TMDB search request */
  memset (url, '\0', MAX_URL_SIZE);
  snprintf (url, MAX_URL_SIZE, TMDB_QUERY_SEARCH,
            TMDB_HOSTNAME, escaped_keywords, TMDB_API_KEY);

  valhalla_log (VALHALLA_MSG_VERBOSE, "Search Request: %s", url);

  udata = url_get_data (handler, url);
  if (udata.status != 0)
    return -1;

  valhalla_log (VALHALLA_MSG_VERBOSE, "Search Reply: %s\n", udata.buffer);

  /* parse the XML answer */
  doc = get_xml_doc_from_memory (udata.buffer);
  free (udata.buffer);

  if (!doc)
    return -1;

  /* check for total number of results */
  n = xmlDocGetRootElement (doc);
  tmp = get_prop_value_from_xml_tree (n, "totalResults");
  if (!tmp)
  {
    valhalla_log (VALHALLA_MSG_VERBOSE,
                  "Unable to find the item \"%s\"", escaped_keywords);
    goto error;
  }

  /* check that requested item is known on TMDB */
  if (!xmlStrcmp ((unsigned char *) tmp, (unsigned char *) "0"))
    goto error;
  xmlFree (tmp);

  /* get TMDB Movie ID */
  tmp = get_prop_value_from_xml_tree (n, "id");
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

  /* fetch movie overview description */
  tmdb_parse (n, "short_overview",
              "synopsis", VALHALLA_META_GRP_CLASSIFICATION);

  /* fetch movie runtime (in minutes) */
  tmdb_parse (n, "runtime", "runtime", VALHALLA_META_GRP_CLASSIFICATION);

  /* fetch movie year of production */
  xml_search_int (n, "release", &res_int);
  if (res_int)
  {
    char v[64] = { 0 };

    snprintf (v, 64, "%d", res_int);
    metadata_add (&fdata->meta_grabber, "year", v, VALHALLA_META_GRP_TEMPORAL);
    res_int = 0;
  }

  /* fetch movie rating */
  xml_search_int (n, "rating", &res_int);
  if (res_int)
  {
    char v[64] = { 0 };

    snprintf (v, 64, "%d", res_int / 2);
    metadata_add (&fdata->meta_grabber, "rating",
                  v, VALHALLA_META_GRP_PERSONAL);
    res_int = 0;
  }

  /* fetch movie budget */
  tmdb_parse (n, "budget", "budget", VALHALLA_META_GRP_COMMERCIAL);

  /* fetch movie revenue */
  tmdb_parse (n, "revenue", "revenue", VALHALLA_META_GRP_COMMERCIAL);

  /* fetch movie country */
  node = get_node_xml_tree (n, "country");
  if (node)
  {
    tmp = get_prop_value_from_xml_tree (node, "short_name");
    if (tmp)
    {
      metadata_add (&fdata->meta_grabber, "country",
                    (char *) tmp, VALHALLA_META_GRP_COMMERCIAL);
      xmlFree (tmp);
    }
  }

  /* fetch movie categories */
  node = get_node_xml_tree (n, "category");
  for (i = 0; i < 5; i++)
  {
    if (!node)
      break;

    tmp = get_prop_value_from_xml_tree (node, "name");
    if (tmp)
    {
      metadata_add (&fdata->meta_grabber, "category",
                    (char *) tmp, VALHALLA_META_GRP_CLASSIFICATION);
      xmlFree (tmp);
    }
    node = node->next;
  }

  /* fetch movie people */
  node = get_node_xml_tree (n, "person");
  while (node)
  {
    xmlChar *ch;

    ch = get_attr_value_from_node (node, "job");
    if (!ch)
    {
      node = node->next;
      continue;
    }

    for (i = 0; tmdb_person_mapping[i].tag; i++)
      if (!strcmp ((char *) ch, tmdb_person_mapping[i].tag))
      {
        grabber_tmdb_add_person (fdata, node, tmdb_person_mapping[i].name);
        break;
      }

    xmlFree (ch);
    node = node->next;
  }

  /* fetch movie poster */
  tmp = get_prop_value_from_xml_tree_by_attr (n, "poster", "size", "cover");
  if (tmp)
  {
    grabber_tmdb_get_picture (fdata, keywords, tmp, VALHALLA_DL_COVER);
    xmlFree (tmp);
  }

  /* fetch movie backdrop */
  tmp = get_prop_value_from_xml_tree_by_attr (n, "backdrop", "size", "mid");
  if (tmp)
  {
    grabber_tmdb_get_picture (fdata, keywords, tmp, VALHALLA_DL_BACKDROP);
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

/*****************************************************************************/
/* Private Grabber API                                                       */
/*****************************************************************************/

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

  tmdb->handler = url_new ();
  return tmdb->handler ? 0 : -1;
}

static void
grabber_tmdb_uninit (void *priv)
{
  grabber_tmdb_t *tmdb = priv;

  valhalla_log (VALHALLA_MSG_VERBOSE, __FUNCTION__);

  if (!tmdb)
    return;

  url_free (tmdb->handler);
  list_free (tmdb->list, NULL);
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

  /* ensure we're handling a video file */
  if (data->type != VALHALLA_FILE_TYPE_VIDEO)
    return -1;

  /*
   * Try with the video's title and finally with the
   * filename if nothing better is available.
   */
  if (!metadata_get (data->meta_parser, "title", 0, &tag))
    keywords = tag->value;
  else
    /* FIXME: get keywords from filename (decrapifier ??) */
    return -1;

  /* Format the keywords */
  escaped_keywords = url_escape_string (tmdb->handler, keywords);
  if (!escaped_keywords)
    return -2;

  res = grabber_tmdb_get (tmdb->handler, data, keywords, escaped_keywords);
  free (escaped_keywords);

  return res;
}

static void
grabber_tmdb_loop (void *priv)
{
  grabber_tmdb_t *tmdb = priv;

  valhalla_log (VALHALLA_MSG_VERBOSE, __FUNCTION__);

  /* Hash cover list cleanup */
  LIST_FREE (tmdb->list, NULL);
}

/*****************************************************************************/
/* Public Grabber API                                                        */
/*****************************************************************************/

/* grabber_tmdb_register () */
GRABBER_REGISTER (tmdb,
                  GRABBER_CAP_FLAGS,
                  grabber_tmdb_priv,
                  grabber_tmdb_init,
                  grabber_tmdb_uninit,
                  grabber_tmdb_grab,
                  grabber_tmdb_loop)
