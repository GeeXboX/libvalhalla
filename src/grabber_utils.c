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
#include <string.h>

#include "utils.h"
#include "xml_utils.h"

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
grabber_parse_str (file_data_t *fdata, xmlNode *nd, const char *tag,
                   const char *name, valhalla_meta_grp_t group)
{
  char *res = NULL;

  if (!fdata || !nd || !tag || !name)
    return;

  xml_search_str (nd, tag, &res);
  if (res)
  {
    metadata_add (&fdata->meta_grabber, name, res, group);
    free (res);
    res = NULL;
  }
}

void
grabber_parse_int (file_data_t *fdata, int val,
                   const char *name, valhalla_meta_grp_t group)
{
  char v[32] = { 0 };

  snprintf (v, sizeof (v), "%d", val);
  metadata_add (&fdata->meta_grabber, name,
                v, VALHALLA_META_GRP_TEMPORAL);
}

void
grabber_parse_categories (file_data_t *fdata, xmlNode *node)
{
  xmlNode *n;
  int i;

  if (!fdata || !node)
    return;

  n = get_node_xml_tree (node, "category");
  for (i = 0; i < 5; i++)
  {
    xmlChar *tmp = NULL;

    if (!n)
      break;

    tmp = get_prop_value_from_xml_tree (n, "name");
    if (tmp)
    {
      metadata_add (&fdata->meta_grabber, "category",
                    (char *) tmp, VALHALLA_META_GRP_CLASSIFICATION);
      xmlFree (tmp);
    }
    n = n->next;
  }
}

static void
grabber_add_person (file_data_t *fdata,
                    xmlNode *node, const char *cat)
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

void
grabber_parse_casting (file_data_t *fdata, xmlNode *node)
{
  xmlNode *n;

  if (!fdata || !node)
    return;

  n = get_node_xml_tree (node, "person");
  while (n)
  {
    int i;

    xmlChar *ch;

    ch = get_attr_value_from_node (n, "job");
    if (!ch)
    {
      n = n->next;
      continue;
    }

    for (i = 0; grabber_casting_mapping[i].tag; i++)
      if (!strcmp ((char *) ch, grabber_casting_mapping[i].tag))
      {
        grabber_add_person (fdata, n, grabber_casting_mapping[i].name);
        break;
      }

    xmlFree (ch);
    n = n->next;
  }
}
