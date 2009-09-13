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
 *
 * This file incorporates work covered by the following copyright and
 * permission notice:
 *
 *   Copyright (C) 2005-2009 The Enna Project
 *
 *   Permission is hereby granted, free of charge, to any person obtaining a
 *   copy of this software and associated documentation files (the "Software"),
 *   to deal in the Software without restriction, including without limitation
 *   the rights to use, copy, modify, merge, publish, distribute, sublicense,
 *   and/or sell copies of the Software, and to permit persons to whom the
 *   Software is furnished to do so, subject to the following conditions:
 *
 *   The above copyright notice and this permission notice shall be included in
 *   all copies of the Software and its Copyright notices. In addition publicly
 *   documented acknowledgment must be given that this software has been used if
 *   no source code of this software is made available publicly. This includes
 *   acknowledgments in either Copyright notices, Manuals, Publicity and
 *   Marketing documents or any documentation provided with any product
 *   containing this software. This License does not apply to any software that
 *   links to the libraries provided by this software (statically or
 *   dynamically), but only to the software provided.
 *
 *   THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 *   IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 *   FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 *   THE AUTHORS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
 *   IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 *   CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

#include <string.h>

#include "xml_utils.h"


xmlDocPtr
vh_get_xml_doc_from_memory (char *buffer)
{
  xmlDocPtr doc = NULL;

  if (!buffer)
    return NULL;

  doc = xmlReadMemory (buffer, strlen (buffer), NULL, NULL,
                       XML_PARSE_RECOVER |
                       XML_PARSE_NOERROR | XML_PARSE_NOWARNING);
  return doc;
}

xmlNode *
vh_get_node_xml_tree (xmlNode *root, const char *prop)
{
  xmlNode *n, *children_node;

  for (n = root; n; n = n->next)
  {
    if (n->type == XML_ELEMENT_NODE
        && !xmlStrcmp ((unsigned char *) prop, n->name))
      return n;

    children_node = vh_get_node_xml_tree (n->children, prop);
    if (children_node)
      return children_node;
  }

  return NULL;
}

xmlChar *
vh_get_prop_value_from_xml_tree (xmlNode *root, const char *prop)
{
  xmlNode *node;

  node = vh_get_node_xml_tree (root, prop);
  if (!node)
    return NULL;

  return xmlNodeGetContent (node);
}

xmlChar *
vh_get_prop_value_from_xml_tree_by_attr (xmlNode *root, const char *prop,
                                         const char *attr_name,
                                         const char *attr_value)
{
  xmlNode *n, *node;
  xmlAttr *attr;

  node = vh_get_node_xml_tree (root, prop);
  if (!node)
    return NULL;

  for (n = node; n; n = n->next)
  {
    xmlChar *content;

    attr = n->properties;
    if (!attr || !attr->children)
      continue;

    if (xmlStrcmp ((unsigned char *) attr_name, attr->name))
      continue;

    content = xmlNodeGetContent (attr->children);
    if (!content)
      continue;

    if (xmlStrcmp ((unsigned char *) attr_value, content))
    {
      xmlFree (content);
      continue;
    }

    xmlFree (content);
    return xmlNodeGetContent (n);
  }

  return NULL;
}

xmlChar *
vh_get_attr_value_from_xml_tree (xmlNode *root,
                                 const char *prop, const char *attr_name)
{
  xmlNode *n, *node;
  xmlAttr *attr;

  node = vh_get_node_xml_tree (root, prop);
  if (!node)
    return NULL;

  for (n = node; n; n = n->next)
  {
    xmlChar *content;

    attr = n->properties;
    if (!attr || !attr->children)
      continue;

    if (xmlStrcmp ((unsigned char *) attr_name, attr->name))
      continue;

    content = xmlNodeGetContent (attr->children);
    if (content)
      return content;
  }

  return NULL;
}

xmlChar *
vh_get_attr_value_from_node (xmlNode *node, const char *attr_name)
{
  xmlNode *n;
  xmlAttr *attr;

  if (!node || !attr_name)
    return NULL;

  for (n = node; n; n = n->next)
  {
    xmlChar *content;

    attr = n->properties;
    if (!attr || !attr->children)
      continue;

    if (xmlStrcmp ((unsigned char *) attr_name, attr->name))
      continue;

    content = xmlNodeGetContent (attr->children);
    if (content)
      return content;
  }

  return NULL;
}

xmlXPathObjectPtr
vh_get_xnodes_from_xml_tree (xmlDocPtr doc, xmlChar *xpath)
{
  xmlXPathContextPtr context;
  xmlXPathObjectPtr result;

  context = xmlXPathNewContext (doc);

  if (!context)
    return NULL;

  result = xmlXPathEvalExpression (xpath, context);
  xmlXPathFreeContext (context);

  if (!result)
    return NULL;

  if (xmlXPathNodeSetIsEmpty (result->nodesetval))
  {
    xmlXPathFreeObject (result);
    return NULL;
  }

  return result;
}

int
vh_xml_search_str (xmlNode *n, const char *node, char **str)
{
  xmlChar *tmp;

  if (*str)
    return 1;

  tmp = vh_get_prop_value_from_xml_tree (n, node);
  if (!tmp)
    return 1;

  *str = strdup ((char *) tmp);
  xmlFree (tmp);

  return 0;
}

int
vh_xml_search_int (xmlNode *n, const char *node, int *val)
{
  xmlChar *tmp;

  if (*val)
    return 1;

  tmp = vh_get_prop_value_from_xml_tree (n, node);
  if (!tmp)
    return 1;

  *val = atoi ((char *) tmp);
  xmlFree (tmp);

  return 0;
}

int
vh_xml_search_year (xmlNode *n, const char *node, int *year)
{
  xmlChar *tmp;
  int r, y, m, d;

  if (*year)
    return 1;

  tmp = vh_get_prop_value_from_xml_tree (n, node);
  if (!tmp)
    return 1;

  r = sscanf ((char *) tmp, "%i-%i-%i", &y, &m, &d);
  xmlFree (tmp);
  if (r == 3)
  {
    *year = y;
    return 0;
  }

  return 1;
}
