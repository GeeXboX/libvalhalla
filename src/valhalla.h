/*
 * GeeXboX Valhalla: tiny media scanner API.
 * Copyright (C) 2009-2011 Mathieu Schroeter <mathieu@schroetersa.ch>
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

#define LIBVALHALLA_VERSION_MAJOR  2
#define LIBVALHALLA_VERSION_MINOR  1
#define LIBVALHALLA_VERSION_MICRO  0

#define LIBVALHALLA_DB_VERSION     2

#define LIBVALHALLA_VERSION_INT VH_VERSION_INT(LIBVALHALLA_VERSION_MAJOR, \
                                               LIBVALHALLA_VERSION_MINOR, \
                                               LIBVALHALLA_VERSION_MICRO)
#define LIBVALHALLA_VERSION     VH_VERSION(LIBVALHALLA_VERSION_MAJOR, \
                                           LIBVALHALLA_VERSION_MINOR, \
                                           LIBVALHALLA_VERSION_MICRO)
#define LIBVALHALLA_VERSION_STR VH_TOSTRING(LIBVALHALLA_VERSION)
#define LIBVALHALLA_BUILD       LIBVALHALLA_VERSION_INT

#include <inttypes.h>
#include <stdarg.h>

/**
 * \brief Return LIBVALHALLA_VERSION_INT constant.
 */
unsigned int libvalhalla_version (void);

/** \brief Languages for metadata. */
typedef enum valhalla_lang {
  VALHALLA_LANG_ALL   = -1,   /**< All languages.   */
  VALHALLA_LANG_UNDEF =  0,   /**< Undefined.       */
  VALHALLA_LANG_DE,           /**< German.          */
  VALHALLA_LANG_EN,           /**< English.         */
  VALHALLA_LANG_ES,           /**< Spanish.         */
  VALHALLA_LANG_FR,           /**< French.          */
  VALHALLA_LANG_IT,           /**< Italian.         */
} valhalla_lang_t;

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

/**
 * \name List of common metadata.
 * @{
 */

/* Classification */
#define VALHALLA_METADATA_CATEGORY                   "category"
#define VALHALLA_METADATA_EPISODE                    "episode"
#define VALHALLA_METADATA_GENRE                      "genre"
#define VALHALLA_METADATA_MPAA                       "mpaa"
#define VALHALLA_METADATA_RUNTIME                    "runtime"
#define VALHALLA_METADATA_SEASON                     "season"
#define VALHALLA_METADATA_SYNOPSIS                   "synopsis"
#define VALHALLA_METADATA_SYNOPSIS_SHOW              "synopsis_show"

/* Commercial */
#define VALHALLA_METADATA_BUDGET                     "budget"
#define VALHALLA_METADATA_COUNTRY                    "country"
#define VALHALLA_METADATA_REVENUE                    "revenue"
#define VALHALLA_METADATA_STUDIO                     "studio"

/* Entities */
#define VALHALLA_METADATA_ACTOR                      "actor"
#define VALHALLA_METADATA_ARTIST                     "artist"
#define VALHALLA_METADATA_AUTHOR                     "author"
#define VALHALLA_METADATA_CASTING                    "casting"
#define VALHALLA_METADATA_COMPOSER                   "composer"
#define VALHALLA_METADATA_CREDITS                    "credits"
#define VALHALLA_METADATA_DIRECTOR                   "director"
#define VALHALLA_METADATA_DIRECTOR_PHOTO             "director_photo"
#define VALHALLA_METADATA_EDITOR                     "editor"
#define VALHALLA_METADATA_PRODUCER                   "producer"
#define VALHALLA_METADATA_WRITER                     "writer"

/* Miscellaneous */
#define VALHALLA_METADATA_COVER                      "cover"
#define VALHALLA_METADATA_COVER_SEASON               "cover_season"
#define VALHALLA_METADATA_COVER_SHOW                 "cover_show"
#define VALHALLA_METADATA_COVER_SHOW_HEADER          "cover_show_header"
#define VALHALLA_METADATA_FAN_ART                    "fanart"
#define VALHALLA_METADATA_LYRICS                     "lyrics"
#define VALHALLA_METADATA_THUMBNAIL                  "thumbnail"

/* Organizational */
#define VALHALLA_METADATA_TRACK                      "track"

/* Personal */
#define VALHALLA_METADATA_PLAY_COUNT                 "playcount"
#define VALHALLA_METADATA_RATING                     "rating"
#define VALHALLA_METADATA_WATCHED                    "watched"

/* Technical */
#define VALHALLA_METADATA_AUDIO_BITRATE              "audio_bitrate"
#define VALHALLA_METADATA_AUDIO_CHANNELS             "audio_channels"
#define VALHALLA_METADATA_AUDIO_CODEC                "audio_codec"
#define VALHALLA_METADATA_AUDIO_LANG                 "audio_lang"
#define VALHALLA_METADATA_AUDIO_STREAMS              "audio_streams"
#define VALHALLA_METADATA_DURATION                   "duration"
#define VALHALLA_METADATA_FILESIZE                   "filesize"
#define VALHALLA_METADATA_HEIGHT                     "height"
#define VALHALLA_METADATA_PICTURE_ORIENTATION        "picture_orientation"
#define VALHALLA_METADATA_SUB_LANG                   "sub_lang"
#define VALHALLA_METADATA_SUB_STREAMS                "sub_streams"
#define VALHALLA_METADATA_VIDEO_ASPECT               "video_aspect"
#define VALHALLA_METADATA_VIDEO_BITRATE              "video_bitrate"
#define VALHALLA_METADATA_VIDEO_CODEC                "video_codec"
#define VALHALLA_METADATA_VIDEO_STREAMS              "video_streams"
#define VALHALLA_METADATA_WIDTH                      "width"

