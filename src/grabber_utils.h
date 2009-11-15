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

#ifndef VALHALLA_GRABBER_UTILS_H
#define VALHALLA_GRABBER_UTILS_H

#ifdef USE_XML
#include "xml_utils.h"
#endif /* USE_XML */

void vh_grabber_parse_int (file_data_t *fdata, int val, const char *name);
void vh_grabber_parse_float (file_data_t *fdata, float val, const char *name);

#ifdef USE_XML
void vh_grabber_parse_str (file_data_t *fdata,
                           xmlNode *nd, const char *tag, const char *name);
void vh_grabber_parse_categories (file_data_t *fdata, xmlNode *node);
void vh_grabber_parse_casting (file_data_t *fdata, xmlNode *node);
#endif /* USE_XML */

#endif /* VALHALLA_GRABBER_UTILS_H */
