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

#ifndef VALHALLA_GRABBER_COMMON_H
#define VALHALLA_GRABBER_COMMON_H

/**
 * \file grabber_common.h
 *
 * GeeXboX Valhalla Grabber private API header.
 *
 * To add a new grabber, a good approach is to copy an existing grabber like
 * grabber_dummy.[ch] in order to have at least the structure. A grabber must
 * not use global/static variables. A grabber must be thread-safe in the case
 * where more than one instance of Valhalla are running concurrently. But, the
 * functions in one grabber are not called in concurrency in one instance
 * of Valhalla.
 *
 * Some others points to consider:
 *  - grabber_list_t::init() and grabber_list_t::uninit() functions are called
 *    only one time by a Valhalla instance.
 *  - grabber_list_t::grab() function is called only between the
 *    grabber_list_t::init() and grabber_list_t::uninit() functions.
 *  - grabber_list_t::loop() function is called only between two scanner loops.
 *    The function is not called if only one loop is configured for the instance
 *    of Valhalla.
 *
 * Some utils are available for the grabbers:
 *  - grabber_utils.h  utils specific to grabbers
 *  - xml_utils.h      for XML parsing (based on libxml2)
 *  - url_utils.h      for downloading (based on libcurl)
 *  - logs.h           for logging
 *  - md5.h            to compute the MD5 sum
 *  - list.h           to handle very simple linked-lists
 *
 * Main header for all grabbers:
 *  - grabber_common.h
 *    - metadata.h to save metadata retrieved by grabber_list_t::grab()
 *    - utils.h    to prepare files (images, ...) for downloading
 *
 * \see grabber_list_t for details on the functions.
 */

#include <pthread.h>
#include <string.h>

#include "stats.h"
#include "utils.h"

/** \name Flags for the capabilities of the grabbers.
 * @{
 */
#define GRABBER_CAP_AUDIO  (1 << 0) /**< \brief grab for audio files */
#define GRABBER_CAP_VIDEO  (1 << 1) /**< \brief grab for video files */
#define GRABBER_CAP_IMAGE  (1 << 2) /**< \brief grab for image files */
/**
 *@}
 */

/**
 * \brief Structure for the init of a grabber.
 */
typedef struct grabber_param_s {
  /** \brief List of priorities for metadata. */
  metadata_plist_t *pl;
  /** \brief This pointer is intended to be used with all vh_url_new(). */
  struct url_ctl_s *url_ctl;
} grabber_param_t;

/**
 * \brief Structure for a grabber.
 */