/* Temporal */
#define VALHALLA_METADATA_DATE                       "date"
#define VALHALLA_METADATA_PREMIERED                  "premiered"
#define VALHALLA_METADATA_YEAR                       "year"

/* Titles */
#define VALHALLA_METADATA_ALBUM                      "album"
#define VALHALLA_METADATA_TITLE                      "title"
#define VALHALLA_METADATA_TITLE_ALTERNATIVE          "title_alternative"
#define VALHALLA_METADATA_TITLE_SHOW                 "title_show"
#define VALHALLA_METADATA_TITLE_STREAM               "title_stream"

/**
 * @}
 */

/******************************************************************************/
/*                                                                            */
/* Valhalla Handling                                                          */
/*                                                                            */
/******************************************************************************/

/** \brief Scanner handle. */
typedef struct valhalla_s valhalla_t;

/** \brief Error code returned by valhalla_run(). */
enum valhalla_errno {
  VALHALLA_ERROR_DEAD    = -4,    /**< Valhalla is already running.         */
  VALHALLA_ERROR_PATH    = -3,    /**< Problem with the paths for the scan. */
  VALHALLA_ERROR_HANDLER = -2,    /**< Allocation memory error.             */
  VALHALLA_ERROR_THREAD  = -1,    /**< Problem with at least one thread.    */
  VALHALLA_SUCCESS       =  0,    /**< The Valkyries are running.           */
};

/** \brief Verbosity level. */
typedef enum valhalla_verb {
  VALHALLA_MSG_NONE,     /**< No error messages.                            */
  VALHALLA_MSG_VERBOSE,  /**< Super-verbose mode: mostly for debugging.     */
  VALHALLA_MSG_INFO,     /**< Working operations.                           */
  VALHALLA_MSG_WARNING,  /**< Harmless failures.                            */
  VALHALLA_MSG_ERROR,    /**< May result in hazardous behavior.             */
  VALHALLA_MSG_CRITICAL, /**< Prevents lib from working.                    */
} valhalla_verb_t;

/** \brief Destinations for downloading. */
typedef enum valhalla_dl {
  VALHALLA_DL_DEFAULT = 0,    /**< Destination by default.                  */
  VALHALLA_DL_COVER,          /**< Destination for covers.                  */
  VALHALLA_DL_THUMBNAIL,      /**< Destination for thumbnails.              */
  VALHALLA_DL_FAN_ART,        /**< Destination for fan-arts.                */
  VALHALLA_DL_LAST            /**< \internal Don't use it!                  */
} valhalla_dl_t;

/** \brief Events for valhalla_ondemand() callback. */
typedef enum valhalla_event_od {
  VALHALLA_EVENTOD_PARSED = 0, /**< Parsed data available in DB.            */
  VALHALLA_EVENTOD_GRABBED,    /**< Grabbed data available in DB.           */
  VALHALLA_EVENTOD_ENDED,      /**< Nothing more (downloading included).    */
} valhalla_event_od_t;

/** \brief Events for general actions in Valhalla. */
typedef enum valhalla_event_gl {
  VALHALLA_EVENTGL_SCANNER_BEGIN = 0, /**< Begin the scanning of paths.     */
  VALHALLA_EVENTGL_SCANNER_END,       /**< All paths scanned.               */
  VALHALLA_EVENTGL_SCANNER_SLEEP,     /**< Scanner is sleeping.             */
  VALHALLA_EVENTGL_SCANNER_ACKS,      /**< All files fully handled.         */
  VALHALLA_EVENTGL_SCANNER_EXIT,      /**< Exit, end of all loops.          */
} valhalla_event_gl_t;

/** \brief Events for metadata callback. */
typedef enum valhalla_event_md {
  VALHALLA_EVENTMD_PARSER = 0,    /**< New parsed data.                     */
  VALHALLA_EVENTMD_GRABBER,       /**< New grabbed data.                    */
} valhalla_event_md_t;

/** \brief Type of statistic. */
typedef enum valhalla_stats_type {
  VALHALLA_STATS_TIMER = 0,   /**< Read value for a timer.                  */
  VALHALLA_STATS_COUNTER,     /**< Read value for a counter.                */
} valhalla_stats_type_t;

/**
 * \brief Priorities for the metadata.
 *
 * The values which are not mod 32, are only for internal use.
 */
typedef enum valhalla_metadata_pl {
  VALHALLA_METADATA_PL_HIGHEST = -128,    /**< The highest priority.        */
  VALHALLA_METADATA_PL_HIGHER  =  -96,    /**< The higher priority.         */
  VALHALLA_METADATA_PL_HIGH    =  -64,    /**< High priority.               */
  VALHALLA_METADATA_PL_ABOVE   =  -32,    /**< Priority above normal.       */
  VALHALLA_METADATA_PL_NORMAL  =    0,    /**< Normal (usual) priority.     */
  VALHALLA_METADATA_PL_BELOW   =   32,    /**< Priority below normal.       */
  VALHALLA_METADATA_PL_LOW     =   64,    /**< Low priority.                */
  VALHALLA_METADATA_PL_LOWER   =   96,    /**< The lower priority.          */
  VALHALLA_METADATA_PL_LOWEST  =  128,    /**< The lowest priority.         */
} valhalla_metadata_pl_t;

/** \brief Metadata structure for general purpose. */
typedef struct valhalla_metadata_s {
  const char         *name;
  const char         *value;
  valhalla_meta_grp_t group;
  valhalla_lang_t     lang;
} valhalla_metadata_t;

