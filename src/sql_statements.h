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

#ifndef VALHALLA_SQL_STATEMENTS_H
#define VALHALLA_SQL_STATEMENTS_H

/******************************************************************************/
/*                                                                            */
/*                                 Controls                                   */
/*                                                                            */
/******************************************************************************/

#define BEGIN_TRANSACTION \
 "BEGIN;"

#define END_TRANSACTION   \
 "COMMIT;"

/******************************************************************************/
/*                                                                            */
/*                              Create tables                                 */
/*                                                                            */
/******************************************************************************/

#define CREATE_TABLE_INFO                                 \
 "CREATE TABLE IF NOT EXISTS info ( "                     \
   "info_name        TEXT    PRIMARY KEY, "               \
   "info_value       TEXT    NOT NULL "                   \
 ");"

#define CREATE_TABLE_FILE                                 \
 "CREATE TABLE IF NOT EXISTS file ( "                     \
   "file_id          INTEGER PRIMARY KEY AUTOINCREMENT, " \
   "file_path        TEXT    NOT NULL UNIQUE, "           \
   "file_mtime       INTEGER NOT NULL, "                  \
   "checked__        INTEGER NOT NULL, "                  \
   "interrupted__    INTEGER NOT NULL, "                  \
   "outofpath__      INTEGER NOT NULL, "                  \
   "_type_id         INTEGER NULL "                       \
 ");"

#define CREATE_TABLE_TYPE                                 \
 "CREATE TABLE IF NOT EXISTS type ( "                     \
   "type_id          INTEGER PRIMARY KEY AUTOINCREMENT, " \
   "type_name        TEXT    NOT NULL UNIQUE "            \
 ");"

#define CREATE_TABLE_META                                 \
 "CREATE TABLE IF NOT EXISTS meta ( "                     \
   "meta_id          INTEGER PRIMARY KEY AUTOINCREMENT, " \
   "meta_name        TEXT    NOT NULL UNIQUE "            \
 ");"

#define CREATE_TABLE_DATA                                 \
 "CREATE TABLE IF NOT EXISTS data ( "                     \
   "data_id          INTEGER PRIMARY KEY AUTOINCREMENT, " \
   "data_value       TEXT    NOT NULL UNIQUE "            \
 ");"

#define CREATE_TABLE_GROUP                                \
 "CREATE TABLE IF NOT EXISTS grp ( "                      \
   "grp_id           INTEGER PRIMARY KEY AUTOINCREMENT, " \
   "grp_name         TEXT    NOT NULL UNIQUE "            \
 ");"

#define CREATE_TABLE_GRABBER                              \
 "CREATE TABLE IF NOT EXISTS grabber ( "                  \
   "grabber_id       INTEGER PRIMARY KEY AUTOINCREMENT, " \
   "grabber_name     INTEGER NOT NULL UNIQUE "            \
 ");"

#define CREATE_TABLE_DLCONTEXT                            \
 "CREATE TABLE IF NOT EXISTS dlcontext ( "                \
   "dlcontext_id     INTEGER PRIMARY KEY AUTOINCREMENT, " \
   "dlcontext_url    TEXT    NOT NULL, "                  \
   "dlcontext_dst    INTEGER NOT NULL, "                  \
   "dlcontext_name   TEXT    NOT NULL, "                  \
   "_file_id         INTEGER NULL "                       \
 ");"

#define CREATE_TABLE_ASSOC_FILE_METADATA                  \
 "CREATE TABLE IF NOT EXISTS assoc_file_metadata ( "      \
   "file_id          INTEGER NOT NULL, "                  \
   "meta_id          INTEGER NOT NULL, "                  \
   "data_id          INTEGER NOT NULL, "                  \
   "_grp_id          INTEGER NOT NULL, "                  \
   "external         INTEGER NOT NULL, "                  \
   "priority__       INTEGER NOT NULL, "                  \
   "PRIMARY KEY (file_id, meta_id, data_id) "             \
 ");"

#define CREATE_TABLE_ASSOC_FILE_GRABBER                   \
 "CREATE TABLE IF NOT EXISTS assoc_file_grabber ( "       \
   "file_id          INTEGER NOT NULL, "                  \
   "grabber_id       INTEGER NOT NULL, "                  \
   "PRIMARY KEY (file_id, grabber_id) "                   \
 ");"

/******************************************************************************/
/*                                                                            */
/*                              Create indexes                                */
/*                                                                            */
/******************************************************************************/

#define CREATE_INDEX_CHECKED      \
 "CREATE INDEX IF NOT EXISTS "    \
 "checked_idx ON file (checked__);"

