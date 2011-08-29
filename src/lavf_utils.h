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

#ifndef VALHALLA_LAVF_UTILS
#define VALHALLA_LAVF_UTILS

#if LIBAVCODEC_VERSION_INT < AV_VERSION_INT(52, 64, 0)
#define AVMEDIA_TYPE_AUDIO    CODEC_TYPE_AUDIO
#define AVMEDIA_TYPE_VIDEO    CODEC_TYPE_VIDEO
#define AVMEDIA_TYPE_SUBTITLE CODEC_TYPE_SUBTITLE
#endif /* LIBAVCODEC_VERSION_INT < AV_VERSION_INT(52, 64, 0) */

const char *vh_lavf_utils_fmtname_get (const char *suffix);
AVFormatContext *vh_lavf_utils_open_input_file (const char *file);

#endif /* VALHALLA_LAVF_UTILS */
