#include "uwsgi.h"

extern struct uwsgi_server uwsgi;

static int set_http_date(time_t t, char *header, int header_len, char *dst, int last) {

        static char *week[] = { "Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat" };
        static char *months[] = {
                "Jan", "Feb", "Mar", "Apr",
                "May", "Jun", "Jul", "Aug",
                "Sep", "Oct", "Nov", "Dec"
        };

        struct tm *hdtm = gmtime(&t);

        if (last) {
                return snprintf(dst, 36 + header_len, "%.*s: %s, %02d %s %4d %02d:%02d:%02d GMT\r\n\r\n", header_len, header, week[hdtm->tm_wday], hdtm->tm_mday, months[hdtm->tm_mon], hdtm->tm_year + 1900, hdtm->tm_hour, hdtm->tm_min, hdtm->tm_sec);
        }

        return snprintf(dst, 34 + header_len, "%.*s: %s, %02d %s %4d %02d:%02d:%02d GMT\r\n", header_len, header, week[hdtm->tm_wday], hdtm->tm_mday, months[hdtm->tm_mon], hdtm->tm_year + 1900, hdtm->tm_hour, hdtm->tm_min, hdtm->tm_sec);
}

// only RFC 1123 is supported
static time_t parse_http_date(char *date, uint16_t len) {

        struct tm hdtm;

        if (len != 29 && date[3] != ',')
                return 0;

        hdtm.tm_mday = uwsgi_str2_num(date + 5);

        switch (date[8]) {
        case 'J':
                if (date[9] == 'a') {
                        hdtm.tm_mon = 0;
                        break;
                }

                if (date[9] == 'u') {
                        if (date[10] == 'n') {
                                hdtm.tm_mon = 5;
                                break;
                        }

                        if (date[10] == 'l') {
                                hdtm.tm_mon = 6;
                                break;
                        }

                        return 0;
                }

                return 0;

        case 'F':
                hdtm.tm_mon = 1;
                break;

        case 'M':
                if (date[9] != 'a')
                        return 0;

                if (date[10] == 'r') {
                        hdtm.tm_mon = 2;
                        break;
                }

                if (date[10] == 'y') {
                        hdtm.tm_mon = 4;
                        break;
                }

                return 0;

        case 'A':
                if (date[10] == 'r') {
                        hdtm.tm_mon = 3;
                        break;
                }
                if (date[10] == 'g') {
                        hdtm.tm_mon = 7;
                        break;
                }
                return 0;

        case 'S':
                hdtm.tm_mon = 8;
                break;

        case 'O':
                hdtm.tm_mon = 9;
                break;

        case 'N':
                hdtm.tm_mon = 10;

        case 'D':
                hdtm.tm_mon = 11;
                break;
        default:
                return 0;
        }

        hdtm.tm_year = uwsgi_str4_num(date + 12) - 1900;

        hdtm.tm_hour = uwsgi_str2_num(date + 17);
        hdtm.tm_min = uwsgi_str2_num(date + 20);
        hdtm.tm_sec = uwsgi_str2_num(date + 23);

        return timegm(&hdtm);

}



void uwsgi_add_expires_type(struct wsgi_request *wsgi_req, char *mime_type, int mime_type_len, struct stat *st) {

	struct uwsgi_dyn_dict *udd = uwsgi.static_expires_type;
	time_t now = wsgi_req->start_of_request / 1000000;
	// Expires+34+1
	char expires[42];

	while (udd) {
		if (!uwsgi_strncmp(udd->key, udd->keylen, mime_type, mime_type_len)) {
			int delta = uwsgi_str_num(udd->value, udd->vallen);
			int size = set_http_date(now + delta, "Expires", 7, expires, 0);
			if (size > 0) {
				wsgi_req->headers_size += wsgi_req->socket->proto_write_header(wsgi_req, expires, size);
				wsgi_req->header_cnt++;
			}
			return;
		}
		udd = udd->next;
	}

	udd = uwsgi.static_expires_type_mtime;
	while (udd) {
		if (!uwsgi_strncmp(udd->key, udd->keylen, mime_type, mime_type_len)) {
			int delta = uwsgi_str_num(udd->value, udd->vallen);
			int size = set_http_date(st->st_mtime + delta, "Expires", 7, expires, 0);
			if (size > 0) {
				wsgi_req->headers_size += wsgi_req->socket->proto_write_header(wsgi_req, expires, size);
				wsgi_req->header_cnt++;
			}
			return;
		}
		udd = udd->next;
	}
}

