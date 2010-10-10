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

#ifndef VALHALLA_XML_UTILS_H
#define VALHALLA_XML_UTILS_H

#include <libxml/parser.h>
#include <libxml/tree.h>
#include <libxml/xpath.h>

xmlDocPtr vh_xml_get_doc_from_memory (char *buffer);
xmlNode *vh_xml_get_node_tree (xmlNode *root, const char *prop);
xmlChar *vh_xml_get_prop_value_from_tree (xmlNode *root, const char *prop);
xmlChar *vh_xml_get_prop_value_from_tree_by_attr (xmlNode *root,
                                                  const char *prop,
                                                  const char *attr_name,
                                                  const char *attr_value);
xmlChar *vh_xml_get_attr_value_from_tree (xmlNode *root,
                                          const char *prop,
                                          const char *attr_name);
xmlChar *vh_xml_get_attr_value_from_node (xmlNode *node, const char *attr_name);
xmlXPathObjectPtr vh_xml_get_xnodes_from_tree (xmlDocPtr doc, xmlChar *xpath);
int vh_xml_search_str (xmlNode *n, const char *node, char **str);
int vh_xml_search_int (xmlNode *n, const char *node, int *val);
int vh_xml_search_year (xmlNode *n, const char *node, int *year);

#endif /* VALHALLA_XML_UTILS_H */