#define CREATE_INDEX_INTERRUPTED  \
 "CREATE INDEX IF NOT EXISTS "    \
 "interrupted_idx ON file (interrupted__);"

#define CREATE_INDEX_OUTOFPATH    \
 "CREATE INDEX IF NOT EXISTS "    \
 "outofpath_idx ON file (outofpath__);"

#define CREATE_INDEX_ASSOC        \
 "CREATE INDEX IF NOT EXISTS "    \
 "assoc_idx ON assoc_file_metadata (meta_id, data_id);"

/* Unique */

#define CREATE_INDEX_FILE_PATH         \
 "CREATE UNIQUE INDEX IF NOT EXISTS "  \
 "file_path_idx ON file (file_path);"

#define CREATE_INDEX_TYPE_NAME         \
 "CREATE UNIQUE INDEX IF NOT EXISTS "  \
 "type_name_idx ON type (type_name);"

#define CREATE_INDEX_META_NAME         \
 "CREATE UNIQUE INDEX IF NOT EXISTS "  \
 "meta_name_idx ON meta (meta_name);"

#define CREATE_INDEX_DATA_VALUE        \
 "CREATE UNIQUE INDEX IF NOT EXISTS "  \
 "data_value_idx ON data (data_value);"

#define CREATE_INDEX_GROUP_NAME        \
 "CREATE UNIQUE INDEX IF NOT EXISTS "  \
 "grp_name_idx ON grp (grp_name);"

#define CREATE_INDEX_GRABBER_NAME      \
 "CREATE UNIQUE INDEX IF NOT EXISTS "  \
 "grabber_name_idx ON grabber (grabber_name);"

/******************************************************************************/
/*                                                                            */
/*                                  Select                                    */
/*                                                                            */
/******************************************************************************/

/* Unique file selection */

#define SELECT_FILE_FROM                                               \
 "SELECT file.file_id, assoc._grp_id, "                                \
        "meta.meta_id, data.data_id, "                                 \
        "meta.meta_name, data.data_value, assoc.external "             \
 "FROM (( "                                                            \
     "file INNER JOIN assoc_file_metadata AS assoc "                   \
     "ON file.file_id = assoc.file_id "                                \
   ") INNER JOIN data "                                                \
   "ON data.data_id = assoc.data_id "                                  \
 ") INNER JOIN meta "                                                  \
 "ON assoc.meta_id = meta.meta_id "

#define SELECT_FILE_END \
 "ORDER BY assoc.priority__;"

#define SELECT_FILE_WHERE_FILE_ID \
 "file.file_id = %"PRIi64" "
#define SELECT_FILE_WHERE_FILE_PATH \
 "file.file_path = '%q' "

/* File list selection */

#define SELECT_LIST_FILE_FROM           \
 "SELECT file_id, file_path, _type_id " \
 "FROM file AS assoc " /* "assoc" is a trick to factorize with the sub-query */

#define SELECT_LIST_FILE_END \
 "ORDER BY file_id;"

#define SELECT_LIST_WHERE_TYPE_ID \
 "_type_id = %"PRIi64" "

/* Metadata list selection */

#define SELECT_LIST_METADATA_FROM                         \
 "SELECT meta.meta_id, data.data_id, "                    \
        "meta.meta_name, data.data_value, "               \
        "assoc._grp_id, assoc.external "                  \
 "FROM ( "                                                \
   "data INNER JOIN assoc_file_metadata AS assoc "        \
   "ON data.data_id = assoc.data_id "                     \
 ") INNER JOIN meta "                                     \
 "ON assoc.meta_id = meta.meta_id "

#define SELECT_LIST_METADATA_WHERE_TYPE_ID  \
 "assoc.file_id IN ( "                      \
   "SELECT file_id "                        \
   "FROM file "                             \
   "WHERE _type_id = %"PRIi64" "            \
 ") "

#define SELECT_LIST_METADATA_END          \
 "GROUP BY assoc.meta_id, assoc.data_id " \
 "ORDER BY data.data_value;"

/* Common */

#define SELECT_LIST_WHERE \
 "WHERE "

#define SELECT_LIST_WHERE_SUB_IN \
   "assoc.file_id IN ( "
#define SELECT_LIST_WHERE_SUB_NOTIN \
   "assoc.file_id NOT IN ( "
#define SELECT_LIST_WHERE_SUB                          \
     "SELECT assoc.file_id "                           \
     "FROM ( "                                         \
       "data INNER JOIN assoc_file_metadata AS assoc " \
       "ON data.data_id = assoc.data_id "              \
     ") INNER JOIN meta "                              \
     "ON assoc.meta_id = meta.meta_id "
#define SELECT_LIST_WHERE_SUB_END \
   ") "