#ifdef UWSGI_PCRE
void uwsgi_add_expires(struct wsgi_request *wsgi_req, char *filename, int filename_len, struct stat *st) {

	struct uwsgi_dyn_dict *udd = uwsgi.static_expires;
	time_t now = wsgi_req->start_of_request / 1000000;
	// Expires+34+1
	char expires[42];

	while (udd) {
		if (uwsgi_regexp_match(udd->pattern, udd->pattern_extra, filename, filename_len) >= 0) {
			int delta = uwsgi_str_num(udd->value, udd->vallen);
			int size = set_http_date(now + delta, "Expires", 7, expires, 0);
			if (size > 0) {
				wsgi_req->headers_size += wsgi_req->socket->proto_write_header(wsgi_req, expires, size);
				wsgi_req->header_cnt++;
			}
			return;
		}
		udd = udd->next;
	}

	udd = uwsgi.static_expires_mtime;
	while (udd) {
		if (uwsgi_regexp_match(udd->pattern, udd->pattern_extra, filename, filename_len) >= 0) {
			int delta = uwsgi_str_num(udd->value, udd->vallen);
			int size = set_http_date(st->st_mtime + delta, "Expires", 7, expires, 0);
			if (size > 0) {
				wsgi_req->headers_size += wsgi_req->socket->proto_write_header(wsgi_req, expires, size);
				wsgi_req->header_cnt++;
			}
			return;
		}
		udd = udd->next;
	}
}

void uwsgi_add_expires_path_info(struct wsgi_request *wsgi_req, struct stat *st) {

	struct uwsgi_dyn_dict *udd = uwsgi.static_expires_path_info;
	time_t now = wsgi_req->start_of_request / 1000000;
	// Expires+34+1
	char expires[42];

	while (udd) {
		if (uwsgi_regexp_match(udd->pattern, udd->pattern_extra, wsgi_req->path_info, wsgi_req->path_info_len) >= 0) {
			int delta = uwsgi_str_num(udd->value, udd->vallen);
			int size = set_http_date(now + delta, "Expires", 7, expires, 0);
			if (size > 0) {
				wsgi_req->headers_size += wsgi_req->socket->proto_write_header(wsgi_req, expires, size);
				wsgi_req->header_cnt++;
			}
			return;
		}
		udd = udd->next;
	}

	udd = uwsgi.static_expires_path_info_mtime;
	while (udd) {
		if (uwsgi_regexp_match(udd->pattern, udd->pattern_extra, wsgi_req->path_info, wsgi_req->path_info_len) >= 0) {
			int delta = uwsgi_str_num(udd->value, udd->vallen);
			int size = set_http_date(st->st_mtime + delta, "Expires", 7, expires, 0);
			if (size > 0) {
				wsgi_req->headers_size += wsgi_req->socket->proto_write_header(wsgi_req, expires, size);
				wsgi_req->header_cnt++;
			}
			return;
		}
		udd = udd->next;
	}
}

void uwsgi_add_expires_uri(struct wsgi_request *wsgi_req, struct stat *st) {

	struct uwsgi_dyn_dict *udd = uwsgi.static_expires_uri;
	time_t now = wsgi_req->start_of_request / 1000000;
	// Expires+34+1
	char expires[42];

	while (udd) {
		if (uwsgi_regexp_match(udd->pattern, udd->pattern_extra, wsgi_req->uri, wsgi_req->uri_len) >= 0) {
			int delta = uwsgi_str_num(udd->value, udd->vallen);
			int size = set_http_date(now + delta, "Expires", 7, expires, 0);
			if (size > 0) {
				wsgi_req->headers_size += wsgi_req->socket->proto_write_header(wsgi_req, expires, size);
				wsgi_req->header_cnt++;
			}
			return;
		}
		udd = udd->next;
	}

	udd = uwsgi.static_expires_uri_mtime;
	while (udd) {
		if (uwsgi_regexp_match(udd->pattern, udd->pattern_extra, wsgi_req->uri, wsgi_req->uri_len) >= 0) {
			int delta = uwsgi_str_num(udd->value, udd->vallen);
			int size = set_http_date(st->st_mtime + delta, "Expires", 7, expires, 0);
			if (size > 0) {
				wsgi_req->headers_size += wsgi_req->socket->proto_write_header(wsgi_req, expires, size);
				wsgi_req->header_cnt++;
			}
			return;
		}
		udd = udd->next;
	}
}



