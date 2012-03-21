/*
 * GeeXboX Valhalla: tiny media scanner API.
 * Copyright (C) 2009-2012 Mathieu Schroeter <mathieu@schroetersa.ch>
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

#include <string.h>

#include "xml_utils.h"


xmlDocPtr
vh_xml_get_doc_from_memory (char *buffer)
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
vh_xml_get_node_tree (xmlNode *root, const char *prop)
{
  xmlNode *n, *children_node;

  for (n = root; n; n = n->next)
  {
    if (n->type == XML_ELEMENT_NODE
        && !xmlStrcmp ((unsigned char *) prop, n->name))
      return n;

    children_node = vh_xml_get_node_tree (n->children, prop);
    if (children_node)
      return children_node;
  }

  return NULL;
}

xmlChar *
vh_xml_get_prop_value_from_tree (xmlNode *root, const char *prop)
{
  xmlNode *node;

  node = vh_xml_get_node_tree (root, prop);
  if (!node)
    return NULL;

  return xmlNodeGetContent (node);
}

xmlChar *
vh_xml_get_prop_value_from_tree_by_attr (xmlNode *root, const char *prop,
                                         const char *attr_name,
                                         const char *attr_value)
{
  xmlNode *n, *node;
  xmlAttr *attr;

  node = vh_xml_get_node_tree (root, prop);
  if (!node)
    return NULL;

  for (n = node; n; n = n->next)
  {
    xmlChar *content;

    attr = n->properties;
    if (!attr || !attr->children)
      continue;

    for (; attr; attr = attr->next)
      if (!xmlStrcmp ((unsigned char *) attr_name, attr->name))
        break;

    if (!attr)
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
vh_xml_get_attr_value_from_tree (xmlNode *root,
                                 const char *prop, const char *attr_name)
{
  xmlNode *node;

  node = vh_xml_get_node_tree (root, prop);
  if (!node)
    return NULL;

  return vh_xml_get_attr_value_from_node (node, attr_name);
}

xmlChar *
vh_xml_get_attr_value_from_node (xmlNode *node, const char *attr_name)
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

    for (; attr; attr = attr->next)
      if (!xmlStrcmp ((unsigned char *) attr_name, attr->name))
      {
        content = xmlNodeGetContent (attr->children);
        if (content)
          return content;
        break;
      }
  }

  return NULL;
}

xmlXPathObjectPtr
vh_xml_get_xnodes_from_tree (xmlDocPtr doc, xmlChar *xpath)
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

  tmp = vh_xml_get_prop_value_from_tree (n, node);
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

  tmp = vh_xml_get_prop_value_from_tree (n, node);
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

  tmp = vh_xml_get_prop_value_from_tree (n, node);
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
