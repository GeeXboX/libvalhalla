/*
 * GeeXboX Valhalla: tiny media scanner API.
 * Copyright (C) 2016 Mathieu Schroeter <mathieu@schroetersa.ch>
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

#ifndef VALHALLA_JSON_UTILS_H
#define VALHALLA_JSON_UTILS_H

#include <json-c/json_tokener.h>

char *vh_json_get_str (json_object *json, const char *path);
int vh_json_get_int (json_object *json, const char *path);

#endif /* VALHALLA_JSON_UTILS_H */