typedef struct grabber_list_s {
  struct grabber_list_s *next;

  /** \brief Textual identification of the grabber. */
  const char *name;
  /** \brief Flags to define the capabilities of the grabber. */
  int caps_flag;

  /**
   * \brief Init function for the grabber.
   *
   * This initialization is called only at the init of an instance of valhalla.
   * The private structure (\p priv) must be created before this initialization.
   *
   * \param[in] priv      Private structure registered with the grabber.
   * \param[in] param     Parameters, see ::grabber_param_t.
   * \return 0 for success, != 0 on error.
   */
  int (*init) (void *priv, const grabber_param_t *param);

  /**
   * \brief Uninit function for the grabber.
   *
   * This unititialization is called only at the uninit of an instance of
   * valhalla. The private structure (\p priv) must be released in this
   * function.
   *
   * \param[in] priv      Private structure registered with the grabber.
   */
  void (*uninit) (void *priv);

  /**
   * \brief Grabbing function for the grabber.
   *
   * This function is called in order to populate the attributes \p meta_grabber
   * and \p list_downloader in the \p data structure. All others attributes must
   * be considered as read-only! Only them are thread-safe for writing.
   *
   * To add new metadata in the database, the function vh_metadata_add() must be
   * used on \p meta_grabber.
   *
   * It is proibited to download files (images for example) with this function.
   * Only textual metadata are proceeded here. But the reference on an image
   * can be saved in the meta_grabber attribute.
   * To download a file, the URL and the destination must be prepared for the
   * downloader with the function vh_file_dl_add() in order to populate the
   * \p list_downloader attribute. The files will be downloaded after the
   * grabbing step.
   *
   * To read \p meta_parser (attribute populated by the parser), you must use
   * the function vh_metadata_get().
   *
   * \param[in] priv      Private structure registered with the grabber.
   * \param[in] data      File structure where some data must be populated.
   * \return 0 for success, != 0 on error.
   */
  int (*grab) (void *priv, file_data_t *data);

  /**
   * \brief Function called for each end of scan loop.
   *
   * This function is optional, it is called after each scanner loop if there
   * are more than one loop. This function is never called after the last loop.
   * It is useful to make some cleanup in the grabber before the next scan.
   *
   * \param[in] priv      Private structure registered with the grabber.
   */
  void (*loop) (void *priv);

  /**
   * \brief Private data for the grabber.
   *
   * The data is registered at the same time that the grabber.
   */
  void *priv;

  /** \brief Parameters for the grabber. */
  grabber_param_t param;

  /** \private Different of 0 if the grabber is enabled. */
  int enable;

  /** \private Minimum time to wait before the next grab(). */
  uint64_t timewait;
  /** \private Time when the grab has finished. */
  uint64_t timegrab;

  /** \private Prevent races with several grabbers. */
  pthread_mutex_t mutex;

  /** \private Timer for statistics. */
  vh_stats_tmr_t *tmr;
  /** \private Counter for statistics (::grab() returns 0). */
  vh_stats_cnt_t *cnt_success;
  /** \private Counter for statistics (::grab() returns != 0). */
  vh_stats_cnt_t *cnt_failure;
  /** \private Counter for statistics when the grabber has been skipped. */
  vh_stats_cnt_t *cnt_skip;

} grabber_list_t;


/**
 * \brief Macro to register and populate a grabber structure.
 *
 * See struct grabber_list_s for more informations on the structure and
 * the functions.
 *
 * \param[in] p_name      Grabber's name.
 * \param[in] p_caps      Capabilities flags.
 * \param[in] p_pl        List of metadata priorities.
 * \param[in] p_tw        Min time to wait [ms] between grabber_list_t::grab().
 * \param[in] fct_priv    Function to retrieve the private data pointer.
 * \param[in] fct_init    grabber_list_t::init().
 * \param[in] fct_uninit  grabber_list_t::uninit().
 * \param[in] fct_grab    grabber_list_t::grab().
 * \param[in] fct_loop    grabber_list_t::loop().
 */
#define GRABBER_REGISTER(p_name, p_caps, p_pl, p_tw,                          \
                         fct_priv, fct_init, fct_uninit, fct_grab, fct_loop)  \
  grabber_list_t *                                                            \
  vh_grabber_##p_name##_register (struct url_ctl_s *url_ctl)                  \
  {                                                                           \
    grabber_list_t *grabber;                                                  \
                                                                              \
    vh_log (VALHALLA_MSG_VERBOSE, __FUNCTION__);                              \
                                                                              \
    grabber = calloc (1, sizeof (grabber_list_t));                            \
    if (!grabber)                                                             \
      return NULL;                                                            \
                                                                              \
    grabber->name      = #p_name;                                             \
    grabber->caps_flag = p_caps;                                              \
    grabber->enable    = 1;                                                   \
    grabber->timewait  = p_tw * 1000000UL;                                    \
    grabber->priv      = fct_priv ();                                         \
                                                                              \
    grabber->init      = fct_init;                                            \
    grabber->uninit    = fct_uninit;                                          \
    grabber->grab      = fct_grab;                                            \
    grabber->loop      = fct_loop;                                            \
                                                                              \
    grabber->param.url_ctl = url_ctl;                                         \
    grabber->param.pl = malloc (sizeof (p_pl));                               \
    if (!grabber->param.pl)                                                   \
    {                                                                         \
      free (grabber);                                                         \
      return NULL;                                                            \
    }                                                                         \
    memcpy (grabber->param.pl, p_pl, sizeof (p_pl));                          \
                                                                              \
    pthread_mutex_init (&grabber->mutex, NULL);                               \
                                                                              \
    return grabber;                                                           \
  }

#endif /* VALHALLA_GRABBER_COMMON_H */