#endif


char *uwsgi_get_mime_type(char *name, int namelen, int *size) {

	int i;
	int count = 0;
	char *ext = NULL;
	for (i = namelen - 1; i >= 0; i--) {
		if (!isalnum((int) name[i])) {
			if (name[i] == '.') {
				ext = name + (namelen - count);
				break;
			}
		}
		count++;
	}

	if (!ext)
		return NULL;

	struct uwsgi_dyn_dict *udd = uwsgi.mimetypes;
	while (udd) {
		if (!uwsgi_strncmp(ext, count, udd->key, udd->keylen)) {
			udd->hits++;
			// auto optimization
			if (udd->prev) {
				if (udd->hits > udd->prev->hits) {
					struct uwsgi_dyn_dict *udd_parent = udd->prev->prev, *udd_prev = udd->prev;
					if (udd_parent) {
						udd_parent->next = udd;
					}

					if (udd->next) {
						udd->next->prev = udd_prev;
					}

					udd_prev->prev = udd;
					udd_prev->next = udd->next;

					udd->prev = udd_parent;
					udd->next = udd_prev;

					if (udd->prev == NULL) {
						uwsgi.mimetypes = udd;
					}
				}
			}
			*size = udd->vallen;
			return udd->value;
		}
		udd = udd->next;
	}

	return NULL;
}

int uwsgi_append_static_path(char *dir, char *file, int file_len) {

	size_t len = strlen(dir);

	if (len + 1 + file_len > PATH_MAX) {
		return -1;
	}

	if (dir[len - 1] == '/') {
		memcpy(dir + len, file, file_len);
		dir[len + file_len] = 0;
	}
	else {
		dir[len] = '/';
		memcpy(dir + len + 1, file, file_len);
		dir[len + 1 + file_len] = 0;
	}

	return len;
}

int uwsgi_static_stat(char *filename, struct stat *st) {

	int ret = stat(filename, st);
	// if non-existant return -1
	if (ret < 0)
		return -1;

	if (S_ISREG(st->st_mode))
		return 0;

	// check for index
	if (S_ISDIR(st->st_mode)) {
		struct uwsgi_string_list *usl = uwsgi.static_index;
		while (usl) {
			ret = uwsgi_append_static_path(filename, usl->value, usl->len);
			if (ret >= 0) {
#ifdef UWSGI_DEBUG
				uwsgi_log("checking for %s\n", filename);
#endif
				if (!uwsgi_static_stat(filename, st)) {
					return 0;
				}
				// reset to original name
				filename[ret] = 0;
			}
			usl = usl->next;
		}
	}

	return -1;
}

