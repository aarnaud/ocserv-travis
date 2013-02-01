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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <pwd.h>
#include <grp.h>
#include <fcntl.h>
#include <ocserv-args.h>
#include <autoopts/options.h>

#include <vpn.h>

#define MAX_ENTRIES 64

#define READ_MULTI_LINE(name, s_name, num, mand) \
	val = optionGetValue(pov, name); \
	if (val != NULL && val->valType == OPARG_TYPE_STRING) { \
		if (s_name == NULL) { \
			num = 0; \
			s_name = malloc(sizeof(char*)*MAX_ENTRIES); \
			do { \
			        if (val && !strcmp(val->pzName, name)==0) \
					continue; \
			        s_name[num] = strdup(val->v.strVal); \
			        num++; \
			        if (num>=MAX_ENTRIES) \
			        break; \
		      } while((val = optionNextValue(pov, val)) != NULL); \
		      s_name[num] = NULL; \
		} \
	} else if (mand != 0) { \
		fprintf(stderr, "Configuration option %s is mandatory.\n", name); \
	}

#define READ_BOOLEAN(name, s_name, mand) \
	val = optionGetValue(pov, name); \
	if (val != NULL) { \
		s_name = 1; \
	} else if (mand != 0) { \
		fprintf(stderr, "Configuration option %s is mandatory.\n", name); \
	}


#define READ_STRING(name, s_name, mand) \
	val = optionGetValue(pov, name); \
	if (val != NULL && val->valType == OPARG_TYPE_STRING) \
		s_name = strdup(val->v.strVal); \
	else if (mand != 0) { \
		fprintf(stderr, "Configuration option %s is mandatory.\n", name); \
	}

#define READ_NUMERIC(name, s_name, mand) \
	val = optionGetValue(pov, name); \
	if (val != NULL) { \
		if (val->valType == OPARG_TYPE_NUMERIC) \
			s_name = val->v.longVal; \
		else if (val->valType == OPARG_TYPE_STRING) \
			s_name = atoi(val->v.strVal); \
	} else if (mand != 0) { \
		fprintf(stderr, "Configuration option %s is mandatory.\n", name); \
	}

static void parse_cfg_file(const char* file, struct cfg_st *config)
{
tOptionValue const * pov;
const tOptionValue* val;
char** auth = NULL;
unsigned auth_size = 0;
unsigned j;

	pov = configFileLoad(file);
	if (pov == NULL) {
		fprintf(stderr, "Error loading config file %s\n", file);
		exit(1);
	}

	READ_MULTI_LINE("auth", auth, auth_size, 1);
	for (j=0;j<auth_size;j++) {
		if (strcasecmp(auth[j], "pam") == 0) {
			config->auth_types |= AUTH_TYPE_PAM;
		} else if (strcasecmp(auth[j], "certificate") == 0) {
			config->auth_types |= AUTH_TYPE_CERTIFICATE;
			config->cert_req = GNUTLS_CERT_REQUIRE;
		} else {
			fprintf(stderr, "Unknown auth method: %s\n", auth[j]);
			exit(1);
		}
		free(auth[j]);
	}
	free(auth);
	
	READ_STRING("listen-host", config->name, 0);

	READ_NUMERIC("tcp-port", config->port, 1);
	READ_NUMERIC("keepalive", config->keepalive, 0);

	READ_STRING("server-cert", config->cert, 1);
	READ_STRING("server-key", config->key, 1);

	READ_STRING("ca-cert", config->ca, 0);
	READ_STRING("crl", config->crl, 0);
	READ_STRING("cert-user-oid", config->cert_user_oid, 0);

	READ_STRING("tls-priorities", config->priorities, 0);
	READ_STRING("chroot-dir", config->chroot_dir, 0);

	READ_NUMERIC("cookie-validity", config->cookie_validity, 1);
	READ_NUMERIC("auth-timeout", config->auth_timeout, 0);
	READ_STRING("cookie-db", config->cookie_db, 1);
	READ_NUMERIC("max-clients", config->max_clients, 0);

	val = optionGetValue(pov, "run-as-user"); \
	if (val != NULL && val->valType == OPARG_TYPE_STRING) {
		const struct passwd* pwd = getpwnam(val->v.strVal);
		if (pwd == NULL) {
			fprintf(stderr, "Unknown user: %s\n", val->v.strVal);
			exit(1);
		}
		config->uid = pwd->pw_uid;
	}

	val = optionGetValue(pov, "run-as-group"); \
	if (val != NULL && val->valType == OPARG_TYPE_STRING) {
		const struct group *grp = getgrnam(val->v.strVal);
		if (grp == NULL) {
			fprintf(stderr, "Unknown group: %s\n", val->v.strVal);
			exit(1);
		}
		config->gid = grp->gr_gid;
	}

	READ_STRING("device", config->network.name, 1);

	READ_STRING("ipv4-network", config->network.ipv4, 0);
	READ_STRING("ipv4-netmask", config->network.ipv4_netmask, 0);
	READ_STRING("ipv4-dns", config->network.ipv4_dns, 0);

	READ_STRING("ipv6-network", config->network.ipv6, 0);
	READ_STRING("ipv6-netmask", config->network.ipv6_netmask, 0);
	READ_STRING("ipv6-dns", config->network.ipv6_dns, 0);

	READ_MULTI_LINE("route", config->network.routes, config->network.routes_size, 0);
}

/* sanity checks on config */
static void check_cfg( struct cfg_st *config)
{
	if (config->network.ipv4 == NULL && config->network.ipv6 == NULL) {
		fprintf(stderr, "No ipv4-network or ipv6-network options set.\n");
		exit(1);
	}

	if (config->network.ipv4 != NULL && config->network.ipv4_netmask == NULL) {
		fprintf(stderr, "No mask found for IPv4 network.\n");
		exit(1);
	}

	if (config->network.ipv6 != NULL && config->network.ipv6_netmask == NULL) {
		fprintf(stderr, "No mask found for IPv6 network.\n");
		exit(1);
	}
	
	if (config->keepalive == 0)
		config->keepalive = 30;
	
	if (config->priorities == NULL)
		config->priorities = "NORMAL:%SERVER_PRECEDENCE:%COMPAT";
}

int cmd_parser (int argc, char **argv, struct cfg_st* config)
{
	const char* cfg_file;

	memset(config, 0, sizeof(*config));

	optionProcess( &ocservOptions, argc, argv);
  
	if (HAVE_OPT(FOREGROUND))
		config->foreground = 1;
	
	if (HAVE_OPT(CONFIG)) {
		cfg_file = OPT_ARG(CONFIG);
	} else {
		fprintf(stderr, "%s -c [config]\nUse %s --help for more information.\n", argv[0], argv[0]);
		exit(1);
	}
	
	parse_cfg_file(cfg_file, config);
	
	check_cfg(config);
	
	return 0;

}