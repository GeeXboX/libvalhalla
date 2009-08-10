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
 * Foundation, Inc, 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */

#ifndef VALHALLA_H
#define VALHALLA_H

/**
 * \file valhalla.h
 *
 * GeeXboX Valhalla public API header.
 */

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#define VH_STRINGIFY(s) #s
#define VH_TOSTRING(s) VH_STRINGIFY(s)

#define VH_VERSION_INT(a, b, c) (a << 16 | b << 8 | c)
#define VH_VERSION_DOT(a, b, c) a ##.## b ##.## c
#define VH_VERSION(a, b, c) VH_VERSION_DOT(a, b, c)

#define LIBVALHALLA_VERSION_MAJOR  0
#define LIBVALHALLA_VERSION_MINOR  0
#define LIBVALHALLA_VERSION_MICRO  1

#define LIBVALHALLA_VERSION_INT VH_VERSION_INT(LIBVALHALLA_VERSION_MAJOR, \
                                               LIBVALHALLA_VERSION_MINOR, \
                                               LIBVALHALLA_VERSION_MICRO)
#define LIBVALHALLA_VERSION     VH_VERSION(LIBVALHALLA_VERSION_MAJOR, \
                                           LIBVALHALLA_VERSION_MINOR, \
                                           LIBVALHALLA_VERSION_MICRO)
#define LIBVALHALLA_VERSION_STR VH_TOSTRING(LIBVALHALLA_VERSION)
#define LIBVALHALLA_BUILD       LIBVALHALLA_VERSION_INT

#include <inttypes.h>


/******************************************************************************/
/*                                                                            */
/* Valhalla Handling                                                          */
/*                                                                            */
/******************************************************************************/

/** \brief Scanner handle. */
typedef struct valhalla_s valhalla_t;

/** \brief Error code returned by valhalla_run(). */
enum valhalla_errno {
  VALHALLA_ERROR_DEAD    = -4,
  VALHALLA_ERROR_PATH    = -3,
  VALHALLA_ERROR_HANDLER = -2,
  VALHALLA_ERROR_THREAD  = -1,
  VALHALLA_SUCCESS       =  0,
};

/** \brief Verbosity level. */
typedef enum valhalla_verb {
  VALHALLA_MSG_NONE,     /**< No error messages.                        */
  VALHALLA_MSG_VERBOSE,  /**< Super-verbose mode: mostly for debugging. */
  VALHALLA_MSG_INFO,     /**< Working operations.                       */
  VALHALLA_MSG_WARNING,  /**< Harmless failures.                        */
  VALHALLA_MSG_ERROR,    /**< May result in hazardous behavior.         */
  VALHALLA_MSG_CRITICAL, /**< Prevents lib from working.                */
} valhalla_verb_t;

/**
 * \name Valhalla Handling.
 * @{
 */

/**
 * \brief Init a scanner and the database.
 *
 * If a database already exists, then it is used. Otherwise, a new database
 * is created to \p db. If more than one handles are created, you can't use
 * the same database. You must specify a different \p db for each handle.
 *
 * Several parsers (\p parser_nb) for metadata can be created in parallel.
 *
 * The interval for \p commit_int is the number of files to be inserted or
 * updated in one pass. A value between 100 and 200 is a good choice. If the
 * value is <=0, then the default interval is used (128).
 *
 * \param[in] db          Path on the database.
 * \param[in] parser_nb   Number of parsers to create.
 * \param[in] commit_int  File Interval between database commits.
 * \return The handle.
 */
valhalla_t *valhalla_init (const char *db,
                           unsigned int parser_nb, unsigned int commit_int);

/**
 * \brief Uninit an handle.
 *
 * If a scanner is running, this function stops immediatly all tasks before
 * releasing all elements.
 *
 * \param[in] handle      Handle on the scanner.
 */
void valhalla_uninit (valhalla_t *handle);

/**
 * \brief Change verbosity level.
 *
 * Default value is VALHALLA_MSG_INFO.
 *
 * \warning This function can be called in anytime.
 * \param[in] level       Level provided by valhalla_verb_t.
 */
void valhalla_verbosity (valhalla_verb_t level);

/**
 * \brief Add a path to the scanner.
 *
 * At least one path must be added to the scanner, otherwise an error is
 * returned by valhalla_run().
 *
 * \warning This function must be called before valhalla_run()!
 * \param[in] handle      Handle on the scanner.
 * \param[in] location    The path to be scanned.
 * \param[in] recursive   1 to scan all folders recursively, 0 otherwise.
 */