#define SELECT_LIST_AND \
 "AND "
#define SELECT_LIST_OR \
 "OR "
#define SELECT_LIST_WHERE_META_NAME \
 "meta.meta_name = '%q' "
#define SELECT_LIST_WHERE_META_ID \
 "meta.meta_id = %"PRIi64" "
#define SELECT_LIST_WHERE_DATA_NAME \
 "data.data_value = '%q' "
#define SELECT_LIST_WHERE_DATA_ID \
 "data.data_id = %"PRIi64" "
#define SELECT_LIST_WHERE_GROUP_ID \
 "assoc._grp_id = %"PRIi64" "
#define SELECT_LIST_WHERE_PRIORITY \
 "assoc.priority__ <= %i " /* << highest,  >> lowest */

/* Internal */

#define SELECT_INFO_VALUE \
 "SELECT info_value "     \
 "FROM info "             \
 "WHERE info_name = ?;"

#define SELECT_FILE_INTERRUP \
 "SELECT interrupted__ "     \
 "FROM file "                \
 "WHERE file_path = ?;"

#define SELECT_FILE_MTIME \
 "SELECT file_mtime "     \
 "FROM file "             \
 "WHERE file_path = ?;"

#define SELECT_TYPE_ID   \
 "SELECT type_id "       \
 "FROM type "            \
 "WHERE type_name = ?;"

#define SELECT_META_ID   \
 "SELECT meta_id "       \
 "FROM meta "            \
 "WHERE meta_name = ?;"

#define SELECT_DATA_ID   \
 "SELECT data_id "       \
 "FROM data "            \
 "WHERE data_value = ?;"

#define SELECT_GROUP_ID  \
 "SELECT grp_id "        \
 "FROM grp "             \
 "WHERE grp_name = ?;"

#define SELECT_GRABBER_ID \
 "SELECT grabber_id "     \
 "FROM grabber "          \
 "WHERE grabber_name = ?;"

#define SELECT_FILE_ID   \
 "SELECT file_id "       \
 "FROM file "            \
 "WHERE file_path = ?;"

#define SELECT_FILE_ID_BY_METADATA                    \
 "SELECT file.file_id "                               \
 "FROM (( "                                           \
     "file INNER JOIN assoc_file_metadata AS assoc "  \
     "ON file.file_id = assoc.file_id "               \
   ") INNER JOIN data "                               \
   "ON data.data_id = assoc.data_id "                 \
 ") INNER JOIN meta "                                 \
 "ON assoc.meta_id = meta.meta_id "                   \
 "WHERE file.file_path = ? AND meta.meta_name = ? AND data.data_value = ?;"

#define SELECT_ASSOC_FILE_METADATA \
 "SELECT _grp_id, external "       \
 "FROM assoc_file_metadata "       \
 "WHERE file_id = ? AND meta_id = ? AND data_id = ?;"

#define SELECT_FILE_CHECKED_CLEAR \
 "SELECT file_path "              \
 "FROM file "                     \
 "WHERE checked__ = 0 AND outofpath__ = 0;"

#define SELECT_FILE_OUTOFPATH_SET \
 "SELECT file_path "              \
 "FROM file "                     \
 "WHERE outofpath__ = 1;"

#define SELECT_FILE_GRABBER_NAME                      \
 "SELECT grabber.grabber_name "                       \
 "FROM ( "                                            \
   "grabber INNER JOIN assoc_file_grabber AS assoc "  \
   "ON grabber.grabber_id = assoc.grabber_id "        \
 ") INNER JOIN file "                                 \
 "ON assoc.file_id = file.file_id "                   \
 "WHERE file.file_path = ?;"

#define SELECT_FILE_DLCONTEXT                           \
 "SELECT dlcontext_url, dlcontext_dst, dlcontext_name " \
 "FROM dlcontext INNER JOIN file "                      \
 "ON dlcontext._file_id = file.file_id "                \
 "WHERE file.file_path = ?;"

/******************************************************************************/
/*                                                                            */
/*                                  Insert                                    */
/*                                                                            */
/******************************************************************************/

#define INSERT_INFO                   \
 "INSERT "                            \
 "INTO info (info_name, info_value) " \
 "VALUES (?, ?);"

#define INSERT_FILE           \
 "INSERT "                    \
 "INTO file (file_path, "     \
 "           file_mtime, "    \
 "           checked__, "     \
 "           interrupted__, " \
 "           outofpath__) "   \
 "VALUES (?, ?, 1, -1, ?);"

#define INSERT_TYPE        \
 "INSERT "                 \
 "INTO type (type_name) "  \
 "VALUES (?);"