int uwsgi_real_file_serve(struct wsgi_request *wsgi_req, char *real_filename, size_t real_filename_len, struct stat *st) {

	struct iovec headers_vec[8];

	int mime_type_size = 0;
	char http_last_modified[49];

	char content_length[sizeof(UMAX64_STR) + 1];

#ifdef UWSGI_THREADING
	if (uwsgi.threads > 1)
		pthread_mutex_lock(&uwsgi.lock_static);
#endif
	char *mime_type = uwsgi_get_mime_type(real_filename, real_filename_len, &mime_type_size);
#ifdef UWSGI_THREADING
	if (uwsgi.threads > 1)
		pthread_mutex_unlock(&uwsgi.lock_static);
#endif

	if (wsgi_req->if_modified_since_len) {
		time_t ims = parse_http_date(wsgi_req->if_modified_since, wsgi_req->if_modified_since_len);
		if (st->st_mtime <= ims) {
			wsgi_req->status = 304;
			headers_vec[0].iov_base = wsgi_req->protocol;
			headers_vec[0].iov_len = wsgi_req->protocol_len;
			headers_vec[1].iov_base = " 304 Not Modified\r\n";
			headers_vec[1].iov_len = 19;

			wsgi_req->headers_size += wsgi_req->socket->proto_writev_header(wsgi_req, headers_vec, 2);

			struct uwsgi_string_list *ah = uwsgi.additional_headers;
			while (ah) {
				headers_vec[0].iov_base = ah->value;
				headers_vec[0].iov_len = ah->len;
				headers_vec[1].iov_base = "\r\n";
				headers_vec[1].iov_len = 2;
				wsgi_req->headers_size += wsgi_req->socket->proto_writev_header(wsgi_req, headers_vec, 2);
				wsgi_req->header_cnt++;
				ah = ah->next;
			}

			wsgi_req->headers_size += wsgi_req->socket->proto_write_header(wsgi_req, "\r\n", 2);
			return 0;
		}
	}
#ifdef UWSGI_DEBUG
	uwsgi_log("[uwsgi-fileserve] file %s found\n", real_filename);
#endif

	// HTTP status
	headers_vec[0].iov_base = wsgi_req->protocol;
	headers_vec[0].iov_len = wsgi_req->protocol_len;
	headers_vec[1].iov_base = " 200 OK\r\n";
	headers_vec[1].iov_len = 9;
	wsgi_req->headers_size = wsgi_req->socket->proto_writev_header(wsgi_req, headers_vec, 2);

	// uWSGI additional headers
	struct uwsgi_string_list *ah = uwsgi.additional_headers;
	while (ah) {
		headers_vec[0].iov_base = ah->value;
		headers_vec[0].iov_len = ah->len;
		headers_vec[1].iov_base = "\r\n";
		headers_vec[1].iov_len = 2;
		wsgi_req->headers_size += wsgi_req->socket->proto_writev_header(wsgi_req, headers_vec, 2);
		wsgi_req->header_cnt++;
		ah = ah->next;
	}

#ifdef UWSGI_PCRE
	uwsgi_add_expires(wsgi_req, real_filename, real_filename_len, st);
	uwsgi_add_expires_path_info(wsgi_req, st);
	uwsgi_add_expires_uri(wsgi_req, st);
#endif

	// Content-Type (if available)
	if (mime_type_size > 0 && mime_type) {
		headers_vec[0].iov_base = "Content-Type: ";
		headers_vec[0].iov_len = 14;
		headers_vec[1].iov_base = mime_type;
		headers_vec[1].iov_len = mime_type_size;
		headers_vec[2].iov_base = "\r\n";
		headers_vec[2].iov_len = 2;
		wsgi_req->headers_size += wsgi_req->socket->proto_writev_header(wsgi_req, headers_vec, 3);
		wsgi_req->header_cnt++;

		// check for content-type related headers
		uwsgi_add_expires_type(wsgi_req, mime_type, mime_type_size, st);
	}

	// increase static requests counter
	uwsgi.workers[uwsgi.mywid].cores[wsgi_req->async_id].static_requests++;

	// nginx
	if (uwsgi.file_serve_mode == 1) {
		headers_vec[0].iov_base = "X-Accel-Redirect: ";
		headers_vec[0].iov_len = 18;
		headers_vec[1].iov_base = real_filename;
		headers_vec[1].iov_len = real_filename_len;
		headers_vec[2].iov_base = "\r\n";
		headers_vec[2].iov_len = 2;
		wsgi_req->headers_size += wsgi_req->socket->proto_writev_header(wsgi_req, headers_vec, 3);
		// this is the final header (\r\n added)
		set_http_date(st->st_mtime, "Last-Modified", 13, http_last_modified, 1);
		wsgi_req->headers_size += wsgi_req->socket->proto_write_header(wsgi_req, http_last_modified, 48);
		wsgi_req->header_cnt += 2;
	}
	// apache
	else if (uwsgi.file_serve_mode == 2) {
		headers_vec[0].iov_base = "X-Sendfile: ";
		headers_vec[0].iov_len = 12;
		headers_vec[1].iov_base = real_filename;
		headers_vec[1].iov_len = real_filename_len;
		headers_vec[2].iov_base = "\r\n";
		headers_vec[2].iov_len = 2;
		wsgi_req->headers_size += wsgi_req->socket->proto_writev_header(wsgi_req, headers_vec, 3);
		// this is the final header (\r\n added)
		set_http_date(st->st_mtime, "Last-Modified", 13, http_last_modified, 1);
		wsgi_req->headers_size += wsgi_req->socket->proto_write_header(wsgi_req, http_last_modified, 48);
		wsgi_req->header_cnt += 2;
	}
	// raw
	else {
		// set Content-Length
		headers_vec[0].iov_base = "Content-Length: ";
		headers_vec[0].iov_len = 16;
		headers_vec[1].iov_len = uwsgi_long2str2n(st->st_size, content_length, sizeof(UMAX64_STR) + 1);
		headers_vec[1].iov_base = content_length;
		headers_vec[2].iov_base = "\r\n";
		headers_vec[2].iov_len = 2;
		// this is the final header (\r\n added)
		set_http_date(st->st_mtime, "Last-Modified", 13, http_last_modified, 1);
		headers_vec[3].iov_base = http_last_modified;
		headers_vec[3].iov_len = 48;
		wsgi_req->headers_size += wsgi_req->socket->proto_writev_header(wsgi_req, headers_vec, 4);
		wsgi_req->header_cnt += 2;

		// if it is a HEAD request just skip transfer
		if (!uwsgi_strncmp(wsgi_req->method, wsgi_req->method_len, "HEAD", 4)) {
			wsgi_req->status = 200;
			return 0;
		}

		// Ok, the file must be transferred from uWSGI
		if (wsgi_req->socket->can_offload) {
			if (!uwsgi_offload_request_sendfile_do(wsgi_req, real_filename, st->st_size)) {
				wsgi_req->status = -30;
				return 0;
			}
		}

		wsgi_req->sendfile_fd = open(real_filename, O_RDONLY);
		wsgi_req->response_size += uwsgi_sendfile(wsgi_req);
		// here we need to close the sendfile fd (no-GC involved)
		close(wsgi_req->sendfile_fd);
	}

	wsgi_req->status = 200;
	return 0;
}