/** \brief File structure for general purpose. */
typedef struct valhalla_file_s {
  const char          *path;
  int64_t              mtime;
  int64_t              size;
  valhalla_file_type_t type;
} valhalla_file_t;

#define VH_CFG_RANGE  8 /**< 256 possibilities for every combinations of type */

#define VH_VOID_T     (0 << VH_CFG_RANGE)  /**< void                        */
#define VH_VOIDP_T    (1 << VH_CFG_RANGE)  /**< void *                      */
#define VH_INT_T      (2 << VH_CFG_RANGE)  /**< int                         */
#define VH_VOIDP_2_T  (4 << VH_CFG_RANGE)  /**< void *                      */

/** \brief Macro to init items in ::valhalla_cfg_t. */
#define VH_CFG_INIT(name, type, num) VALHALLA_CFG_##name = ((type) + (num))

/**
 * \brief List of parameters available for the configuration.
 *
 * These parameters must be used with valhalla_config_set().
 *
 * <b>When adding a new entry in the enum:</b>
 *
 * When an entry must be added in this enum, keep this one by alphabetical
 * order. ABI is safely preserved as long as the types and the number provided
 * with VH_CFG_INIT() are not changed.
 *
 * Next \p num for the current combinations :
 * <pre>
 * VH_VOIDP_T                           : 2
 * VH_VOIDP_T | VH_INT_T                : 3
 * VH_VOIDP_T | VH_INT_T | VH_VOIDP_2_T : 1
 * </pre>
 *
 * \see VH_CFG_INIT().
 */
typedef enum valhalla_cfg {
  /**
   * Set a destination for the downloader. The default destination is used when
   * a specific destination is NULL.
   *
   * \p arg1 must be a null-terminated string.
   *
   * \warning There is no effect if the grabber support is not compiled.
   * \param[in] arg1 ::VH_VOIDP_T   Path for the destination.
   * \param[in] arg2 ::VH_INT_T     Type of destination to set, ::valhalla_dl_t.
   */
  VH_CFG_INIT (DOWNLOADER_DEST, VH_VOIDP_T | VH_INT_T, 2),

  /**
   * Change the metadata priorities in the grabbers.
   *
   * The argument \p arg3 should be a name provided in the list of common
   * metadata (above). If \p arg1 is NULL, it affects all grabbers.
   * If \p arg3 is NULL, then it changes the default priority, but specific
   * priorities are not modified.
   *
   * The string \p arg3 is not copied. The address must be valid until the
   * call on valhalla_uninit().
   *
   * \p arg1 and \p arg3 must be null-terminated strings.
   *
   * \warning There is no effect if the grabber support is not compiled.
   * \param[in] arg1 ::VH_VOIDP_T   Grabber ID.
   * \param[in] arg2 ::VH_INT_T     The new priority, ::valhalla_metadata_pl_t.
   * \param[in] arg3 ::VH_VOIDP_2_T Metadata.
   */
  VH_CFG_INIT (GRABBER_PRIORITY, VH_VOIDP_T | VH_INT_T | VH_VOIDP_2_T, 0),

  /**
   * Set the state of a grabber. By default, all grabbers are enabled.
   *
   * \p arg1 must be a null-terminated string.
   *
   * \warning There is no effect if the grabber support is not compiled.
   * \param[in] arg1 ::VH_VOIDP_T   Grabber ID.
   * \param[in] arg2 ::VH_INT_T     0 to disable, !=0 to enable.
   */
  VH_CFG_INIT (GRABBER_STATE, VH_VOIDP_T | VH_INT_T, 0),

  /**
   * This parameter is useful only if the decrapifier is enabled with
   * valhalla_init().
   *
   * The keywords are case insensitive except when a pattern (NUM, SE or EP)
   * is used.
   *
   * Available patterns (unsigned int):
   * - NUM to trim a number
   * - SE  to trim and retrieve a "season" number (at least >= 1)
   * - EP  to trim and retrieve an "episode" number (at least >= 1)
   *
   * NUM can be used several time in the same keyword, like "NUMxNUM". But SE
   * and EP must be used only one time by keyword. When a season or an episode
   * is found, a new metadata is added for each one.
   *
   * Examples:
   * - Blacklist : "xvid", "foobar", "fileNUM",
   *               "sSEeEP", "divx", "SExEP", "NumEP"
   * \n\n
   * - Filename  : "{XvID-Foobar}.file01.My_Movie.s02e10.avi"
   * - Result    : "My Movie", season=2 and episode=10
   * \n\n
   * - Filename  : "My_Movie_2.s02e10_(5x3)_.mkv"
   * - Result    : "My Movie 2", season=2, episode=10, season=5, episode=3
   * \n\n
   * - Filename  : "The-Episode.-.Pilot_DivX.(01x01)_FooBar.mkv"
   * - Result    : "The Episode Pilot", season=1 and episode=1
   * \n\n
   * - Filename  : "_Name_of_the_episode_Num05.ogg"
   * - Result    : "Name of the episode", episode=5
   *
   * If the same keyword is added several times, only one is saved in the
   * decrapifier.
   *
   * \p arg1 must be a null-terminated string.
   *
   * \param[in] arg1 ::VH_VOIDP_T   Keyword to blacklist.
   */
  VH_CFG_INIT (PARSER_KEYWORD, VH_VOIDP_T, 0),

  /**
   * Add a path to the scanner. If the same path is added several times,
   * only one is saved in the scanner.
   *
   * \p arg1 must be a null-terminated string.
   *
   * \param[in] arg1 ::VH_VOIDP_T   The path to be scanned.
   * \param[in] arg2 ::VH_INT_T     1 to scan all dirs recursively, 0 otherwise.
   */
  VH_CFG_INIT (SCANNER_PATH, VH_VOIDP_T | VH_INT_T, 1),

  /**
   * If no suffix is added to the scanner, then all files will be parsed by
   * FFmpeg without exception and it can be very slow. It is highly recommanded
   * to always set at least one suffix (file extension)! If the same suffix is
   * added several times, only one is saved in the scanner. The suffixes are
   * case insensitive.
   *
   * \p arg1 must be a null-terminated string.
   *
   * \param[in] arg1 ::VH_VOIDP_T   File suffix to add.
   */
  VH_CFG_INIT (SCANNER_SUFFIX, VH_VOIDP_T, 1),

} valhalla_cfg_t;