#define INSERT_META        \
 "INSERT "                 \
 "INTO meta (meta_name) "  \
 "VALUES (?);"

#define INSERT_DATA        \
 "INSERT "                 \
 "INTO data (data_value) " \
 "VALUES (?);"

#define INSERT_GROUP       \
 "INSERT "                 \
 "INTO grp (grp_name) "    \
 "VALUES (?);"

#define INSERT_GRABBER          \
 "INSERT "                      \
 "INTO grabber (grabber_name) " \
 "VALUES (?);"

#define INSERT_DLCONTEXT                                                    \
 "INSERT "                                                                  \
 "INTO dlcontext (dlcontext_url, dlcontext_dst, dlcontext_name, _file_id) " \
 "VALUES (?, ?, ?, ?);"

#define INSERT_ASSOC_FILE_METADATA                                          \
 "INSERT "                                                                  \
 "INTO assoc_file_metadata (file_id, meta_id, data_id, "                    \
                            "_grp_id, external, priority__) "               \
 "VALUES (?, ?, ?, ?, ?, ?);"

#define INSERT_ASSOC_FILE_GRABBER                                 \
 "INSERT "                                                        \
 "INTO assoc_file_grabber (file_id, grabber_id) "                 \
 "VALUES (?, ?);"

/******************************************************************************/
/*                                                                            */
/*                                  Update                                    */
/*                                                                            */
/******************************************************************************/

#define UPDATE_INFO     \
 "UPDATE info "         \
 "SET info_value = ? "  \
 "WHERE info_name = ?;"

#define UPDATE_FILE          \
 "UPDATE file "              \
 "SET file_mtime      = ?, " \
 "    checked__       = 1, " \
 "    interrupted__   = 1, " \
 "    outofpath__     = ?, " \
 "    _type_id        = ?  " \
 "WHERE file_path = ?;"

#define UPDATE_FILE_CHECKED_CLEAR \
 "UPDATE file "                   \
 "SET checked__ = 0;"

#define UPDATE_FILE_INTERRUP_CLEAR \
 "UPDATE file "                    \
 "SET interrupted__ = 0 "          \
 "WHERE file_path = ?;"

#define UPDATE_FILE_INTERRUP_FIX \
 "UPDATE file "                  \
 "SET interrupted__ = 1 "        \
 "WHERE interrupted__ = -1;"

#define UPDATE_ASSOC_FILE_METADATA \
 "UPDATE assoc_file_metadata "     \
 "SET _grp_id  = ?, "              \
 "    external = ?, "              \
 "    priority__ = ? "             \
 "WHERE file_id = ? AND meta_id = ? AND data_id = ?; "

/******************************************************************************/
/*                                                                            */
/*                                  Delete                                    */
/*                                                                            */
/******************************************************************************/

#define DELETE_FILE  \
 "DELETE FROM file " \
 "WHERE file_path = ?;"

#define DELETE_ASSOC_FILE_METADATA  \
 "DELETE FROM assoc_file_metadata " \
 "WHERE file_id = ? AND external = 0;"

#define DELETE_ASSOC_FILE_METADATA2  \
 "DELETE FROM assoc_file_metadata "  \
 "WHERE file_id = ? AND meta_id = ? AND data_id = ?;"

#define DELETE_ASSOC_FILE_GRABBER   \
 "DELETE FROM assoc_file_grabber "  \
 "WHERE file_id = ?;"

#define DELETE_DLCONTEXT  \
 "DELETE FROM dlcontext;"

/* Cleanup */

#define CLEANUP_META                \
 "DELETE FROM meta "                \
 "WHERE meta_id NOT IN ( "          \
   "SELECT meta_id "                \
   "FROM assoc_file_metadata "      \
 ");"

#define CLEANUP_DATA                \
 "DELETE FROM data "                \
 "WHERE data_id NOT IN ( "          \
   "SELECT data_id "                \
   "FROM assoc_file_metadata "      \
 ");"

#define CLEANUP_GRABBER             \
 "DELETE FROM grabber "             \
 "WHERE grabber_id NOT IN ( "       \
   "SELECT grabber_id "             \
   "FROM assoc_file_grabber "       \
 ");"

#define CLEANUP_ASSOC_FILE_METADATA \
 "DELETE FROM assoc_file_metadata " \
 "WHERE file_id NOT IN ( "          \
   "SELECT file_id "                \
   "FROM file "                     \
 ");"

#define CLEANUP_ASSOC_FILE_GRABBER  \
 "DELETE FROM assoc_file_grabber "  \
 "WHERE file_id NOT IN ( "          \
   "SELECT file_id "                \
   "FROM file "                     \
 ");"

#endif /* VALHALLA_SQL_STATEMENTS_H */
