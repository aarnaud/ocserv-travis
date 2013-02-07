#ifndef WORKER_H
#define WORKER_H

#include <config.h>
#include <syslog.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>
#include <net/if.h>
#include <vpn.h>
#include <cookies.h>
#include <tlslib.h>


struct tls_st {
	gnutls_certificate_credentials_t xcred;
	gnutls_priority_t cprio;
};


typedef enum {
	UP_DISABLED,
	UP_SETUP,
	UP_HANDSHAKE,
	UP_INACTIVE,
	UP_ACTIVE
} udp_port_state_t;

enum {
	HEADER_COOKIE = 1,
	HEADER_MASTER_SECRET,
	HEADER_HOSTNAME,
};

typedef struct worker_st {
	struct tls_st *creds;
	gnutls_session_t session;
	gnutls_session_t dtls_session;
	int cmd_fd;
	int conn_fd;
	
	http_parser *parser;
	struct cfg_st *config;

	struct sockaddr_storage remote_addr;	/* peer's address */
	socklen_t remote_addr_len;
	int proto; /* AF_INET or AF_INET6 */

	/* set after authentication */
	int udp_fd;
	udp_port_state_t udp_state;
	
	/* for mtu trials */
	unsigned last_good_mtu;
	unsigned last_bad_mtu;

	/* the following are set only if authentication is complete */
	char tun_name[IFNAMSIZ];
	char username[MAX_USERNAME_SIZE];
	char hostname[MAX_HOSTNAME_SIZE];
	uint8_t cookie[COOKIE_SIZE];
	uint8_t master_secret[TLS_MASTER_SIZE];
	uint8_t session_id[GNUTLS_MAX_SESSION_ID];
	unsigned auth_ok;
	int tun_fd;
} worker_st;


void vpn_server(struct worker_st* ws);

int auth_cookie(worker_st *ws, void* cookie, size_t cookie_size);

int get_auth_handler(worker_st *server);
int post_old_auth_handler(worker_st *server);
int post_new_auth_handler(worker_st *server);

void set_resume_db_funcs(gnutls_session_t);

struct req_data_st {
	char url[256];
	char hostname[MAX_HOSTNAME_SIZE];
	unsigned int next_header;
	unsigned char cookie[COOKIE_SIZE];
	unsigned int cookie_set;
	unsigned char master_secret[TLS_MASTER_SIZE];
	unsigned int master_secret_set;
	char *body;
	unsigned int headers_complete;
	unsigned int message_complete;
};

void __attribute__ ((format(printf, 3, 4)))
    oclog(const worker_st * server, int priority, const char *fmt, ...);

#endif