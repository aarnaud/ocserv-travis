/*
 * Copyright (C) 2013 Nikos Mavrogiannopoulos
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include <config.h>

#include <gnutls/gnutls.h>
#include <errno.h>
#include <stdlib.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <limits.h>

#include <tlslib.h>
#include <vpn.h>


ssize_t tls_send(gnutls_session_t session, const void *data,
			size_t data_size)
{
	int ret;

	do {
		ret = gnutls_record_send(session, data, data_size);
	} while (ret < 0 && gnutls_error_is_fatal(ret) == 0);
	
	return ret;
}

ssize_t tls_send_file(gnutls_session_t session, const char *file)
{
FILE* fp;
char buf[512];
ssize_t len, total = 0;
int ret;

	fp = fopen(file, "r");
	
	while (	(len = fread( buf, 1, sizeof(buf), fp)) > 0) {
		ret = tls_send(session, buf, len);
		GNUTLS_FATAL_ERR(ret);
		
		total += ret;
	}
	
	fclose(fp);
	
	return total;
}

ssize_t tls_recv(gnutls_session_t session, void *data, size_t data_size)
{
	int ret;

	do {
		ret = gnutls_record_recv(session, data, data_size);
	} while (ret < 0 && gnutls_error_is_fatal(ret) == 0);
	
	return ret;
}

int __attribute__ ((format(printf, 2, 3)))
    tls_printf(gnutls_session_t session, const char *fmt, ...)
{
	char buf[1024];
	va_list args;

	buf[1023] = 0;

	va_start(args, fmt);
	vsnprintf(buf, 1023, fmt, args);
	va_end(args);
	return tls_send(session, buf, strlen(buf));

}

void tls_close(gnutls_session_t session)
{
	gnutls_bye(session, GNUTLS_SHUT_WR);
	gnutls_deinit(session);
}

void tls_fatal_close(gnutls_session_t session,
			    gnutls_alert_description_t a)
{
	gnutls_alert_send(session, GNUTLS_AL_FATAL, a);
	gnutls_deinit(session);
}