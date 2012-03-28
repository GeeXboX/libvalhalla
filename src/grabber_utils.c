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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "utils.h"
#ifdef USE_XML
#include "xml_utils.h"
#endif /* USE_XML */

static const struct {
  const char *tag;
  const char *name;
} grabber_casting_mapping[] = {
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


void
vh_grabber_parse_int (file_data_t *fdata, int val,
                      const char *name, const metadata_plist_t *pl)
{
  char v[32] = { 0 };

  snprintf (v, sizeof (v), "%d", val);
  vh_metadata_add_auto (&fdata->meta_grabber, name, v, VALHALLA_LANG_UNDEF, pl);
}

void
vh_grabber_parse_int64 (file_data_t *fdata, int64_t val,
                        const char *name, const metadata_plist_t *pl)
{
  char v[32] = { 0 };

  snprintf (v, sizeof (v), "%"PRIi64, val);
  vh_metadata_add_auto (&fdata->meta_grabber, name, v, VALHALLA_LANG_UNDEF, pl);
}

void
vh_grabber_parse_float (file_data_t *fdata, float val,
                        const char *name, const metadata_plist_t *pl)
{
  char v[32] = { 0 };

  snprintf (v, sizeof (v), "%.5f", val);
  vh_metadata_add_auto (&fdata->meta_grabber, name, v, VALHALLA_LANG_UNDEF, pl);
}

#ifdef USE_XML
void
vh_grabber_parse_str (file_data_t *fdata, xmlNode *nd, const char *tag,
                      const char *name, valhalla_lang_t lang,
                      const metadata_plist_t *pl)
{
  char *res = NULL;

  if (!fdata || !nd || !tag || !name)
    return;

  vh_xml_search_str (nd, tag, &res);
  if (res)
  {
    vh_metadata_add_auto (&fdata->meta_grabber, name, res, lang, pl);
    free (res);
    res = NULL;
  }
}

static void
grabber_parse_list (file_data_t *fdata, xmlNode *node,
                    const char *node_name, const char *meta,
                    valhalla_lang_t lang, const metadata_plist_t *pl)
{
  xmlNode *n;
  int i;

  if (!fdata || !node)
    return;

  n = vh_xml_get_node_tree (node, node_name);
  for (i = 0; i < 5; i++, n = n->next)
  {
    xmlChar *tmp = NULL;

    if (!n)
      break;

    switch (n->type)
    {
    case XML_TEXT_NODE:
      tmp = vh_xml_get_prop_value_from_tree (n, "name");
      break;

    case XML_ELEMENT_NODE:
      tmp = vh_xml_get_attr_value_from_node (n, "name");
      break;

    default:
      break;
    }

    /* we retry with the next because it depends of n->type */
    if (!tmp)
    {
      --i;
      continue;
    }

      vh_metadata_add_auto (&fdata->meta_grabber, meta, (char *) tmp, lang, pl);
      xmlFree (tmp);
  }
}

void
vh_grabber_parse_categories (file_data_t *fdata, xmlNode *node,
                             valhalla_lang_t lang, const metadata_plist_t *pl)
{
  grabber_parse_list (fdata, node,
                      "category", VALHALLA_METADATA_CATEGORY, lang, pl);
}

void
vh_grabber_parse_countries (file_data_t *fdata, xmlNode *node,
                            valhalla_lang_t lang, const metadata_plist_t *pl)
{
  grabber_parse_list (fdata, node,
                      "country", VALHALLA_METADATA_COUNTRY, lang, pl);
}

static void
grabber_add_person (file_data_t *fdata,
                    xmlNode *node, const char *cat, const metadata_plist_t *pl)
{
  char *name = NULL, *role = NULL;

  if (!fdata || !node || !cat)
    return;

  vh_xml_search_str (node, "name", &name);
  vh_xml_search_str (node, "role", &role);
  if (!role)
    vh_xml_search_str (node, "character", &role);

  if (name)
  {
    char str[128] = { 0 };

    if (role && *role)
      snprintf (str, sizeof (str), "%s (%s)", name, role);
    else
      snprintf (str, sizeof (str), "%s", name);
    free (name);
    vh_metadata_add_auto (&fdata->meta_grabber,
                          cat, str, VALHALLA_LANG_UNDEF, pl);
  }

  if (role)
    free (role);
}

void
vh_grabber_parse_casting (file_data_t *fdata,
                          xmlNode *node, const metadata_plist_t *pl)
{
  xmlNode *n;

  if (!fdata || !node)
    return;

  n = vh_xml_get_node_tree (node, "person");
  for (; n; n = n->next)
  {
    int i;

    xmlChar *ch;

    ch = vh_xml_get_attr_value_from_node (n, "job");
    if (!ch)
      continue;

    for (i = 0; grabber_casting_mapping[i].tag; i++)
      if (!strcasecmp ((char *) ch, grabber_casting_mapping[i].tag))
      {
        grabber_add_person (fdata, n, grabber_casting_mapping[i].name, pl);
        break;
      }

    xmlFree (ch);
  }
}
#endif /* USE_XML */
