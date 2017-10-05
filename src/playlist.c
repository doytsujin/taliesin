/**
 *
 * Taliesin - Media server
 * 
 * Playlists functions definitions
 *
 * Copyright 2017 Nicolas Mora <mail@babelouest.org>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU GENERAL PUBLIC LICENSE
 * License as published by the Free Software Foundation;
 * version 3 of the License.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU GENERAL PUBLIC LICENSE for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

#include "taliesin.h"

json_t * playlist_get_stream_attached(struct config_elements * config, json_int_t tpl_id) {
	json_t * j_return = NULL, * j_stream_array;
	int i;
	
	if (tpl_id) {
		j_stream_array = json_array();
		if (j_stream_array != NULL) {
			for (i=0; i<config->nb_webradio; i++) {
				if (config->webradio_set[i]->tpl_id == tpl_id) {
					json_array_append_new(j_stream_array, json_pack("{ssssso}", "name", config->webradio_set[i]->name, "display_name", config->webradio_set[i]->display_name, "webradio", json_true()));
				}
			}
			for (i=0; i<config->nb_jukebox; i++) {
				if (config->jukebox_set[i]->tpl_id == tpl_id) {
					json_array_append_new(j_stream_array, json_pack("{ssssso}", "name", config->jukebox_set[i]->name, "display_name", config->jukebox_set[i]->display_name, "webradio", json_false()));
				}
			}
			j_return = json_pack("{siso}", "result", T_OK, "stream", j_stream_array);
		} else {
			y_log_message(Y_LOG_LEVEL_ERROR, "playlist_get_stream_attached - Error allocating resources for j_stream_array");
			j_return = json_pack("{si}", "result", T_ERROR_MEMORY);
		}
	} else {
		y_log_message(Y_LOG_LEVEL_ERROR, "playlist_get_stream_attached - Error tpl_id is null");
		j_return = json_pack("{si}", "result", T_ERROR_PARAM);
	}
	return j_return;
}

json_t * playlist_get_media_list(struct config_elements * config, json_int_t tpl_id, int with_id, size_t offset, size_t limit) {
  json_t * j_result, * j_return;
  int res;
  char * query, * tmp;
  
  query = msprintf("SELECT %s`%s`.`tds_name` AS data_source, %s`%s`.`tm_path` AS path FROM `%s`, `%s`, `%s` "
                   "WHERE `%s`.`tpl_id`=%" JSON_INTEGER_FORMAT " AND `%s`.`tm_id`=`%s`.`tm_id` "
                   "AND `%s`.`tds_id`=`%s`.`tds_id` ORDER BY `%s`.`tds_name`%s, `%s`.`tm_path`%s, `%s`.`tm_name`%s",
                   with_id?"`"TALIESIN_TABLE_DATA_SOURCE"`.`tds_path` AS tds_path, ":"" , TALIESIN_TABLE_DATA_SOURCE,
                   (with_id?"`"TALIESIN_TABLE_MEDIA"`.`tm_id` AS tm_id, ":""), TALIESIN_TABLE_MEDIA, TALIESIN_TABLE_DATA_SOURCE, TALIESIN_TABLE_MEDIA,
                   TALIESIN_TABLE_PLAYLIST_ELEMENT, TALIESIN_TABLE_PLAYLIST_ELEMENT, tpl_id, TALIESIN_TABLE_PLAYLIST_ELEMENT, TALIESIN_TABLE_MEDIA,
                   TALIESIN_TABLE_MEDIA, TALIESIN_TABLE_DATA_SOURCE, TALIESIN_TABLE_DATA_SOURCE, config->conn->type==HOEL_DB_TYPE_MARIADB?"":" COLLATE NOCASE",
									 TALIESIN_TABLE_MEDIA, config->conn->type==HOEL_DB_TYPE_MARIADB?"":" COLLATE NOCASE",
									 TALIESIN_TABLE_MEDIA, config->conn->type==HOEL_DB_TYPE_MARIADB?"":" COLLATE NOCASE",
                   offset);
  if (limit) {
    tmp = msprintf("%s LIMIT %zu", query, limit);
    o_free(query);
    query = tmp;
  }
  if (offset) {
    tmp = msprintf("%s OFFSET %zu", query, limit);
    o_free(query);
    query = tmp;
  }
  res = h_execute_query_json(config->conn, query, &j_result);
  o_free(query);
  if (res == H_OK) {
    j_return = json_pack("{siso}", "result", T_OK, "media", j_result);
  } else {
    y_log_message(Y_LOG_LEVEL_ERROR, "playlist_get_media - Error executing j_query (1)");
    j_return = json_pack("{si}", "result", T_ERROR_DB);
  }
  return j_return;
}

json_t * playlist_count_element(struct config_elements * config, json_int_t tpl_id) {
  json_t * j_query, * j_result, * j_return;
  int res;
  
  j_query = json_pack("{sss[s]s{sI}}",
                      "table",
                      TALIESIN_TABLE_PLAYLIST_ELEMENT,
                      "columns",
                        "COUNT(`tpe_id`) AS `count`",
                      "where",
                        "tpl_id",
                        tpl_id);
  res = h_select(config->conn, j_query, &j_result, NULL);
  json_decref(j_query);
  if (res == H_OK) {
    if (json_array_size(j_result)) {
      j_return = json_pack("{sisI}", "result", T_OK, "count", json_integer_value(json_object_get(json_array_get(j_result, 0), "count")));
    } else {
      y_log_message(Y_LOG_LEVEL_ERROR, "playlist_count_element - Error no result");
      j_return = json_pack("{si}", "result", T_ERROR);
    }
    json_decref(j_result);
  } else {
    y_log_message(Y_LOG_LEVEL_ERROR, "playlist_count_element - Error executing j_query");
    j_return = json_pack("{si}", "result", T_ERROR_DB);
  }
  return j_return;
}

json_t * playlist_list(struct config_elements * config, const char * username) {
  json_t * j_query, * j_result, * j_element, * j_return, * j_count, * j_stream;
  int res;
  char * clause_username, * temp;
  size_t index;
  
  temp = h_escape_string(config->conn, username);
  clause_username = msprintf("`tpl_username` = '%s' OR `tpl_username` IS NULL", temp);
  o_free(temp);
  j_query = json_pack("{sss[sssssssss]s{s{ssss}}}",
                      "table",
                      TALIESIN_TABLE_PLAYLIST,
                      "columns",
                        "tpl_id",
                        "tpl_name AS name",
                        "tpl_description AS description",
                        "tpl_username",
                        "tpl_webradio_startup",
                        "tpl_webradio_startup_format",
                        "tpl_webradio_startup_channels",
                        "tpl_webradio_startup_sample_rate",
                        "tpl_webradio_startup_bit_rate",
                      "where",
                        " ",
                          "operator",
                          "raw",
                          "value",
                          clause_username);
  o_free(clause_username);
  res = h_select(config->conn, j_query, &j_result, NULL);
  json_decref(j_query);
  if (res == H_OK) {
    json_array_foreach(j_result, index, j_element) {
      j_count = playlist_count_element(config, json_integer_value(json_object_get(j_element, "tpl_id")));
      if (check_result_value(j_count, T_OK)) {
        json_object_set_new(j_element, "elements", json_integer(json_integer_value(json_object_get(j_count, "count"))));
      }
      json_decref(j_count);
			j_stream = playlist_get_stream_attached(config, json_integer_value(json_object_get(j_element, "tpl_id")));
      if (check_result_value(j_stream, T_OK)) {
        json_object_set(j_element, "stream", json_object_get(j_stream, "stream"));
      }
			json_decref(j_stream);
      if (json_object_get(j_element, "tpl_username")  != json_null()) {
        json_object_set_new(j_element, "scope", json_string(TALIESIN_SCOPE_ME));
      } else {
        json_object_set_new(j_element, "scope", json_string(TALIESIN_SCOPE_ALL));
      }
      json_object_set_new(j_element, "webradio_startup", json_object());
      if (json_integer_value(json_object_get(j_element, "tpl_webradio_startup")) == TALIESIN_STORED_PLAYLIST_WEBRADIO_STARTUP_NO_RANDOM) {
        json_object_set_new(json_object_get(j_element, "webradio_startup"), "status", json_string("no random"));
        json_object_set(json_object_get(j_element, "webradio_startup"), "format", json_object_get(j_element, "tpl_webradio_startup_format"));
        json_object_set(json_object_get(j_element, "webradio_startup"), "channels", json_object_get(j_element, "tpl_webradio_startup_channels"));
        json_object_set(json_object_get(j_element, "webradio_startup"), "sample_rate", json_object_get(j_element, "tpl_webradio_startup_sample_rate"));
        json_object_set(json_object_get(j_element, "webradio_startup"), "bitrate", json_object_get(j_element, "tpl_webradio_startup_bit_rate"));
      } else if (json_integer_value(json_object_get(j_element, "tpl_webradio_startup")) == TALIESIN_STORED_PLAYLIST_WEBRADIO_STARTUP_RANDOM) {
        json_object_set_new(json_object_get(j_element, "webradio_startup"), "status", json_string("random"));
        json_object_set(json_object_get(j_element, "webradio_startup"), "format", json_object_get(j_element, "tpl_webradio_startup_format"));
        json_object_set(json_object_get(j_element, "webradio_startup"), "channels", json_object_get(j_element, "tpl_webradio_startup_channels"));
        json_object_set(json_object_get(j_element, "webradio_startup"), "sample_rate", json_object_get(j_element, "tpl_webradio_startup_sample_rate"));
        json_object_set(json_object_get(j_element, "webradio_startup"), "bitrate", json_object_get(j_element, "tpl_webradio_startup_bit_rate"));
      } else {
        json_object_set_new(json_object_get(j_element, "webradio_startup"), "status", json_string("off"));
      }
      json_object_del(j_element, "tpl_webradio_startup");
      json_object_del(j_element, "webradio_startup_format");
      json_object_del(j_element, "webradio_startup_channels");
      json_object_del(j_element, "webradio_startup_sample_rate");
      json_object_del(j_element, "webradio_startup_bit_rate");
      json_object_del(j_element, "tpl_id");
      json_object_del(j_element, "tpl_username");
    }
    j_return = json_pack("{sisO}", "result", T_OK, "playlist", j_result);
    json_decref(j_result);
  } else {
    y_log_message(Y_LOG_LEVEL_ERROR, "playlist_list - Error executing j_query");
    j_return = json_pack("{si}", "result", T_ERROR_DB);
  }
  return j_return;
}

json_t * playlist_get(struct config_elements * config, const char * username, const char * name, int with_id, size_t offset, size_t limit) {
  json_t * j_query, * j_result, * j_media, * j_return, * j_count, * j_stream;
  int res;
  char * clause_username, * temp;
  
  temp = h_escape_string(config->conn, username);
  clause_username = msprintf("(`tpl_username` = '%s' OR `tpl_username` IS NULL)", temp);
  o_free(temp);
  j_query = json_pack("{sss[sssssssss]s{s{ssss}ss}}",
                      "table",
                      TALIESIN_TABLE_PLAYLIST,
                      "columns",
                        "tpl_id",
                        "tpl_name AS name",
                        "tpl_description AS description",
                        "tpl_username",
                        "tpl_webradio_startup",
                        "tpl_webradio_startup_format",
                        "tpl_webradio_startup_channels",
                        "tpl_webradio_startup_sample_rate",
                        "tpl_webradio_startup_bit_rate",
                      "where",
                        " ",
                          "operator",
                          "raw",
                          "value",
                          clause_username,
                        "tpl_name",
                        name);
  o_free(clause_username);
  res = h_select(config->conn, j_query, &j_result, NULL);
  json_decref(j_query);
  if (res == H_OK) {
    if (json_array_size(j_result) > 0) {
      j_count = playlist_count_element(config, json_integer_value(json_object_get(json_array_get(j_result, 0), "tpl_id")));
      if (check_result_value(j_count, T_OK)) {
        json_object_set_new(json_array_get(j_result, 0), "elements", json_integer(json_integer_value(json_object_get(j_count, "count"))));
      }
      json_decref(j_count);
			j_stream = playlist_get_stream_attached(config, json_integer_value(json_object_get(json_array_get(j_result, 0), "tpl_id")));
      if (check_result_value(j_stream, T_OK)) {
        json_object_set(json_array_get(j_result, 0), "stream", json_object_get(j_stream, "stream"));
      }
			json_decref(j_stream);
      j_media = playlist_get_media_list(config, json_integer_value(json_object_get(json_array_get(j_result, 0), "tpl_id")), with_id, offset, limit);
      if (check_result_value(j_media, T_OK)) {
        json_object_set(json_array_get(j_result, 0), "media", json_object_get(j_media, "media"));
      } else {
        y_log_message(Y_LOG_LEVEL_ERROR, "playlist_get - Error check_result_value");
      }
      if (json_object_get(json_array_get(j_result, 0), "tpl_username")  != json_null()) {
        json_object_set_new(json_array_get(j_result, 0), "scope", json_string(TALIESIN_SCOPE_ME));
      } else {
        json_object_set_new(json_array_get(j_result, 0), "scope", json_string(TALIESIN_SCOPE_ALL));
      }
      json_object_set_new(json_array_get(j_result, 0), "webradio_startup", json_object());
      if (json_integer_value(json_object_get(json_array_get(j_result, 0), "tpl_webradio_startup")) == TALIESIN_STORED_PLAYLIST_WEBRADIO_STARTUP_NO_RANDOM) {
        json_object_set_new(json_object_get(json_array_get(j_result, 0), "webradio_startup"), "status", json_string("no random"));
        json_object_set(json_object_get(json_array_get(j_result, 0), "webradio_startup"), "format", json_object_get(json_array_get(j_result, 0), "tpl_webradio_startup_format"));
        json_object_set(json_object_get(json_array_get(j_result, 0), "webradio_startup"), "channels", json_object_get(json_array_get(j_result, 0), "tpl_webradio_startup_channels"));
        json_object_set(json_object_get(json_array_get(j_result, 0), "webradio_startup"), "sample_rate", json_object_get(json_array_get(j_result, 0), "tpl_webradio_startup_sample_rate"));
        json_object_set(json_object_get(json_array_get(j_result, 0), "webradio_startup"), "bitrate", json_object_get(json_array_get(j_result, 0), "tpl_webradio_startup_bit_rate"));
      } else if (json_integer_value(json_object_get(json_array_get(j_result, 0), "tpl_webradio_startup")) == TALIESIN_STORED_PLAYLIST_WEBRADIO_STARTUP_RANDOM) {
        json_object_set_new(json_object_get(json_array_get(j_result, 0), "webradio_startup"), "status", json_string("random"));
        json_object_set(json_object_get(json_array_get(j_result, 0), "webradio_startup"), "format", json_object_get(json_array_get(j_result, 0), "tpl_webradio_startup_format"));
        json_object_set(json_object_get(json_array_get(j_result, 0), "webradio_startup"), "channels", json_object_get(json_array_get(j_result, 0), "tpl_webradio_startup_channels"));
        json_object_set(json_object_get(json_array_get(j_result, 0), "webradio_startup"), "sample_rate", json_object_get(json_array_get(j_result, 0), "tpl_webradio_startup_sample_rate"));
        json_object_set(json_object_get(json_array_get(j_result, 0), "webradio_startup"), "bitrate", json_object_get(json_array_get(j_result, 0), "tpl_webradio_startup_bit_rate"));
      } else {
        json_object_set_new(json_object_get(json_array_get(j_result, 0), "webradio_startup"), "status", json_string("off"));
      }
      json_decref(j_media);
      if (!with_id) {
        json_object_del(json_array_get(j_result, 0), "tpl_id");
      }
      json_object_del(json_array_get(j_result, 0), "tpl_webradio_startup");
      json_object_del(json_array_get(j_result, 0), "tpl_webradio_startup_format");
      json_object_del(json_array_get(j_result, 0), "tpl_webradio_startup_channels");
      json_object_del(json_array_get(j_result, 0), "tpl_webradio_startup_sample_rate");
      json_object_del(json_array_get(j_result, 0), "tpl_webradio_startup_bit_rate");
      json_object_del(json_array_get(j_result, 0), "tpl_username");
      j_return = json_pack("{sisO}", "result", T_OK, "playlist", json_array_get(j_result, 0));
    } else {
      j_return = json_pack("{si}", "result", T_ERROR_NOT_FOUND);
    }
    json_decref(j_result);
  } else {
    y_log_message(Y_LOG_LEVEL_ERROR, "playlist_get - Error executing j_query");
    j_return = json_pack("{si}", "result", T_ERROR_DB);
  }
  return j_return;
}

json_t * playlist_get_by_id(struct config_elements * config, json_int_t tpl_id) {
  json_t * j_query, * j_result, * j_media, * j_return;
  int res;
  
  j_query = json_pack("{sss[ssss]s{sI}}",
                      "table",
                      TALIESIN_TABLE_PLAYLIST,
                      "columns",
                        "tpl_id",
                        "tpl_name AS name",
                        "tpl_description AS description",
                        "tpl_username",
                      "where",
                        "tpl_id",
                        tpl_id);
  res = h_select(config->conn, j_query, &j_result, NULL);
  json_decref(j_query);
  if (res == H_OK) {
    if (json_array_size(j_result) > 0) {
      j_media = playlist_get_media_list(config, json_integer_value(json_object_get(json_array_get(j_result, 0), "tpl_id")), 1, 0, 0);
      if (check_result_value(j_media, T_OK)) {
        json_object_set(json_array_get(j_result, 0), "media", json_object_get(j_media, "media"));
      } else {
        y_log_message(Y_LOG_LEVEL_ERROR, "playlist_get - Error check_result_value");
      }
      if (json_object_get(json_array_get(j_result, 0), "tpl_username")  != json_null()) {
        json_object_set_new(json_array_get(j_result, 0), "scope", json_string(TALIESIN_SCOPE_ME));
      } else {
        json_object_set_new(json_array_get(j_result, 0), "scope", json_string(TALIESIN_SCOPE_ALL));
      }
      json_decref(j_media);
      json_object_del(json_array_get(j_result, 0), "tpl_username");
      j_return = json_pack("{sisO}", "result", T_OK, "playlist", json_array_get(j_result, 0));
    } else {
      j_return = json_pack("{si}", "result", T_ERROR_NOT_FOUND);
    }
    json_decref(j_result);
  } else {
    y_log_message(Y_LOG_LEVEL_ERROR, "playlist_get - Error executing j_query");
    j_return = json_pack("{si}", "result", T_ERROR_DB);
  }
  return j_return;
}

json_t * is_playlist_element_list_valid(struct config_elements * config, int is_admin, const char * username, json_t * j_element_list) {
  json_t * j_return = json_array(), * j_element;
  size_t index;
  
  if (j_return != NULL) {
    if (!json_is_array(j_element_list) || !json_array_size(j_element_list)) {
      json_array_append_new(j_return, json_pack("{ss}", "media", "You must set at least one media of format {data_source, path}"));
    } else {
      json_array_foreach(j_element_list, index, j_element) {
        if (!is_valid_jukebox_element_parameter(config, j_element, username, is_admin)) {
          json_array_append_new(j_return, json_pack("{ss}", "media", "media is not a valid {data_source, path} object or does not exist"));
        }
      }
    }
  } else {
    y_log_message(Y_LOG_LEVEL_ERROR, "is_playlist_valid - Error allocating resources for j_return");
  }
  return j_return;
}

json_t * is_playlist_valid(struct config_elements * config, const char * username, int is_admin, json_t * j_playlist, int update) {
  json_t * j_return = json_array(), * j_element = NULL, * j_temp;
  size_t index;
  const char * format = NULL;
  unsigned short int channels = 0;
  unsigned int sample_rate = 0, bit_rate = 0;
  
  if (j_return != NULL) {
    if (j_playlist == NULL || !json_is_object(j_playlist)) {
      json_array_append_new(j_return, json_pack("{ss}", "playlist", "playlist must be a valid JSON object"));
    } else {
      if (!update) {
        if (json_object_get(j_playlist, "name") == NULL || !json_is_string(json_object_get(j_playlist, "name")) || json_string_length(json_object_get(j_playlist, "name")) == 0 || json_string_length(json_object_get(j_playlist, "name")) > 128) {
          json_array_append_new(j_return, json_pack("{ss}", "name", "name is mandatory and must be a non empty string of maximum 128 characters"));
        } else {
          j_element = playlist_get(config, username, json_string_value(json_object_get(j_playlist, "name")), 0, 0, 1);
          if (check_result_value(j_element, T_OK)) {
            json_array_append_new(j_return, json_pack("{ss}", "name", "playlist name already exist"));
          }
          json_decref(j_element);
        }
      }
      
      if (json_object_get(j_playlist, "description") != NULL && (!json_is_string(json_object_get(j_playlist, "description")) || json_string_length(json_object_get(j_playlist, "description")) > 512)) {
        json_array_append_new(j_return, json_pack("{ss}", "description", "description is an optional string of maximum 512 characters"));
      }
      
      if (json_object_get(j_playlist, "cover") != NULL && !json_is_string(json_object_get(j_playlist, "cover"))) {
        json_array_append_new(j_return, json_pack("{ss}", "cover", "cover is an optional string"));
      } else if (json_object_get(j_playlist, "cover") != NULL && is_valid_b64_image((unsigned char *)json_string_value(json_object_get(j_playlist, "cover"))) != T_OK) {
				json_array_append_new(j_return, json_pack("{ss}", "cover", "cover is not a valid image encoded in base64"));
			}
      
      if (json_object_get(j_playlist, "media") == NULL || !json_is_array(json_object_get(j_playlist, "media")) || !json_array_size(json_object_get(j_playlist, "media"))) {
        json_array_append_new(j_return, json_pack("{ss}", "media", "You must set at least one media of format {data_source, path}"));
      } else {
        json_array_foreach(json_object_get(j_playlist, "media"), index, j_element) {
          if (!is_valid_jukebox_element_parameter(config, j_element, username, is_admin)) {
            json_array_append_new(j_return, json_pack("{ss}", "media", "media is not a valid {data_source, path} object or does not exist"));
          }
        }
      }

      if (json_object_get(j_playlist, "scope") != NULL &&
          (
            !json_is_string(json_object_get(j_playlist, "scope")) ||
            (
              o_strcmp(json_string_value(json_object_get(j_playlist, "scope")), TALIESIN_SCOPE_ALL) != 0 &&
              o_strcmp(json_string_value(json_object_get(j_playlist, "scope")), TALIESIN_SCOPE_ME) != 0
            )
            || (o_strcmp(json_string_value(json_object_get(j_playlist, "scope")), TALIESIN_SCOPE_ALL) == 0 && !is_admin)
          )
        ) {
        json_array_append_new(j_return, json_pack("{ss}", "scope", "scope value is an optional string and can be only " TALIESIN_SCOPE_ALL " or " TALIESIN_SCOPE_ME ", only administrator can add playlists for all users"));
      }
      
      if (json_object_get(j_playlist, "webradio_startup") != NULL && (!json_is_object(json_object_get(j_playlist, "webradio_startup")))) {
        json_array_append_new(j_return, json_pack("{ss}", "webradio_startup", "webradio_startup is optional and must be a JSON object"));
      } else if (json_object_get(j_playlist, "webradio_startup") != NULL) {
        if (json_object_get(json_object_get(j_playlist, "webradio_startup"), "status") != NULL && (!json_is_string(json_object_get(json_object_get(j_playlist, "webradio_startup"), "status")) || 
        (0 != o_strcmp(json_string_value(json_object_get(json_object_get(j_playlist, "webradio_startup"), "status")), "off") &&
         0 != o_strcmp(json_string_value(json_object_get(json_object_get(j_playlist, "webradio_startup"), "status")), "no random") &&
         0 != o_strcmp(json_string_value(json_object_get(json_object_get(j_playlist, "webradio_startup"), "status")), "random")))) {
          json_array_append_new(j_return, json_pack("{ss}", "webradio_startup status", "webradio_startup status is optional and must have one of the following values: 'off', 'random', 'no random'"));
        }
        
        if (0 == o_strcmp(json_string_value(json_object_get(json_object_get(j_playlist, "webradio_startup"), "status")), "no random") ||
            0 == o_strcmp(json_string_value(json_object_get(json_object_get(j_playlist, "webradio_startup"), "status")), "random")) {
          if (json_object_get(json_object_get(j_playlist, "webradio_startup"), "format") != NULL && !json_is_string(json_object_get(json_object_get(j_playlist, "webradio_startup"), "format"))) {
            json_array_append_new(j_return, json_pack("{ss}", "webradio_startup format", "webradio_startup format is optional and must be a string of one of the available formats"));
          } else if (json_object_get(json_object_get(j_playlist, "webradio_startup"), "format") != NULL) {
            format = json_string_value(json_object_get(json_object_get(j_playlist, "webradio_startup"), "format"));
          }
          if (json_object_get(json_object_get(j_playlist, "webradio_startup"), "channels") != NULL && !json_is_integer(json_object_get(json_object_get(j_playlist, "webradio_startup"), "channels"))) {
            json_array_append_new(j_return, json_pack("{ss}", "webradio_startup channels", "webradio_startup channels is optional and must be 1 or 2"));
          } else if (json_object_get(json_object_get(j_playlist, "webradio_startup"), "channels") != NULL) {
            channels = (int)json_integer_value(json_object_get(json_object_get(j_playlist, "webradio_startup"), "channels"));
          }
          if (json_object_get(json_object_get(j_playlist, "webradio_startup"), "samplerate") != NULL && !json_is_integer(json_object_get(json_object_get(j_playlist, "webradio_startup"), "samplerate"))) {
            json_array_append_new(j_return, json_pack("{ss}", "webradio_startup samplerate", "webradio_startup samplerate is optional and must be an integer"));
          } else if (json_object_get(json_object_get(j_playlist, "webradio_startup"), "samplerate") != NULL) {
            sample_rate = (int)json_integer_value(json_object_get(json_object_get(j_playlist, "webradio_startup"), "samplerate"));
          }
          if (0 == o_strcmp(format, "flac")) {
            bit_rate = TALIESIN_STREAM_FLAC_BIT_RATE;
          } else {
            if (json_object_get(json_object_get(j_playlist, "webradio_startup"), "bitrate") != NULL && !json_is_integer(json_object_get(json_object_get(j_playlist, "webradio_startup"), "bitrate"))) {
              json_array_append_new(j_return, json_pack("{ss}", "webradio_startup bitrate", "webradio_startup bitrate is optional and must be an integer"));
            } else if (json_object_get(json_object_get(j_playlist, "webradio_startup"), "bitrate") != NULL) {
              bit_rate = (int)json_integer_value(json_object_get(json_object_get(j_playlist, "webradio_startup"), "bitrate"));
            }
          }
          if (format == NULL) {
            format = TALIESIN_STREAM_DEFAULT_FORMAT;
          }
          if (!sample_rate) {
            sample_rate = TALIESIN_STREAM_DEFAULT_SAMPLE_RATE;
          }
          if (!channels) {
            channels = TALIESIN_STREAM_DEFAULT_CHANNELS;
          }
          if (!bit_rate) {
            bit_rate = TALIESIN_STREAM_DEFAULT_BIT_RATE;
          }
          j_temp = is_stream_parameters_valid(format, channels, sample_rate, bit_rate);
          json_array_extend(j_return, j_temp);
          json_decref(j_temp);
        }
      }
    }
  } else {
    y_log_message(Y_LOG_LEVEL_ERROR, "is_playlist_valid - Error allocating resources for j_return");
  }
  return j_return;
}

int db_playlist_replace_element_list(struct config_elements * config, json_int_t tpl_id, json_t * j_playlist) {
  json_t * j_query, * j_element;
  int res, ret;
  size_t index;
  
  // Delete all existing elements
  j_query = json_pack("{sss{sI}}",
                      "table",
                      TALIESIN_TABLE_PLAYLIST_ELEMENT,
                      "where",
                        "tpl_id",
                        tpl_id);
  res = h_delete(config->conn, j_query, NULL);
  json_decref(j_query);
  if (res == H_OK) {
    // Insert new elements
    j_query = json_pack("{sss[]}",
                        "table",
                        TALIESIN_TABLE_PLAYLIST_ELEMENT,
                        "values");
    if (j_query != NULL) {
      json_array_foreach(json_object_get(j_playlist, "media"), index, j_element) {
        json_array_append_new(json_object_get(j_query, "values"), json_pack("{sIsO}", "tpl_id", tpl_id, "tm_id", json_object_get(j_element, "tm_id")));
      }
      res = h_insert(config->conn, j_query, NULL);
      json_decref(j_query);
      if (res != H_OK) {
        y_log_message(Y_LOG_LEVEL_ERROR, "db_playlist_replace_element_list - Error executing j_query (2)");
        ret = T_ERROR_DB;
      } else {
        ret = T_OK;
      }
    } else {
      y_log_message(Y_LOG_LEVEL_ERROR, "db_playlist_replace_element_list - Error allocating resources for j_query");
      ret = T_ERROR_MEMORY;
    }
  } else {
    y_log_message(Y_LOG_LEVEL_ERROR, "db_playlist_replace_element_list - Error executing j_query (1)");
    ret = T_ERROR_DB;
  }
  return ret;
}

int playlist_add(struct config_elements * config, const char * username, json_t * j_playlist) {
  json_t * j_query, * j_last_id;
  json_int_t tic_id = 0, tpl_id;
  int res, ret, webradio_startup = TALIESIN_STORED_PLAYLIST_WEBRADIO_STARTUP_OFF;
  const char * format = TALIESIN_STREAM_DEFAULT_FORMAT;
  unsigned short int channels = TALIESIN_STREAM_DEFAULT_CHANNELS;
  unsigned int sample_rate = TALIESIN_STREAM_DEFAULT_SAMPLE_RATE, bit_rate = TALIESIN_STREAM_DEFAULT_BIT_RATE;
  
	if (json_object_get(j_playlist, "cover") != NULL) {
		tic_id = media_cover_save(config, 0, (const unsigned char *)json_string_value(json_object_get(j_playlist, "cover")));
	}
  
  if (json_object_get(j_playlist, "webradio_startup") != NULL) {
    if (0 == o_strcmp("no random", json_string_value(json_object_get(json_object_get(j_playlist, "webradio_startup"), "status")))) {
      webradio_startup = TALIESIN_STORED_PLAYLIST_WEBRADIO_STARTUP_NO_RANDOM;
    } else if (0 == o_strcmp("random", json_string_value(json_object_get(json_object_get(j_playlist, "webradio_startup"), "status")))) {
      webradio_startup = TALIESIN_STORED_PLAYLIST_WEBRADIO_STARTUP_RANDOM;
    }
    if (0 == o_strcmp(json_string_value(json_object_get(json_object_get(j_playlist, "webradio_startup"), "status")), "no random") ||
        0 == o_strcmp(json_string_value(json_object_get(json_object_get(j_playlist, "webradio_startup"), "status")), "random")) {
      if (json_object_get(json_object_get(j_playlist, "webradio_startup"), "format") != NULL) {
        format = json_string_value(json_object_get(json_object_get(j_playlist, "webradio_startup"), "format"));
      }
      if (json_object_get(json_object_get(j_playlist, "webradio_startup"), "channels") != NULL) {
        channels = (int)json_integer_value(json_object_get(json_object_get(j_playlist, "webradio_startup"), "channels"));
      }
      if (json_object_get(json_object_get(j_playlist, "webradio_startup"), "samplerate") != NULL) {
        sample_rate = (int)json_integer_value(json_object_get(json_object_get(j_playlist, "webradio_startup"), "samplerate"));
      }
      if (0 == o_strcmp(format, "flac")) {
        bit_rate = TALIESIN_STREAM_FLAC_BIT_RATE;
      } else {
        if (json_object_get(json_object_get(j_playlist, "webradio_startup"), "bitrate") != NULL) {
          bit_rate = (int)json_integer_value(json_object_get(json_object_get(j_playlist, "webradio_startup"), "bitrate"));
        }
      }
      if (format == NULL) {
        format = TALIESIN_STREAM_DEFAULT_FORMAT;
      }
      if (!sample_rate) {
        sample_rate = TALIESIN_STREAM_DEFAULT_SAMPLE_RATE;
      }
      if (!channels) {
        channels = TALIESIN_STREAM_DEFAULT_CHANNELS;
      }
      if (!bit_rate) {
        bit_rate = TALIESIN_STREAM_DEFAULT_BIT_RATE;
      }
    }
  }
  
  j_query = json_pack("{sss{sosssssisssisisi}}",
                      "table",
                      TALIESIN_TABLE_PLAYLIST,
                      "values",
                        "tpl_username",
                        (0!=o_strcmp("all", json_string_value(json_object_get(j_playlist, "scope"))))?json_string(username):json_null(),
                        "tpl_name",
                        json_string_value(json_object_get(j_playlist, "name")),
                        "tpl_description",
                        json_object_get(j_playlist, "description")!=NULL?json_string_value(json_object_get(j_playlist, "description")):"",
                        "tpl_webradio_startup",
                        webradio_startup,
                        "tpl_webradio_startup_format",
                        format,
                        "tpl_webradio_startup_channels",
                        channels,
                        "tpl_webradio_startup_sample_rate",
                        sample_rate,
                        "tpl_webradio_startup_bit_rate",
                        bit_rate);
	if (tic_id) {
    json_object_set_new(json_object_get(j_query, "values"), "tic_id", json_integer(tic_id));
  }
  res = h_insert(config->conn, j_query, NULL);
  json_decref(j_query);
  if (res == H_OK) {
    j_last_id = h_last_insert_id(config->conn);
    tpl_id = json_integer_value(j_last_id);
    if (tpl_id) {
      if (db_playlist_replace_element_list(config, tpl_id, j_playlist) != T_OK) {
        y_log_message(Y_LOG_LEVEL_ERROR, "playlist_add - Error insert playlist elements");
        ret = T_ERROR;
      } else {
        ret = T_OK;
      }
    } else {
      y_log_message(Y_LOG_LEVEL_ERROR, "playlist_add - Error getting last id");
      ret = T_ERROR_DB;
    }
    json_decref(j_last_id);
  } else {
    y_log_message(Y_LOG_LEVEL_ERROR, "playlist_add - Error executing j_query (1)");
    ret = T_ERROR_DB;
  }
  return ret;
}

int playlist_can_update(json_t * j_playlist, int is_admin) {
  return (is_admin || o_strcmp(TALIESIN_SCOPE_ME, json_string_value(json_object_get(j_playlist, "scope"))) == 0);
}

int playlist_set(struct config_elements * config, json_int_t tpl_id, json_t * j_playlist) {
  json_t * j_query;
  json_int_t tic_id = 0;
  int res, ret;
  int webradio_startup = TALIESIN_STORED_PLAYLIST_WEBRADIO_STARTUP_NO_UPDATE;
  const char * format = NULL;
  unsigned short int channels = 0;
  unsigned int sample_rate = 0, bit_rate = 0;
  
  if (json_object_get(j_playlist, "webradio_startup") != NULL) {
    if (0 == o_strcmp("no random", json_string_value(json_object_get(json_object_get(j_playlist, "webradio_startup"), "status")))) {
      webradio_startup = TALIESIN_STORED_PLAYLIST_WEBRADIO_STARTUP_NO_RANDOM;
    } else if (0 == o_strcmp("random", json_string_value(json_object_get(json_object_get(j_playlist, "webradio_startup"), "status")))) {
      webradio_startup = TALIESIN_STORED_PLAYLIST_WEBRADIO_STARTUP_RANDOM;
    } else {
      webradio_startup = TALIESIN_STORED_PLAYLIST_WEBRADIO_STARTUP_OFF;
    }
    if (0 == o_strcmp(json_string_value(json_object_get(json_object_get(j_playlist, "webradio_startup"), "status")), "no random") ||
        0 == o_strcmp(json_string_value(json_object_get(json_object_get(j_playlist, "webradio_startup"), "status")), "random")) {
      if (json_object_get(json_object_get(j_playlist, "webradio_startup"), "format") != NULL) {
        format = json_string_value(json_object_get(json_object_get(j_playlist, "webradio_startup"), "format"));
      }
      if (json_object_get(json_object_get(j_playlist, "webradio_startup"), "channels") != NULL) {
        channels = (int)json_integer_value(json_object_get(json_object_get(j_playlist, "webradio_startup"), "channels"));
      }
      if (json_object_get(json_object_get(j_playlist, "webradio_startup"), "samplerate") != NULL) {
        sample_rate = (int)json_integer_value(json_object_get(json_object_get(j_playlist, "webradio_startup"), "samplerate"));
      }
      if (0 == o_strcmp(format, "flac")) {
        bit_rate = TALIESIN_STREAM_FLAC_BIT_RATE;
      } else {
        if (json_object_get(json_object_get(j_playlist, "webradio_startup"), "bitrate") != NULL) {
          bit_rate = (int)json_integer_value(json_object_get(json_object_get(j_playlist, "webradio_startup"), "bitrate"));
        }
      }
    }
  }
  
	if (json_object_get(j_playlist, "cover") != NULL) {
		tic_id = media_cover_save(config, 0, (const unsigned char *)json_string_value(json_object_get(j_playlist, "cover")));
	}
  j_query = json_pack("{sss{ss}s{sI}}",
                      "table",
                      TALIESIN_TABLE_PLAYLIST,
                      "set",
                        "tpl_description",
                        json_object_get(j_playlist, "description")!=NULL?json_string_value(json_object_get(j_playlist, "description")):"",
                      "where",
                        "tpl_id",
                        tpl_id);
	if (tic_id) {
    json_object_set_new(json_object_get(j_query, "set"), "tic_id", json_integer(tic_id));
  }
  if (webradio_startup != TALIESIN_STORED_PLAYLIST_WEBRADIO_STARTUP_NO_UPDATE) {
    json_object_set_new(json_object_get(j_query, "set"), "tpl_webradio_startup", json_integer(webradio_startup));
  }
  if (format != NULL) {
    json_object_set_new(json_object_get(j_query, "set"), "tpl_webradio_startup_format", json_string(format));
  }
  if (channels) {
    json_object_set_new(json_object_get(j_query, "set"), "tpl_webradio_startup_channels", json_integer(channels));
  }
  if (sample_rate) {
    json_object_set_new(json_object_get(j_query, "set"), "tpl_webradio_startup_sample_rate", json_integer(sample_rate));
  }
  if (bit_rate) {
    json_object_set_new(json_object_get(j_query, "set"), "tpl_webradio_startup_bit_rate", json_integer(bit_rate));
  }
  res = h_update(config->conn, j_query, NULL);
  json_decref(j_query);
  if (res == H_OK) {
    if (db_playlist_replace_element_list(config, tpl_id, j_playlist) != T_OK) {
      y_log_message(Y_LOG_LEVEL_ERROR, "playlist_set - Error replace playlist elements");
      ret = T_ERROR;
    } else {
      ret = T_OK;
    }
  } else {
    y_log_message(Y_LOG_LEVEL_ERROR, "playlist_set - Error executing j_query (1)");
    ret = T_ERROR_DB;
  }
  return ret;
}

int playlist_delete(struct config_elements * config, json_int_t tpl_id) {
  json_t * j_query;
  int res;
  
  j_query = json_pack("{sss{sI}}",
                      "table",
                      TALIESIN_TABLE_PLAYLIST,
                      "where",
                        "tpl_id",
                        tpl_id);
  res = h_delete(config->conn, j_query, NULL);
  json_decref(j_query);
  if (res == H_OK) {
    return T_OK;
  } else {
    y_log_message(Y_LOG_LEVEL_ERROR, "playlist_delete - Error executing j_query");
    return T_ERROR_DB;
  }
}

int playlist_add_media(struct config_elements * config, json_int_t tpl_id, json_t * media_list) {
  json_t * j_query, * j_element;
  int res, ret;
  size_t index;
  
  // Insert new elements
  j_query = json_pack("{sss[]}",
                      "table",
                      TALIESIN_TABLE_PLAYLIST_ELEMENT,
                      "values");
  if (j_query != NULL) {
    json_array_foreach(media_list, index, j_element) {
      json_array_append_new(json_object_get(j_query, "values"), json_pack("{sIsO}", "tpl_id", tpl_id, "tm_id", json_object_get(j_element, "tm_id")));
    }
    res = h_insert(config->conn, j_query, NULL);
    json_decref(j_query);
    if (res != H_OK) {
      y_log_message(Y_LOG_LEVEL_ERROR, "playlist_add_media - Error executing j_query");
      ret = T_ERROR_DB;
    } else {
      ret = T_OK;
    }
  } else {
    y_log_message(Y_LOG_LEVEL_ERROR, "playlist_add_media - Error allocating resources for j_query");
    ret = T_ERROR_MEMORY;
  }
  return ret;
}

int playlist_delete_media(struct config_elements * config, json_int_t tpl_id, json_t * media_list) {
  json_t * j_query, * j_element;
  char * tmp, * clause_tm_id = NULL, * clause_where;
  int res, ret;
  size_t index;
  
  json_array_foreach(media_list, index, j_element) {
    if (clause_tm_id == NULL) {
      clause_tm_id = msprintf("%" JSON_INTEGER_FORMAT, json_integer_value(json_object_get(j_element, "tm_id")));
    } else {
      tmp = msprintf("%s,%" JSON_INTEGER_FORMAT, clause_tm_id, json_integer_value(json_object_get(j_element, "tm_id")));
      o_free(clause_tm_id);
      clause_tm_id = tmp;
    }
  }
  
  if (clause_tm_id != NULL) {
    clause_where = msprintf("tpl_id=%" JSON_INTEGER_FORMAT " AND tm_id in (%s)", tpl_id, clause_tm_id);
    o_free(clause_tm_id);
    j_query = json_pack("{sss{s{ssss}}}",
                        "table",
                        TALIESIN_TABLE_PLAYLIST_ELEMENT,
                        "where",
                          " ",
                            "operator",
                            "raw",
                            "value",
                            clause_where);
    o_free(clause_where);
    res = h_delete(config->conn, j_query, NULL);
    json_decref(j_query);
    if (res == H_OK) {
      ret = T_OK;
    } else {
      y_log_message(Y_LOG_LEVEL_ERROR, "playlist_add_media - Error executing j_query");
      ret = T_ERROR_DB;
    }
  } else {
    y_log_message(Y_LOG_LEVEL_ERROR, "playlist_add_media - Error allocating resources for clause_tm_id");
    ret = T_ERROR_MEMORY;
  }
  return ret;
}

json_t * playlist_media_cover_get(struct config_elements * config, const char * username, const char * name, int thumbnail) {
  json_t * j_query, * j_result_playlist, * j_result_image, * j_return;
  int res;
	char * temp, * clause_username;
  
  temp = h_escape_string(config->conn, username);
  clause_username = msprintf("(`tpl_username` = '%s' OR `tpl_username` IS NULL)", temp);
  o_free(temp);
  j_query = json_pack("{sss[sssss]s{s{ssss}ss}}",
                      "table",
                      TALIESIN_TABLE_PLAYLIST,
                      "columns",
                        "tpl_id",
                        "tpl_name AS name",
                        "tpl_description AS description",
                        "tpl_username",
                        "tic_id",
                      "where",
                        " ",
                          "operator",
                          "raw",
                          "value",
                          clause_username,
                        "tpl_name",
                        name);
  o_free(clause_username);
  res = h_select(config->conn, j_query, &j_result_playlist, NULL);
  json_decref(j_query);
  if (res == H_OK) {
    if (json_array_size(j_result_playlist) > 0 && json_object_get(json_array_get(j_result_playlist, 0), "tic_id") != json_null()) {
			j_query = json_pack("{sss[]s{sI}}",
													"table",
													TALIESIN_TABLE_IMAGE_COVER,
													"columns",
													"where",
														"tic_id",
														json_integer_value(json_object_get(json_array_get(j_result_playlist, 0), "tic_id")));
			if (thumbnail) {
				json_array_append_new(json_object_get(j_query, "columns"), json_string("tic_cover_thumbnail AS cover"));
			} else {
				json_array_append_new(json_object_get(j_query, "columns"), json_string("tic_cover_original AS cover"));
			}
			res = h_select(config->conn, j_query, &j_result_image, NULL);
			json_decref(j_query);
			if (res == H_OK) {
				if (json_array_size(j_result_image) > 0) {
					j_return = json_pack("{siss}", "result", T_OK, "cover", json_string_value(json_object_get(json_array_get(j_result_image, 0), "cover")));
				} else {
					j_return = json_pack("{si}", "result", T_ERROR_NOT_FOUND);
				}
				json_decref(j_result_image);
			} else {
				y_log_message(Y_LOG_LEVEL_ERROR, "playlist_media_cover_get - Error executing j_query (image)");
				j_return = json_pack("{si}", "result", T_ERROR_DB);
			}
		} else {
			j_return = json_pack("{si}", "result", T_ERROR_NOT_FOUND);
    }
    json_decref(j_result_playlist);
  } else {
    y_log_message(Y_LOG_LEVEL_ERROR, "playlist_media_cover_get - Error executing j_query (category)");
    j_return = json_pack("{si}", "result", T_ERROR_DB);
  }
  return j_return;
}

json_t * playlist_list_webradio_startup(struct config_elements * config) {
  json_t * j_query, * j_result, * j_media, * j_element, * j_return;
  int res;
  size_t index;
	
  j_query = json_pack("{sss[sssssssss]s{s{sssi}}}",
                      "table",
                      TALIESIN_TABLE_PLAYLIST,
                      "columns",
                        "tpl_id",
                        "tpl_name AS name",
                        "tpl_description AS description",
                        "tpl_username AS username",
                        "tpl_webradio_startup",
                        "tpl_webradio_startup_format",
                        "tpl_webradio_startup_channels",
                        "tpl_webradio_startup_sample_rate",
                        "tpl_webradio_startup_bit_rate",
                      "where",
                        "tpl_webradio_startup",
                          "operator",
                          "!=",
                          "value",
                          TALIESIN_STORED_PLAYLIST_WEBRADIO_STARTUP_OFF);
  res = h_select(config->conn, j_query, &j_result, NULL);
  json_decref(j_query);
  if (res == H_OK) {
    json_array_foreach(j_result, index, j_element) {
      j_media = playlist_get_media_list(config, json_integer_value(json_object_get(j_element, "tpl_id")), 1, 0, 0);
      if (check_result_value(j_media, T_OK)) {
        json_object_set(j_element, "media", json_object_get(j_media, "media"));
      } else {
        y_log_message(Y_LOG_LEVEL_ERROR, "playlist_list - Error check_result_value");
      }
      json_decref(j_media);
    }
    j_return = json_pack("{siso}", "result", T_OK, "playlist", j_result);
  } else {
    y_log_message(Y_LOG_LEVEL_ERROR, "playlist_list_webradio_startup - Error executing j_query");
    j_return = json_pack("{si}", "result", T_ERROR_DB);
  }
  return j_return;
}

int playlist_load_as_webradio(struct config_elements * config, json_t * j_playlist) {
  struct _t_webradio * webradio;
  json_t * j_stream_info;
  int ret_thread_webradio = 0, detach_thread_webradio = 0, ret = T_OK;
  pthread_t thread_webradio;
  
  j_stream_info = add_webradio_from_playlist(config, j_playlist, json_string_value(json_object_get(j_playlist, "username")), json_string_value(json_object_get(j_playlist, "tpl_webradio_startup_format")), json_integer_value(json_object_get(j_playlist, "tpl_webradio_startup_channels")), json_integer_value(json_object_get(j_playlist, "tpl_webradio_startup_sample_rate")), json_integer_value(json_object_get(j_playlist, "tpl_webradio_startup_bit_rate")), (json_integer_value(json_object_get(j_playlist, "tpl_webradio_startup"))==TALIESIN_STORED_PLAYLIST_WEBRADIO_STARTUP_RANDOM), &webradio);
  if (check_result_value(j_stream_info, T_OK)) {
    ret_thread_webradio = pthread_create(&thread_webradio, NULL, webradio_run_thread, (void *)webradio);
    detach_thread_webradio = pthread_detach(thread_webradio);
    if (ret_thread_webradio || detach_thread_webradio) {
      y_log_message(Y_LOG_LEVEL_ERROR, "Error running thread webradio");
      ret = T_ERROR;
    } else {
    }
  } else {
    y_log_message(Y_LOG_LEVEL_ERROR, "playlist_load_as_webradio - Error streaming file");
    ret = T_ERROR;
  }
  json_decref(j_stream_info);
  return ret;
}