/** \brief Parameters for valhalla_init(). */
typedef struct valhalla_init_param_s {
  /**
   * Number of threads for parsing (max 8); the parsers are concurrent.
   * The default number of threads is 2.
   */
  unsigned int parser_nb;
  /**
   * Number of threads for grabbing (max 16); the grabbers are concurrent
   * as long as their ID are different. The default number of threads is 2.
   * To use many threads will not increase a lot the use of memory, but
   * it can increase significantly the use of the bandwidth for Internet
   * and the CPU load. Set this parameter to 1, in order to serialize the
   * calls on the grabbers. A value of 3 or 4 is a good choice for most of
   * the uses.
   */
  unsigned int grabber_nb;
  /**
   * Number of data (set of metadata) to be inserted or updated in one pass
   * in the database (BEGIN and COMMIT sql mechanisms). A value between 100
   * and 200 is a good choice. The default interval is 128.
   */
  unsigned int commit_int;
  /**
   * If the "title" metadata is not available with a file, the decrapifier
   * can be used to create this metadata by using the filename. This feature
   * is very useful when the grabbing support is enabled, because the title
   * is used as keywords in a lot of grabbers. By default the decrapifier is
   * disabled.
   */
  unsigned int decrapifier : 1;
  /**
   * If the attribute is set, then the meta keys can be retrieved from the
   * ondemand callback by using the function valhalla_ondemand_cb_meta().
   */
  unsigned int od_meta     : 1;

  /**
   * When \p od_cb is defined, an event is sent for each step with an on demand
   * query. If an event arrives, the data are really inserted in the DB. The
   * order for the events is not determinative, VALHALLA_EVENTOD_GRABBED can be
   * sent before VALHALLA_EVENTOD_PARSED. VALHALLA_EVENTOD_GRABBED is sent for
   * each grabber and \p id is its textual identifier (for example: "amazon",
   * "exif", etc, ...). Only VALHALLA_EVENTOD_ENDED is always sent at the end,
   * but this one has not a high priority unlike other events. If the file is
   * already (fully) inserted in the DB, only VALHALLA_EVENTOD_ENDED is sent to
   * the callback.
   */
  void (*od_cb) (const char *file, valhalla_event_od_t e,
                 const char *id, void *data);
  /** User data for ondemand callback. */
  void *od_data;

  /**
   * When \p gl_cb is defined, events can be sent by Valhalla according to some
   * global actions. See ::valhalla_event_gl_t for details on the events.
   */
  void (*gl_cb) (valhalla_event_gl_t e, void *data);
  /** User data for global event callback. */
  void *gl_data;

  /**
   * When \p md_cb is defined, events can be sent by Valhalla each time that a
   * file metadata set is completed. Where \p id is the textual identifier (for
   * example: "amazon", "exif", etc, ...) of the grabber when the event \p e
   * is VALHALLA_EVENTMD_GRABBER. This callback is called for each metadata.
   * If there are 10 metadata in one set, then this callback is called 10 times.
   * The use of this callback is not recommanded. It may increase significantly
   * the use of memory because all metadata are kept (and duplicated when it
   * comes from the parser) until a set is fully read.
   */
  void (*md_cb) (valhalla_event_md_t e, const char *id,
                 const valhalla_file_t *file,
                 const valhalla_metadata_t *md, void *data);
  /** User data for metadata event callback. */
  void *md_data;

} valhalla_init_param_t;

/**
 * \name Valhalla Handling.
 * @{
 */

/** \cond */
/* This function must not be used directly; refer to valhalla_config_set(). */
int valhalla_config_set_orig (valhalla_t *handle, valhalla_cfg_t conf, ...);
/** \endcond */

/**
 * \brief Configure an handle.
 *
 * The list of available parameters is defined by enum ::valhalla_cfg_t.
 * VALHALLA_CFG_ is automatically prepended to \p conf.
 *
 * The function must be used as follow (for example):
 * \code
 * ret = valhalla_config_set (handle, GRABBER_STATE, "ffmpeg", 0);
 * \endcode
 *
 * Because it uses variadic arguments, there is a check on the number of
 * arguments passed to the function and it returns a critical error if it
 * fails. But it can't detect all bad uses. It is the job of the programmer
 * to use correctly this function in all cases.
 *
 * \warning This function must be called before valhalla_run()!
 * \param[in] handle      Handle on the scanner.
 * \param[in] conf        Parameter to configure.
 * \param[in] arg         List of arguments.
 * \return !=0 on error.
 */