int uwsgi_file_serve(struct wsgi_request *wsgi_req, char *document_root, uint16_t document_root_len, char *path_info, uint16_t path_info_len, int is_a_file) {

	struct stat st;
	char real_filename[PATH_MAX + 1];
	uint64_t real_filename_len = 0;
	char *filename = NULL;
	size_t filename_len = 0;


	if (!is_a_file) {
		filename = uwsgi_concat3n(document_root, document_root_len, "/", 1, path_info, path_info_len);
		filename_len = document_root_len + 1 + path_info_len;
	}
	else {
		filename = uwsgi_concat2n(document_root, document_root_len, "", 0);
		filename_len = document_root_len;
	}

#ifdef UWSGI_DEBUG
	uwsgi_log("[uwsgi-fileserve] checking for %s\n", filename);
#endif

	if (uwsgi.static_cache_paths) {
		uwsgi_rlock(uwsgi.cache_lock);
		char *item = uwsgi_cache_get(filename, filename_len, &real_filename_len);
		if (item && real_filename_len > 0 && real_filename_len <= PATH_MAX) {
			memcpy(real_filename, item, real_filename_len);
			uwsgi_rwunlock(uwsgi.cache_lock);
			goto found;
		}
		uwsgi_rwunlock(uwsgi.cache_lock);
	}

	if (!realpath(filename, real_filename)) {
#ifdef UWSGI_DEBUG
		uwsgi_log("[uwsgi-fileserve] unable to get realpath() of the static file\n");
#endif
		free(filename);
		return -1;
	}
	real_filename_len = strlen(real_filename);

	if (uwsgi.static_cache_paths) {
		uwsgi_wlock(uwsgi.cache_lock);
		uwsgi_cache_set(filename, filename_len, real_filename, real_filename_len, uwsgi.static_cache_paths, UWSGI_CACHE_FLAG_UPDATE);
		uwsgi_rwunlock(uwsgi.cache_lock);
	}

found:
	free(filename);

	if (uwsgi_starts_with(real_filename, real_filename_len, document_root, document_root_len)) {
		struct uwsgi_string_list *safe = uwsgi.static_safe;
		while(safe) {
			if (!uwsgi_starts_with(real_filename, real_filename_len, safe->value, safe->len)) {
				goto safe;
			}		
			safe = safe->next;
		}
		uwsgi_log("[uwsgi-fileserve] security error: %s is not under %.*s or a safe path\n", real_filename, document_root_len, document_root);
		return -1;
	}

safe:

	if (!uwsgi_static_stat(real_filename, &st)) {

		// check for skippable ext
		struct uwsgi_string_list *sse = uwsgi.static_skip_ext;
		while (sse) {
			if (real_filename_len >= sse->len) {
				if (!uwsgi_strncmp(real_filename + (real_filename_len - sse->len), sse->len, sse->value, sse->len)) {
#ifdef UWSGI_ROUTING
					if (uwsgi_apply_routes_fast(wsgi_req) == UWSGI_ROUTE_BREAK)
						return 0;
#endif
					return -1;
				}
			}
			sse = sse->next;
		}

		return uwsgi_real_file_serve(wsgi_req, real_filename, real_filename_len, &st);
	}

	return -1;

}