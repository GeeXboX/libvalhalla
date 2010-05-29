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

#include <pthread.h>
#include <string.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>

#include <curl/curl.h>
#include <curl/types.h>
#include <curl/easy.h>

#include "valhalla.h"
#include "valhalla_internals.h"
#include "url_utils.h"
#include "logs.h"


struct url_ctl_s {
  pthread_mutex_t mutex;
  int abort;
};

static size_t
url_buffer_get (void *ptr, size_t size, size_t nmemb, void *data)
{
  size_t realsize = size * nmemb;
  url_data_t *mem = (url_data_t *) data;

  mem->buffer = realloc (mem->buffer, mem->size + realsize + 1);
  if (mem->buffer)
  {
    memcpy (&(mem->buffer[mem->size]), ptr, realsize);
    mem->size += realsize;
    mem->buffer[mem->size] = 0;
  }

  return realsize;
}

static int
url_progress_cb (void *clientp,
                 vh_unused double dltotal, vh_unused double dlnow,
                 vh_unused double ultotal, vh_unused double ulnow)
{
  url_ctl_t *a = clientp;
  int abort;

  if (!a)
    return 0;

  pthread_mutex_lock (&a->mutex);
  abort = a->abort;
  pthread_mutex_unlock (&a->mutex);

  return abort;
}

url_t *
vh_url_new (url_ctl_t *url_ctl)
{
  char useragent[256];
  CURL *curl;

  curl = curl_easy_init ();
  if (!curl)
    return NULL;

  snprintf (useragent, sizeof (useragent),
            "libvalhalla/%s %s", LIBVALHALLA_VERSION_STR, curl_version ());

  curl_easy_setopt (curl, CURLOPT_FOLLOWLOCATION, 1);
  curl_easy_setopt (curl, CURLOPT_WRITEFUNCTION, url_buffer_get);
  curl_easy_setopt (curl, CURLOPT_NOSIGNAL, 1);
  curl_easy_setopt (curl, CURLOPT_TIMEOUT, 20);
  curl_easy_setopt (curl, CURLOPT_CONNECTTIMEOUT, 5);
  curl_easy_setopt (curl, CURLOPT_USERAGENT, useragent);
  curl_easy_setopt (curl, CURLOPT_FAILONERROR, 1);

  if (url_ctl)
  {
    /*
     * The progress callback provides a way to abort a download. A call on
     * vh_url_ctl_abort() with the same url_ctl, will break vh_url_get_data()
     * with the status code CURLE_ABORTED_BY_CALLBACK. The breaking is not
     * immediate. The callback is called roughly once per second.
     */
    curl_easy_setopt (curl, CURLOPT_NOPROGRESS, 0);
    curl_easy_setopt (curl, CURLOPT_PROGRESSDATA, url_ctl);
    curl_easy_setopt (curl, CURLOPT_PROGRESSFUNCTION, url_progress_cb);
  }

  return (url_t *) curl;
}

void
vh_url_free (url_t *url)
{
  if (url)
    curl_easy_cleanup ((CURL *) url);
}

void
vh_url_global_init (void)
{
  curl_global_init (CURL_GLOBAL_DEFAULT);
}

void
vh_url_global_uninit (void)
{
  curl_global_cleanup ();
}

url_data_t
vh_url_get_data (url_t *handler, char *url)
{
  url_data_t chunk;
  CURL *curl = (CURL *) handler;

  chunk.buffer = NULL; /* we expect realloc(NULL, size) to work */
  chunk.size = 0; /* no data at this point */
  chunk.status = CURLE_FAILED_INIT;

  if (!curl || !url)
    return chunk;

  curl_easy_setopt (curl, CURLOPT_URL, url);
  curl_easy_setopt (curl, CURLOPT_WRITEDATA, (void *) &chunk);

  chunk.status = curl_easy_perform (curl);
  if (chunk.status)
  {
    const char *err = curl_easy_strerror (chunk.status);
    vh_log (VALHALLA_MSG_VERBOSE, "%s: %s", __FUNCTION__, err);

    if (chunk.buffer)
    {
      free (chunk.buffer);
      chunk.buffer = NULL;
    }
  }

  return chunk;
}

char *
vh_url_escape_string (url_t *handler, const char *buf)
{
  CURL *curl = (CURL *) handler;

  if (!curl || !buf)
    return NULL;

  return curl_easy_escape (curl, buf, strlen (buf));
}

int
vh_url_save_to_disk (url_t *handler, char *src, char *dst)
{
  url_data_t data;
  int n, fd;
  CURL *curl = (CURL *) handler;

  if (!curl || !src || !dst)
    return URL_ERROR_PARAMS;

  vh_log (VALHALLA_MSG_VERBOSE, "Saving %s to %s", src, dst);

  data = vh_url_get_data (curl, src);
  if (data.status != CURLE_OK)
  {
    if (data.status == CURLE_ABORTED_BY_CALLBACK)
      return URL_ERROR_ABORT;

    vh_log (VALHALLA_MSG_WARNING, "Unable to download requested file %s", src);
    return URL_ERROR_TRANSFER;
  }

  fd = open (dst, O_WRONLY | O_CREAT | O_BINARY, 0666);
  if (fd < 0)
  {
    vh_log (VALHALLA_MSG_WARNING, "Unable to open stream to save file %s", dst);
    free (data.buffer);
    return URL_ERROR_FILE;
  }

  n = write (fd, data.buffer, data.size);
  close (fd);
  free (data.buffer);

  return URL_SUCCESS;
}

url_ctl_t *
vh_url_ctl_new (void)
{
  url_ctl_t *url_ctl;

  url_ctl = calloc (1, sizeof (url_ctl_t));
  if (!url_ctl)
    return NULL;

  pthread_mutex_init (&url_ctl->mutex, NULL);
  return url_ctl;
}

void
vh_url_ctl_free (url_ctl_t *url_ctl)
{
  if (!url_ctl)
    return;

  pthread_mutex_destroy (&url_ctl->mutex);
  free (url_ctl);
}

void
vh_url_ctl_abort (url_ctl_t *url_ctl)
{
  if (!url_ctl)
    return;

  pthread_mutex_lock (&url_ctl->mutex);
  url_ctl->abort = 1;
  pthread_mutex_unlock (&url_ctl->mutex);
}

