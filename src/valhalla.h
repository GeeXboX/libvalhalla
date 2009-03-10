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

/** \brief Author structure for valhalla_db_author(). */
typedef struct valhalla_db_author_s {
  int64_t     id;
  const char *name;
} valhalla_db_author_t;

/** \brief Album structure for valhalla_db_album(). */
typedef struct valhalla_db_album_s {
  int64_t     id;
  const char *name;
} valhalla_db_album_t;

/** \brief Genre structure for valhalla_db_genre(). */
typedef struct valhalla_db_genre_s {
  int64_t     id;
  const char *name;
} valhalla_db_genre_t;

/** \brief File structure for valhalla_db_file(). */
typedef struct valhalla_db_file_s {
  int64_t     id;
  const char *path;
  const char *title;
  const char *year;
  const char *track;
} valhalla_db_file_t;

/** \brief Types of condition for the query. */
typedef enum valhalla_db_cond {
  VALHALLA_DB_IGNORE = 0, /**< No condition in the query.           */
  VALHALLA_DB_NULL,       /**< foobar_id IS NULL.                   */
  VALHALLA_DB_EQUAL,      /**< foobar_id = ?.                       */
  VALHALLA_DB_NOTEQUAL,   /**< foobar_id != ? OR foobar_id IS NULL. */
} valhalla_db_cond_t;

/** \brief Conditions for WHERE in the query. */
typedef struct valhalla_db_file_where_s {
  struct valhalla_db_file_where_s *next;
  int64_t author_id;
  valhalla_db_cond_t author_w;
  int64_t album_id;
  valhalla_db_cond_t album_w;
  int64_t genre_id;
  valhalla_db_cond_t genre_w;
} valhalla_db_file_where_t;

/**
 * \name Database selections.
 * @{
 */

/**
 * \brief Retrieve authors from the database.
 *
 * \param[in] handle      Handle on the scanner.
 * \param[out] author     Structure to populate.
 * \param[in] album       List author by this album ID, 0 otherwise.
 * \return !=0 on error or if no more rows are available.
 */
int valhalla_db_author (valhalla_t *handle,
                        valhalla_db_author_t *author, int64_t album);

/**
 * \brief Retrieve albums from the database.
 *
 * \param[in] handle      Handle on the scanner.
 * \param[out] album      Structure to populate.
 * \param[in] where_id    List album by this ID, 0 otherwise.
 * \param[in] what        0 for author ID, !=0 for genre ID.
 * \return !=0 on error or if no more rows are available.
 */
int valhalla_db_album (valhalla_t *handle, valhalla_db_album_t *album,
                       int64_t where_id, int what);

/**
 * \brief Retrieve genres from the database.
 *
 * \param[in] handle      Handle on the scanner.
 * \param[out] genre      Structure to populate.
 * \return !=0 on error or if no more rows are available.
 */
int valhalla_db_genre (valhalla_t *handle, valhalla_db_genre_t *genre);

/**
 * \brief Retrieve files from the database.
 *
 * This function must be called until the result is different of 0, otherwise
 * the SQL statement will not be finalized.
 *
 * \p where can be used to give some conditions for the query. All
 * conditions are seperated by AND.
 *
 * \param[in] handle      Handle on the scanner.
 * \param[out] file       Structure to populate.
 * \param[in] where       Conditions for WHERE in the SQL query.
 * \return !=0 on error or if no more rows are available.
 */
int valhalla_db_file (valhalla_t *handle, valhalla_db_file_t *file,
                      valhalla_db_file_where_t *where);

/**
 * @}
 */

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* VALHALLA_H */
