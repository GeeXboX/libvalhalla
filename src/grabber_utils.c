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
  vh_metadata_add_auto (&fdata->meta_grabber, name, v, pl);
}

void
vh_grabber_parse_int64 (file_data_t *fdata, int64_t val,
                        const char *name, const metadata_plist_t *pl)
{
  char v[32] = { 0 };

  snprintf (v, sizeof (v), "%"PRIi64, val);
  vh_metadata_add_auto (&fdata->meta_grabber, name, v, pl);
}

void
vh_grabber_parse_float (file_data_t *fdata, float val,
                        const char *name, const metadata_plist_t *pl)
{
  char v[32] = { 0 };

  snprintf (v, sizeof (v), "%.5f", val);
  vh_metadata_add_auto (&fdata->meta_grabber, name, v, pl);
}

#ifdef USE_XML
void
vh_grabber_parse_str (file_data_t *fdata, xmlNode *nd, const char *tag,
                      const char *name, const metadata_plist_t *pl)
{
  char *res = NULL;

  if (!fdata || !nd || !tag || !name)
    return;

  vh_xml_search_str (nd, tag, &res);
  if (res)
  {
    vh_metadata_add_auto (&fdata->meta_grabber, name, res, pl);
    free (res);
    res = NULL;
  }
}

void
vh_grabber_parse_categories (file_data_t *fdata,
                             xmlNode *node, const metadata_plist_t *pl)
{
  xmlNode *n;
  int i;

  if (!fdata || !node)
    return;

  n = vh_get_node_xml_tree (node, "category");
  for (i = 0; i < 5; i++)
  {
    xmlChar *tmp = NULL;

    if (!n)
      break;

    tmp = vh_get_prop_value_from_xml_tree (n, "name");
    if (tmp)
    {
      vh_metadata_add_auto (&fdata->meta_grabber,
                            VALHALLA_METADATA_CATEGORY, (char *) tmp, pl);
      xmlFree (tmp);
    }
    n = n->next;
  }
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

  if (name)
  {
    char str[128] = { 0 };

    if (role && *role)
      snprintf (str, sizeof (str), "%s (%s)", name, role);
    else
      snprintf (str, sizeof (str), "%s", name);
    free (name);
    vh_metadata_add_auto (&fdata->meta_grabber, cat, str, pl);
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

  n = vh_get_node_xml_tree (node, "person");
  for (; n; n = n->next)
  {
    int i;

    xmlChar *ch;

    ch = vh_get_attr_value_from_node (n, "job");
    if (!ch)
      continue;

    for (i = 0; grabber_casting_mapping[i].tag; i++)
      if (!strcmp ((char *) ch, grabber_casting_mapping[i].tag))
      {
        grabber_add_person (fdata, n, grabber_casting_mapping[i].name, pl);
        break;
      }

    xmlFree (ch);
  }
}
#endif /* USE_XML */