void valhalla_path_add (valhalla_t *handle,
                        const char *location, int recursive);

/**
 * \brief Add a file suffix for the scanner.
 *
 * If no suffix is added to the scanner, then all files will be parsed by
 * FFmpeg without exception and it can be very slow. It is highly recommanded
 * to always set at least one suffix (file extension)!
 *
 * \warning This function must be called before valhalla_run()!
 * \param[in] handle      Handle on the scanner.
 * \param[in] suffix      File suffix to add.
 */
void valhalla_suffix_add (valhalla_t *handle, const char *suffix);

/**
 * \brief Run the scanner, the database manager and all parsers.
 *
 * The \p priority can be set to all thread especially to run the system
 * in background with less priority. In the case of a user, you can change
 * only for a lower priority.
 *  -  0 : normal priority (default)
 *  -  1 to  19 : lower priorities
 *  - -1 to -20 : higher priorities
 *
 * \param[in] handle      Handle on the scanner.
 * \param[in] loop        Number of loops (<=0 for infinite).
 * \param[in] timeout     Timeout between loops, 0 to disable [seconds].
 * \param[in] priority    Priority set to all threads.
 * \return 0 for success and <0 on error (\see enum valhalla_errno).
 */
int valhalla_run (valhalla_t *handle, int loop, uint16_t timeout, int priority);

/**
 * \brief Wait until the scanning is finished.
 *
 * This function wait until the scanning is finished for all loops. If the
 * number of loops is infinite, then this function will wait forever. You
 * must not break this function with valhalla_uninit(), that is not safe!
 * If you prefer stop the scanner even if it is not finished. In this case
 * you must use _only_ valhalla_uninit().
 *
 * \warning This function can be used only after valhalla_run()!
 * \param[in] handle      Handle on the scanner.
 */
void valhalla_wait (valhalla_t *handle);

/**
 * @}
 */

/******************************************************************************/
/*                                                                            */
/* Database Selections                                                        */
/*                                                                            */
/******************************************************************************/

typedef enum valhalla_file_type {
  VALHALLA_FILE_TYPE_NULL = 0,
  VALHALLA_FILE_TYPE_AUDIO,
  VALHALLA_FILE_TYPE_IMAGE,
  VALHALLA_FILE_TYPE_PLAYLIST,
  VALHALLA_FILE_TYPE_VIDEO,
} valhalla_file_type_t;

/** \brief Groups for metadata. */
typedef enum valhalla_meta_grp {
  /**
   * NULL value for a group attribution.
   */
  VALHALLA_META_GRP_NIL = 0,

  /**
   * genre, mood, subject, synopsis, summary, description, keywords,
   * mediatype, period, ...
   */
  VALHALLA_META_GRP_CLASSIFICATION,

  /**
   * commercial, payment, purchase info, purchase price, purchase item,
   * purchase owner, purchase currency, file owner, ...
   */
  VALHALLA_META_GRP_COMMERCIAL,

  /**
   * url, email, address, phone, fax, ...
   */
  VALHALLA_META_GRP_CONTACT,

  /**
   * artist, url, performer, accompaniment, band, ensemble, composer,
   * arranger, lyricist, conductor, actor, character, author, director,
   * producer, coproducer, executive producer, costume designer, label,
   * choregrapher, sound engineer, production studio, publisher, ...
   */
  VALHALLA_META_GRP_ENTITIES,

  /**
   * isrc, mcdi, isbn, barcode, lccn, cdid, ufid, ...
   */
  VALHALLA_META_GRP_IDENTIFIER,

  /**
   * copyright, terms of use, url, ownership, license, rights, ...
   */
  VALHALLA_META_GRP_LEGAL,

  /**
   * user text, orig filename, picture, lyrics, ...
   */
  VALHALLA_META_GRP_MISCELLANEOUS,

  /**
   * bmp, measure, tunning, initial key, ...
   */
  VALHALLA_META_GRP_MUSICAL,

  /**
   * track, disk, part number, track number, disc number, total tracks,
   * total parts, ...
   */
  VALHALLA_META_GRP_ORGANIZATIONAL,

  /**
   * comment, rating, play count, ...
   */
  VALHALLA_META_GRP_PERSONAL,

  /**
   * composition location, recording location, composer nationality, ...
   */
  VALHALLA_META_GRP_SPACIAL,

  /**
   * encoder, playlist delay, buffer size, ...
   */
  VALHALLA_META_GRP_TECHNICAL,

  /**
   * date written, date recorded, date released, date digitized, date encoded,
   * date tagged, date purchased, year, ...
   */
  VALHALLA_META_GRP_TEMPORAL,

  /**
   * title, album, subtitle, title sort order, album sort order, part ...
   */
  VALHALLA_META_GRP_TITLES,

} valhalla_meta_grp_t;