#define valhalla_config_set(handle, conf, arg...) \
  valhalla_config_set_orig (handle, VALHALLA_CFG_##conf, ##arg, ~0)

/**
 * \brief Init a scanner and the database.
 *
 * If a database already exists, then it is used. Otherwise, a new database
 * is created to \p db. If more than one handles are created, you can't use
 * the same database. You must specify a different \p db for each handle.
 *
 * For a description of each parameters supported by this function:
 * \see ::valhalla_init_param_t
 *
 * When a parameter in \p param is 0 (or NULL), its default value is used.
 * If \p param is NULL, then all default values are forced for all parameters.
 *
 * \param[in] db          Path on the database.
 * \param[in] param       Parameters, NULL for default values.
 * \return The handle.
 */
valhalla_t *valhalla_init (const char *db, valhalla_init_param_t *param);

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
 * \brief Retrieve an human readable string according to a group number.
 *
 * The strings returned are the same that the strings saved in the database.
 *
 * \warning This function can be called in anytime.
 * \param[in] group       Group number.
 * \return the string.
 */
const char *valhalla_metadata_group_str (valhalla_meta_grp_t group);

/**
 * \brief Retrieve the ID of all grabbers compiled in Valhalla.
 *
 * The function returns the ID after \p id, or the first grabber ID if \p id
 * is NULL.
 *
 * \warning This function must be called before valhalla_run()!
 *          There is no effect if the grabber support is not compiled.
 * \param[in] handle      Handle on the scanner.
 * \param[in] id          Grabber ID or NULL to retrieve the first.
 * \return the next ID or NULL if \p id is the last (or on error).
 */
const char *valhalla_grabber_next (valhalla_t *handle, const char *id);

/**
 * \brief Retrieve the priority for a metadata according to a grabber.
 *
 * If \p id is NULL, the result is 0. To retrieve the default priority,
 * the argument \p *meta must be set to NULL. On the return, \p *meta is
 * the next metadata in the list, or NULL if there is nothing more.
 * If on call, \p *meta is not found, then the result is 0 and \p *meta
 * is not changed. If \p meta is NULL, the result is 0.
 *
 * Please, note that 0 is a valid value for a priority and must not be
 * used to detect errors. If this function is used correctly, no error is
 * possible.
 *
 * Use valhalla_grabber_next() in order to retrieve the IDs.
 *
 * \param[in] handle      Handle on the scanner.
 * \param[in] id          A valid grabber ID.
 * \param[in,out] meta    A valid address; the next meta is returned.
 * \return the priority.
 */
valhalla_metadata_pl_t valhalla_grabber_priority_read (valhalla_t *handle,
                                                       const char *id,
                                                       const char **meta);

/**
 * \brief Retrieve the ID of all groups in the statistics.
 *
 * The function returns the ID after \p id, or the first group ID if \p id
 * is NULL.
 *
 * \warning This function can be called in anytime.
 * \param[in] handle      Handle on the scanner.
 * \param[in] id          Group ID or NULL to retrieve the first.
 * \return the next ID or NULL if \p id is the last (or on error).
 */
const char *valhalla_stats_group_next (valhalla_t *handle, const char *id);

/**
 * \brief Retrieve the value of a timer or a counter in the statistics.
 *
 * \p item ID is set according to the next timer or the next counter.
 * If the \p item ID is not changed on the return, then an error was
 * encountered.
 *
 * \warning This function can be called in anytime.
 * \param[in] handle      Handle on the scanner.
 * \param[in] id          Group ID.
 * \param[in] type        Timer or counter.
 * \param[in,out] item    Item ID or NULL for the first.
 * \return the value (nanoseconds for the timers).
 */
uint64_t valhalla_stats_read_next (valhalla_t *handle, const char *id,
                                   valhalla_stats_type_t type,
                                   const char **item);

/**
 * \brief Run the scanner, the database manager and all parsers.
 *
 * The \p priority can be set to all thread especially to run the system
 * in background with less priority. In the case of a user, you can change
 * only for a lower priority.
 *
 *             0 (normal priority used by default)
 * Linux   : -20 (highest) to 19 (lowest)
 * FreeBSD : -20 (highest) to 20 (lowest)
 * Windows :  -3 (highest) to  3 (lowest)
 *
 * \param[in] handle      Handle on the scanner.
 * \param[in] loop        Number of loops (<=0 for infinite).
 * \param[in] timeout     Timeout between loops, 0 to disable [seconds].
 * \param[in] delay       Delay before the scanning begins [seconds].
 * \param[in] priority    Priority set to all threads.
 * \return 0 for success and <0 on error (see enum valhalla_errno).
 */
int valhalla_run (valhalla_t *handle,
                  int loop, uint16_t timeout, uint16_t delay, int priority);

/**
 * \brief Wait until the scanning is finished.
 *
 * This function wait until the scanning is finished for all loops. If the
 * number of loops is infinite, then this function will wait forever. You
 * must not break this function with valhalla_uninit(), that is not safe!
 * If you prefer stop the scanner even if it is not finished. In this case
 * you must use _only_ valhalla_uninit().
 *
 * If no path is defined (then the scanner is not running), this function
 * returns immediately.
 *
 * \warning This function can be used only after valhalla_run()!
 * \param[in] handle      Handle on the scanner.
 */
void valhalla_wait (valhalla_t *handle);

/**
 * \brief Force to wake up the scanner.
 *
 * If the scanner is sleeping, this function will wake up this one independently
 * of the time (\p timeout) set with valhalla_run(). If the number of loops is
 * already reached or if the scanner is already working, this function has no
 * effect.
 *
 * \warning This function can be used only after valhalla_run()!
 * \param[in] handle      Handle on the scanner.
 */
void valhalla_scanner_wakeup (valhalla_t *handle);

/**
 * \brief Force Valhalla to retrieve metadata on-demand for a file.
 *
 * This functionality can be used on files in/out of paths defined for
 * the scanner. This function is non-blocked and it has the top priority
 * over the files retrieved by the scanner.
 *
 * \warning This function can be used only after valhalla_run()!
 * \param[in] handle      Handle on the scanner.
 * \param[in] file        Target.
 */
void valhalla_ondemand (valhalla_t *handle, const char *file);

/**
 * \brief Retrieve the meta key when running in the ondemand callback.
 *
 * This function is a no-op when it is used elsewhere that an ondemand callback
 * or if the od_meta attribute of ::valhalla_init_param_t is 0.
 *
 * The function returns the key after \p meta, or the first key if \p meta
 * is NULL. The returned pointer is valid as long as your are in the callback.
 *
 * \param[in] handle      Handle on the scanner.
 * \param[in] meta        Meta or NULL to retrieve the first.
 * \return the meta or NULL if \p meta is the last (or on error).
 */
const char *valhalla_ondemand_cb_meta (valhalla_t *handle, const char *meta);

/**
 * @}
 */

/******************************************************************************/
/*                                                                            */
/* Database Selections                                                        */
/*                                                                            */
/******************************************************************************/

/** \brief Prepared statement. */
typedef struct valhalla_db_stmt_s valhalla_db_stmt_t;

/** \brief Type of field. */
typedef enum valhalla_db_type {
  VALHALLA_DB_TYPE_ID,
  VALHALLA_DB_TYPE_TEXT,
  VALHALLA_DB_TYPE_GROUP,
} valhalla_db_type_t;

/** \brief Operator for a restriction. */
typedef enum valhalla_db_operator {
  VALHALLA_DB_OPERATOR_IN,
  VALHALLA_DB_OPERATOR_NOTIN,
  VALHALLA_DB_OPERATOR_EQUAL,
} valhalla_db_operator_t;

/** \brief Main structure to search in the DB. */
typedef struct valhalla_db_item_s {
  valhalla_db_type_t type;
  int64_t     id;
  const char *text;
  valhalla_meta_grp_t group;
  valhalla_lang_t lang;
  valhalla_metadata_pl_t priority;
} valhalla_db_item_t;

/** \brief Results for valhalla_db_metalist_get(). */
typedef struct valhalla_db_metares_s {
  int64_t     meta_id,    data_id;
  const char *meta_name, *data_value;
  valhalla_meta_grp_t group;
  valhalla_lang_t lang;
  int         external;
} valhalla_db_metares_t;

/** \brief Results for valhalla_db_filelist_get(). */
typedef struct valhalla_db_fileres_s {
  int64_t     id;
  const char *path;
  valhalla_file_type_t type;
} valhalla_db_fileres_t;

/** \brief Restriction. */
typedef struct valhalla_db_restrict_s {
  struct valhalla_db_restrict_s *next;
  valhalla_db_operator_t op;
  valhalla_db_item_t meta;
  valhalla_db_item_t data;
} valhalla_db_restrict_t;


/**
 * \name Macros for selection functions handling.
 * @{
 */

/**
 * \brief Set valhalla_db_item_t local variable.
 *
 * If possible, prefer the macros VALHALLA_DB_SEARCH_*() instead of this one.
 *
 * \param[in] id      Meta or data ID.
 * \param[in] txt     Meta or data text.
 * \param[in] g       Meta group.
 * \param[in] t       Type of field.
 * \param[in] l       Language.
 * \param[in] p       Minimum priority.
 */
#define VALHALLA_DB_SEARCH(id, txt, g, t, l, p)   \
  {                                               \
    /* .type     = */ VALHALLA_DB_TYPE_##t,       \
    /* .id       = */ id,                         \
    /* .text     = */ txt,                        \
    /* .group    = */ VALHALLA_META_GRP_##g,      \
    /* .lang     = */ l,                          \
    /* .priority = */ p                           \
  }

/**
 * \brief Set valhalla_db_restrict_t local variable.
 *
 * If possible, prefer the macros VALHALLA_DB_RESTRICT_*() instead of this one.
 *
 * \param[in] op      Operator applied on the restriction.
 * \param[in] m_id    Meta ID.
 * \param[in] d_id    Data ID.
 * \param[in] m_txt   Meta text.
 * \param[in] d_txt   Data text.
 * \param[in] m_t     Type of field for meta.
 * \param[in] d_t     Type of field for data.
 * \param[in] l       Language.
 * \param[in] p       Minimum priority.
 */
#define VALHALLA_DB_RESTRICT(op, m_id, d_id, m_txt, d_txt, m_t, d_t, l, p)  \
  {                                                                         \
    /* .next = */ NULL,                                                     \
    /* .op   = */ VALHALLA_DB_OPERATOR_##op,                                \
    /* .meta = */ VALHALLA_DB_SEARCH (m_id, m_txt, NIL, m_t, l, p),         \
    /* .data = */ VALHALLA_DB_SEARCH (d_id, d_txt, NIL, d_t, l, p)          \
  }

/** \brief Set valhalla_db_item_t local variable for an id. */
#define VALHALLA_DB_SEARCH_ID(meta_id, group, l, p) \
  VALHALLA_DB_SEARCH (meta_id, NULL, group, ID, l, p)
/** \brief Set valhalla_db_item_t local variable for a text. */
#define VALHALLA_DB_SEARCH_TEXT(meta_name, group, l, p) \
  VALHALLA_DB_SEARCH (0, meta_name, group, TEXT, l, p)
/** \brief Set valhalla_db_item_t local variable for a group. */
#define VALHALLA_DB_SEARCH_GRP(group, l, p) \
  VALHALLA_DB_SEARCH (0, NULL, group, GROUP, l, p)

/** \brief Set valhalla_db_restrict_t local variable for meta.id, data.id. */
#define VALHALLA_DB_RESTRICT_INT(op, meta, data, l, p) \
  VALHALLA_DB_RESTRICT (op, meta, data, NULL, NULL, ID, ID, l, p)
/** \brief Set valhalla_db_restrict_t local variable for meta.text, data.text. */
#define VALHALLA_DB_RESTRICT_STR(op, meta, data, l, p) \
  VALHALLA_DB_RESTRICT (op, 0, 0, meta, data, TEXT, TEXT, l, p)
/** \brief Set valhalla_db_restrict_t local variable for meta.id, data.text. */
#define VALHALLA_DB_RESTRICT_INTSTR(op, meta, data, l, p) \
  VALHALLA_DB_RESTRICT (op, meta, 0, NULL, data, ID, TEXT, l, p)
/** \brief Set valhalla_db_restrict_t local variable for meta.text, data.id. */
#define VALHALLA_DB_RESTRICT_STRINT(op, meta, data, l, p) \
  VALHALLA_DB_RESTRICT (op, 0, data, meta, NULL, TEXT, ID, l, p)
/** \brief Link two valhalla_db_restrict_t variables together. */
#define VALHALLA_DB_RESTRICT_LINK(from, to) \
  do {(to).next = &(from);} while (0)

/**
 * @}
 * \name Database selections.
 * @{
 */

/**
 * \brief Init a statement to retrieve a list of metadata.
 *
 * It is possible to retrieve a list of metadata according to
 * restrictions on metadata and values.
 *
 * Example (to list all albums of an author):
 *  \code
 *  lang   = VALHALLA_LANG_ALL;
 *  pmin   = VALHALLA_METADATA_PL_LOWEST;
 *  search = VALHALLA_DB_SEARCH_TEXT ("album", TITLES, lang, pmin);
 *  restr  = VALHALLA_DB_RESTRICT_STR (IN, "author", "John Doe", lang, pmin);
 *  \endcode
 *
 * \param[in] handle      Handle on the scanner.
 * \param[in] search      Condition for the search.
 * \param[in] filetype    File type.
 * \param[in] restriction Restrictions on the list.
 * \return the statement, NULL on error.
 */
valhalla_db_stmt_t *
valhalla_db_metalist_get (valhalla_t *handle,
                          valhalla_db_item_t *search,
                          valhalla_file_type_t filetype,
                          valhalla_db_restrict_t *restriction);

/**
 * \brief Read the next row of a 'metalist' statement.
 *
 * The argument \p vhstmt must be initialized with valhalla_db_metalist_get().
 * It is freed when the returned value is NULL. The pointer returned by the
 * function is valid as long as no new call is done for the \p vhstmt.
 *
 * \param[in] handle      Handle on the scanner.
 * \param[in] vhstmt      Statement.
 * \return the result, NULL if no more row or on error.
 */
const valhalla_db_metares_t *
valhalla_db_metalist_read (valhalla_t *handle, valhalla_db_stmt_t *vhstmt);

/**
 * \brief Init a statement to retrieve a list of files.
 *
 * It is possible to retrieve a list of files according to
 * restrictions on metadata and values.
 *
 * Example (to list all files of an author, without album):
 *  \code
 *  lang    = VALHALLA_LANG_ALL;
 *  pmin    = VALHALLA_METADATA_PL_NORMAL;
 *  restr_1 = VALHALLA_DB_RESTRICT_STR (IN, "author", "John Doe", lang, pmin);
 *  restr_2 = VALHALLA_DB_RESTRICT_STR (NOTIN, "album", NULL, lang, pmin);
 *  VALHALLA_DB_RESTRICT_LINK (restr_2, restr_1);
 *  \endcode
 *
 * \param[in] handle      Handle on the scanner.
 * \param[in] filetype    File type.
 * \param[in] restriction Restrictions on the list.
 * \return the statement, NULL on error.
 */
valhalla_db_stmt_t *
valhalla_db_filelist_get (valhalla_t *handle,
                          valhalla_file_type_t filetype,
                          valhalla_db_restrict_t *restriction);

/**
 * \brief Read the next row of a 'filelist' statement.
 *
 * The argument \p vhstmt must be initialized with valhalla_db_filelist_get().
 * It is freed when the returned value is NULL. The pointer returned by the
 * function is valid as long as no new call is done for the \p vhstmt.
 *
 * \param[in] handle      Handle on the scanner.
 * \param[in] vhstmt      Statement.
 * \return the result, NULL if no more row or on error.
 */
const valhalla_db_fileres_t *
valhalla_db_filelist_read (valhalla_t *handle, valhalla_db_stmt_t *vhstmt);

/**
 * \brief Init a statement to retrieve the metadata of file.
 *
 * Only one parameter (\p id or \p path) must be set in order to retrieve
 * a file. If both parameters are not null, then the \p path is ignored.
 *
 * Example (to retrieve only the track and the title):
 *  \code
 *  pmin          = VALHALLA_METADATA_PL_LOWEST;
 *  restriction_1 = VALHALLA_DB_RESTRICT_STR (EQUAL, "track", NULL, pmin);
 *  restriction_2 = VALHALLA_DB_RESTRICT_STR (EQUAL, "title", NULL, pmin);
 *  VALHALLA_DB_RESTRICT_LINK (restriction_2, restriction_1);
 *  \endcode
 *
 *  If several tracks and(or) titles are returned, you must use the group id
 *  in the result, in order to know what metadata is the right.
 *
 * \param[in] handle      Handle on the scanner.
 * \param[in] id          File ID or 0.
 * \param[in] path        Path or NULL.
 * \param[in] restriction Restrictions on the list.
 * \return the statement, NULL on error.
 */
valhalla_db_stmt_t *
valhalla_db_file_get (valhalla_t *handle, int64_t id, const char *path,
                      valhalla_db_restrict_t *restriction);

/**
 * \brief Read the next row of a 'file' statement.
 *
 * The argument \p vhstmt must be initialized with valhalla_db_file_get().
 * It is freed when the returned value is NULL. The pointer returned by the
 * function is valid as long as no new call is done for the \p vhstmt.
 *
 * \param[in] handle      Handle on the scanner.
 * \param[in] vhstmt      Statement.
 * \return the result, NULL if no more row or on error.
 */
const valhalla_db_metares_t *
valhalla_db_file_read (valhalla_t *handle, valhalla_db_stmt_t *vhstmt);

/**
 * @}
 * \name Database insertions/updates/deletions.
 *
 * With these functions, you can insert/update and delete metadata for a
 * particular file (\p path). They should not be used to provide grabbing
 * functionalities with the front-end (implement a grabber in Valhalla is the
 * better way); but in some exceptional cases it can be necessary.
 *
 * For example, you can use this functionality to write data like "playcount"
 * or "last_position" (to replay a file from the last position).
 *
 * @{
 *
 * \page ext_metadata External Metadata
 *
 * \section ext_metadata External Metadata rules
 *
 * \see valhalla_db_metadata_insert().
 * \see valhalla_db_metadata_update().
 * \see valhalla_db_metadata_delete().
 * \see valhalla_db_metadata_priority() (only 6.).
 *
 * <ol>
 * <li>A data inserted/updated by these functions can not be updated by
 * Valhalla.</li>
 *
 * <li>The metadata are only inserted/updated and deleted in the database, the
 * tags in the files are not modified.</li>
 *
 * <li>If a metadata is changed in a file, a new metadata will be inserted by
 * Valhalla but your entries (inserted or updated by these functions) will
 * not be altered (consequence, you can have duplicated informations if the
 * value is not exactly the same).</li>
 *
 * <li>If a metadata was already inserted by Valhalla and you use these
 * functions to insert or to update the same entry, this metadata will be
 * changed to be considered like an external metadata (see point 1).</li>
 *
 * <li>If a file is no longer available, when Valhalla removes all metadata,
 * the metadata inserted and updated with these functions are removed too.</li>
 *
 * <li>If valhalla_uninit() is called shortly after one of these functions,
 * there is no guarenteed that the metadata is handled.</li>
 * </ol>
 */

/**
 * \brief Insert an external metadata in the database.
 *
 * When a metadata is inserted with this function, you must use
 * valhalla_db_metadata_update() to change the value, else two metadata will
 * be available (for both values).
 *
 * If the metadata is already available in the database and the \p group
 * (or the \p lang) passed with this function is not the same, then the
 * insertion is canceled and no error is returned, else the 'external' flag is
 * set to 1.
 * \see ::valhalla_db_metares_t
 *
 * Please, refer to \ref ext_metadata.
 *
 * \param[in] handle      Handle on the scanner.
 * \param[in] path        Path on the file.
 * \param[in] meta        Meta name.
 * \param[in] data        Data value.
 * \param[in] lang        Language.
 * \param[in] group       Group.
 * \return !=0 on error.
 */
int valhalla_db_metadata_insert (valhalla_t *handle, const char *path,
                                 const char *meta, const char *data,
                                 valhalla_lang_t lang,
                                 valhalla_meta_grp_t group);

/**
 * \brief Update an external metadata in the database.
 *
 * The previous \p data is necessary for Valhalla to identify the
 * association for the update.
 *
 * If \p ndata already exists in the database, the language is not updated
 * with the value passed by this function.
 *
 * Please, refer to \ref ext_metadata.
 *
 * \param[in] handle      Handle on the scanner.
 * \param[in] path        Path on the file.
 * \param[in] meta        Meta name.
 * \param[in] data        Current data value.
 * \param[in] ndata       New data value.
 * \param[in] lang        Language.
 * \return !=0 on error.
 */
int valhalla_db_metadata_update (valhalla_t *handle, const char *path,
                                 const char *meta, const char *data,
                                 const char *ndata, valhalla_lang_t lang);

/**
 * \brief Delete an external metadata in the database.
 *
 * Only a metadata inserted or updated with valhalla_db_metadata_insert(), and
 * valhalla_db_metadata_update() can be deleted with this function.
 *
 * Please, refer to \ref ext_metadata.
 *
 * \param[in] handle      Handle on the scanner.
 * \param[in] path        Path on the file.
 * \param[in] meta        Meta name.
 * \param[in] data        Data value.
 * \return !=0 on error.
 */
int valhalla_db_metadata_delete (valhalla_t *handle, const char *path,
                                 const char *meta, const char *data);

/**
 * \brief Change the priority for one or more metadata in the database.
 *
 * If \p meta is NULL, all metadata are changed. If \p data is NULL, all
 * metadata for a specific \p meta are changed. If \p meta is NULL, but
 * \p data is set, then the function returns an error.
 *
 * The 'external' flag is not altered by this function.
 *
 * Please, refer to \ref ext_metadata.
 *
 * \param[in] handle      Handle on the scanner.
 * \param[in] path        Path on the file.
 * \param[in] meta        Meta name.
 * \param[in] data        Data value.
 * \param[in] p           New priority.
 * \return !=0 on error.
 */
int valhalla_db_metadata_priority (valhalla_t *handle, const char *path,
                                   const char *meta, const char *data,
                                   valhalla_metadata_pl_t p);

/**
 * @}
 */

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* VALHALLA_H */
