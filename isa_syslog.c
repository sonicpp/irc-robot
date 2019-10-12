/*
 * ISA 2014/2015
 * IRC bot s logováním přes SYSLOG protokol
 *
 * RFC 3164
 * http://tools.ietf.org/html/rfc3164
 *
 * Jan Havran <xhavra13@stud.fit.vutbr.cz>
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>

#include <time.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>

#include "isa_syslog.h"
#include "isa_queue.h"
#include "isa_common.h"

#define TIME_SIZE 16
#define SYSLOG_TAG	"ircbot"
#define SYSLOG_FACILITY 16	// local0
#define SYSLOG_SEVERITY 6	// Informational

/* Vytvoreni cisla PRI z facility a severity */
#define MAKE_PRI(fac, sev) ((fac) * 8 + (sev))

struct addrinfo *res;

int connectToSyslog(const char *host, const char *port)
{
	int status;
	int sockfd;
	struct addrinfo hints;
	//struct addrinfo *res;

	/* Nastaveni spojeni */
	memset(&hints, 0, sizeof hints);
	hints.ai_family = AF_INET;     // IPv4
	hints.ai_socktype = SOCK_DGRAM; // UDP

	/* Priprava spojeni */
	if ((status = getaddrinfo(host, port, &hints, &res)) != 0) {
		fprintf(stderr, "getaddrinfo() chyba: %s\n", gai_strerror(status));
		return 0;
	}

	/* Vytvoreni socketu */
	if ((sockfd = socket(res->ai_family, res->ai_socktype, res->ai_protocol)) == -1) {
		fprintf(stderr, "socket() chyba: %s\n", strerror(errno));
		freeaddrinfo(res);
		return 0;
	}

	freeaddrinfo(res);

	return sockfd;
}

/*
 * Vytvoreni SysLog zpravy
 */
void createSysLogMsg(struct SLogMessage *msg, char *login, char *text, unsigned type)
{
	strcpy(msg->login, login);
	strcpy(msg->msg, text);
	msg->type = type;
}

/*
 * Zapisovaci vlakno
 */
void *SyslogWriter (void *sockfd)
{
	int sockfdSyslog = *((int *) sockfd);
	struct SLogMessage *msg;
	int running = 1;
	char timestamp[TIME_SIZE];
	char m_buf[SYSLOG_BUF_SIZE];
	char hostname[64];
	time_t t;
	struct tm *p_tm;

	/* Zjistime hostname */
	getMyHostname(hostname, sizeof(hostname));

	while (running) {
		/* Vyjmuti zpravy z cela fronty */
		msg = (struct SLogMessage *) QThreadFront(&qSyslog);

		if (msg->type == TYPE_MSG) {
			/* Ziskani aktualniho casu */
			t = time(NULL);
			p_tm = localtime(&t);
			strftime(timestamp, sizeof timestamp, "%b %d %T", p_tm);

			/* Vytvoreni zpravy
			 *
			 * Format SYSLOG: <PRI> HEADER MSG
			 *	PRI: (FACILITY*8) + SEVERITY
			 *	HEADER:TIMESTAMP HOSTNAME
			 *	MSG: TAG CONTENT
			 *
			 * V nasem pripade ma CONTENT podobu: <NICKNAME>: MESSAGE
			 */
			sprintf(m_buf, "<%d>%s %s %s <%s>: %s",
				MAKE_PRI(SYSLOG_FACILITY, SYSLOG_SEVERITY),	/* PRI */
				timestamp, 					/* TIMESTAMP */
				hostname,					/* HOSTNAME */
				SYSLOG_TAG,					/* TAG */
				msg->login,					/* LOGIN */
				msg->msg					/* MESSAGE */
			);

			/* Pokus o odeslani zpravy */
			if (sendto(sockfdSyslog, m_buf, strlen(m_buf) + 1, 0, res->ai_addr, res->ai_addrlen) == 0)
				running = 0;

			printf("LOG >>> %s\n", m_buf);
		}
		else {
			running = 0;
		}

		free(msg);
	}
	return NULL;
}