typedef enum valhalla_db_type {
  VALHALLA_DB_TYPE_ID,
  VALHALLA_DB_TYPE_TEXT,
  VALHALLA_DB_TYPE_GROUP,
} valhalla_db_type_t;

typedef enum valhalla_db_operator {
  VALHALLA_DB_OPERATOR_IN,
  VALHALLA_DB_OPERATOR_NOTIN,
  VALHALLA_DB_OPERATOR_EQUAL,
} valhalla_db_operator_t;

typedef struct valhalla_db_item_s {
  valhalla_db_type_t type;
  int64_t     id;
  const char *text;
  valhalla_meta_grp_t group;
} valhalla_db_item_t;

typedef struct valhalla_db_metares_s {
  int64_t     meta_id,    data_id;
  const char *meta_name, *data_value;
  valhalla_meta_grp_t group;
} valhalla_db_metares_t;

typedef struct valhalla_db_fileres_s {
  int64_t     id;
  const char *path;
  valhalla_file_type_t type;
} valhalla_db_fileres_t;

typedef struct valhalla_db_restrict_s {
  struct valhalla_db_restrict_s *next;
  valhalla_db_operator_t op;
  valhalla_db_item_t meta;
  valhalla_db_item_t data;
} valhalla_db_restrict_t;

typedef struct valhalla_db_filemeta_s {
  struct valhalla_db_filemeta_s *next;
  int64_t  meta_id;
  int64_t  data_id;
  char    *meta_name;
  char    *data_value;
  valhalla_meta_grp_t group;
} valhalla_db_filemeta_t;

#define VALHALLA_DB_SEARCH(_id, _text, _group, _type) \
  {.type = VALHALLA_DB_TYPE_##_type,                  \
   .id = _id, .text = _text, .group = VALHALLA_META_GRP_##_group}

#define VALHALLA_DB_RESTRICT(_op, _m_id, _d_id,                  \
                             _m_text, _d_text, _m_type, _d_type) \
  {.next = NULL, .op = VALHALLA_DB_OPERATOR_##_op,               \
   .meta = VALHALLA_DB_SEARCH (_m_id, _m_text, NIL, _m_type),   \
   .data = VALHALLA_DB_SEARCH (_d_id, _d_text, NIL, _d_type)}


/**
 * \name Macros for selection functions handling.
 * @{
 */

/** \brief Set valhalla_db_item_t local variable for an id. */
#define VALHALLA_DB_SEARCH_ID(meta_id, group) \
  VALHALLA_DB_SEARCH (meta_id, NULL, group, ID)
/** \brief Set valhalla_db_item_t local variable for a text. */
#define VALHALLA_DB_SEARCH_TEXT(meta_name, group) \
  VALHALLA_DB_SEARCH (0, meta_name, group, TEXT)
/** \brief Set valhalla_db_item_t local variable for a group. */
#define VALHALLA_DB_SEARCH_GRP(group) \
  VALHALLA_DB_SEARCH (0, NULL, group, GROUP)

/** \brief Set valhalla_db_restrict_t local variable for meta.id, data.id. */
#define VALHALLA_DB_RESTRICT_INT(op, meta, data) \
  VALHALLA_DB_RESTRICT (op, meta, data, NULL, NULL, ID, ID)
/** \brief Set valhalla_db_restrict_t local variable for meta.text, data.text. */
#define VALHALLA_DB_RESTRICT_STR(op, meta, data) \
  VALHALLA_DB_RESTRICT (op, 0, 0, meta, data, TEXT, TEXT)
/** \brief Set valhalla_db_restrict_t local variable for meta.id, data.text. */
#define VALHALLA_DB_RESTRICT_INTSTR(op, meta, data) \
  VALHALLA_DB_RESTRICT (op, meta, 0, NULL, data, ID, TEXT)
