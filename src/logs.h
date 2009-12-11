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
 */

#ifndef VALHALLA_LOGS_H
#define VALHALLA_LOGS_H

void vh_log_verb (valhalla_verb_t level);
int vh_log_test (valhalla_verb_t level);
void vh_log_orig (valhalla_verb_t level,
                  const char *file, int line, const char *format, ...);

#define vh_log(level, format, arg...) \
  vh_log_orig (level, __FILE__, __LINE__, format, ##arg)

#endif /* VALHALLA_LOGS_H */