/** \brief Set valhalla_db_restrict_t local variable for meta.text, data.id. */
#define VALHALLA_DB_RESTRICT_STRINT(op, meta, data) \
  VALHALLA_DB_RESTRICT (op, 0, data, meta, NULL, TEXT, ID)
/** \brief Link two valhalla_db_restrict_t variables together. */
#define VALHALLA_DB_RESTRICT_LINK(from, to) \
  do {(to).next = &(from);} while (0)

/** \brief Free a valhalla_db_filemeta_t pointer. */
#define VALHALLA_DB_FILEMETA_FREE(meta)                  \
  do {                                                   \
    typeof (meta) tmp;                                   \
    while (meta) {                                       \
      if ((meta)->meta_name)  free ((meta)->meta_name);  \
      if ((meta)->data_value) free ((meta)->data_value); \
      tmp = (meta)->next; free (meta); meta = tmp;}      \
  } while (0)

/**
 * @}
 * \name Database selections.
 * @{
 */

/**
 * \brief Retrieve a list of metadata according to a condition.
 *
 * It is possible to retrieve a list of metadata according to
 * restrictions on metadata and values.
 *
 * Example (to list all albums of an author):
 *  \code
 *  search      = VALHALLA_DB_SEARCH_TEXT ("album", TITLES);
 *  restriction = VALHALLA_DB_RESTRICT_STR (IN, "author", "John Doe");
 *  \endcode
 *
 * \param[in] handle      Handle on the scanner.
 * \param[in] search      Condition for the search.
 * \param[in] restriction Restrictions on the list.
 * \param[out] result_cb  Result callback.
 * \param[in,out] data    Data for the first callback argument.
 * \return !=0 on error.
 */
int valhalla_db_metalist_get (valhalla_t *handle,
                              valhalla_db_item_t *search,
                              valhalla_db_restrict_t *restriction,
                              int (*result_cb) (void *data,
                                                valhalla_db_metares_t *res),
                              void *data);

/**
 * \brief Retrieve a list of files.
 *
 * It is possible to retrieve a list of files according to
 * restrictions on metadata and values.
 *
 * Example (to list all files of an author, without album):
 *  \code
 *  restriction_1 = VALHALLA_DB_RESTRICT_STR (IN, "author", "John Doe");
 *  restriction_2 = VALHALLA_DB_RESTRICT_STR (NOTIN, "album", NULL);
 *  VALHALLA_DB_RESTRICT_LINK (restriction_2, restriction_1);
 *  \endcode
 *
 * \param[in] handle      Handle on the scanner.
 * \param[in] filetype    File type.
 * \param[in] restriction Restrictions on the list.
 * \param[out] result_cb  Result callback.
 * \param[in,out] data    Data for the first callback argument.
 * \return !=0 on error.
 */
int valhalla_db_filelist_get (valhalla_t *handle,
                              valhalla_file_type_t filetype,
                              valhalla_db_restrict_t *restriction,
                              int (*result_cb) (void *data,
                                                valhalla_db_fileres_t *res),
                              void *data);

/**
 * \brief Retrieve all metadata of a file.
 *
 * Only one parameter (\p id or \p path) must be set in order to retrieve
 * a file. If both parameters are not null, then the \p path is ignored.
 *
 * \p *res must be freed by VALHALLA_DB_FILEMETA_FREE().
 *
 * Example (to retrieve only the track and the title):
 *  \code
 *  restriction_1 = VALHALLA_DB_RESTRICT_STR (EQUAL, "track", NULL);
 *  restriction_2 = VALHALLA_DB_RESTRICT_STR (EQUAL, "title", NULL);
 *  VALHALLA_DB_RESTRICT_LINK (restriction_2, restriction_1);
 *  \endcode
 *
 *  If several tracks and(or) titles are returned, you must use the group id
 *  in the result, in order to know what metadata is the right.
 *
 * \param[in] handle      Handle on the scanner.
 * \param[in] id          File ID or 0.
 * \param[in] path        Path or NULL.
 * \param[in] restriction Restrictions on the list of meta.
 * \param[out] res        Pointer on the linked list to populate.
 * \return !=0 on error.
 */
int valhalla_db_file_get (valhalla_t *handle,
                          int64_t id, const char *path,
                          valhalla_db_restrict_t *restriction,
                          valhalla_db_filemeta_t **res);

/**
 * @}
 */

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* VALHALLA_H */
