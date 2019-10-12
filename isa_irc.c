/*
 * ISA 2014/2015
 * IRC bot s logováním přes SYSLOG protokol
 *
 * RFC 1459
 * http://tools.ietf.org/html/rfc1459
 *
 * Jan Havran <xhavra13@stud.fit.vutbr.cz>
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>

#include "isa_irc.h"
#include "isa_queue.h"
#include "isa_common.h"
#include "isa_syslog.h"

#include <unistd.h>

void parseIRCMsg(char *data, struct IRCMessage *msg);
void handleIRCMsg(struct IRCMessage *msg, struct Queue *qCheck, struct QueueThread *qIRC, struct QueueThread *q_Syslog);

/*
 * Pripojeni k IRC serveru
 */
int connectToIRC(const char *host, const char *port)
{
	int status;
	int sockfd;
	struct addrinfo hints;
	struct addrinfo *res;

	// Nastaveni spojeni
	memset(&hints, 0, sizeof hints);
	hints.ai_family = AF_INET;		// IPv4
	hints.ai_socktype = SOCK_STREAM;	// TCP
	hints.ai_flags = AI_PASSIVE;

	// Priprava spojeni
	if ((status = getaddrinfo(host, port, &hints, &res)) != 0) {
		fprintf(stderr, "getaddrinfo() chyba: %s\n", gai_strerror(status));
		return 0;
	}

	// Vytvoreni socketu
	if ((sockfd = socket(res->ai_family, res->ai_socktype, res->ai_protocol)) == -1) {
		fprintf(stderr, "socket() chyba: %s\n", strerror(errno));
		freeaddrinfo(res);
		return 0;
	}

	// Navazani spojeni
	if (connect(sockfd, res->ai_addr, res->ai_addrlen) == -1) {
		fprintf(stderr, "connect() chyba: %s\n", strerror(errno));
		freeaddrinfo(res);
		return 0;
	}

	freeaddrinfo(res);

	return sockfd;
}

/*
 * Vytvoreni IRC zpravy
 */
void createIRCMsg(struct IRCMessage *msg, char *prefix, char *command, char *first_param, unsigned type)
{
	msg->free_ptr = msg->reserved;
	msg->type = type;

	if (prefix == NULL)
		msg->prefix = NULL;
	else {
		msg->prefix = msg->reserved;
		strcpy(msg->prefix, prefix);
		msg->free_ptr += strlen(prefix) + 1;
	}

	if (command == NULL)
		msg->command = NULL;
	else {
		msg->command = msg->free_ptr;
		strcpy(msg->command, command);
		msg->free_ptr += strlen(command) + 1;
	}

	if (first_param == NULL) {
		msg->params[0] = msg->free_ptr;
		msg->params_cnt = 0;
	}
	else {
		msg->params[0] = msg->free_ptr;
		strcpy(msg->params[0], first_param);
		msg->free_ptr += strlen(first_param) + 1;
		msg->params_cnt = 1;
	}
}

/*
 * Pridani parametru do IRC zpravy
 */
void appendParam(struct IRCMessage *msg, char *param)
{
	msg->params[msg->params_cnt] = msg->free_ptr;
	strcpy(msg->params[msg->params_cnt], param);
	msg->free_ptr += strlen(param) + 1;
	msg->params_cnt++;

}

/*
 * Vytvoreni retezce k odeslani ze struktury IRC zpravy
 */
void createStringFromMsg(char *str, struct IRCMessage *msg)
{
	char *ptr = str;	/* Ukazatel na konec retezce */
	int par = 0;

	/* Zpracovani prefixu */
	if (msg->prefix != NULL) {
		sprintf(ptr, ":%s ", msg->prefix);
		ptr += strlen(msg->prefix) + 2;
	}

	/* Zpracovani prikazu */
	sprintf(ptr, "%s ", msg->command);
	ptr += strlen(msg->command) + 1;

	/* Zpracovani parametru krome posledniho */
	while (par < msg->params_cnt - 1) {
		sprintf(ptr, "%s ", msg->params[par]);
		ptr += strlen(msg->params[par]) + 1;
		par++;
	}

	/* Zpracovani posledniho parametru - prefix ":" */
	sprintf(ptr, "%s%s\r\n", (par == msg->params_cnt) ? "" : ":", (par == msg->params_cnt) ? "" : msg->params[par]);
}

/*
 * Cteci vlakno IRC
 */
void *IRCReader (void *sockfd)
{
	int sockfdIRC = *((int *) sockfd);
	char packet[IRC_BUF_SIZE];	// Prijaty packet
	char msg_buf[IRC_BUF_SIZE];	// Buffer prijate zpravy
	int msg_ptr = 0;		// Pocet zpracovanych znaku zpravy
	int CR = 0;			// Precteny Carriage Return
	int msg_size;			// Delka prijateho packetu
	struct IRCMessage msg;
	struct IRCMessage *irc_msg;	// Ukoncujici IRC zprava
	struct SLogMessage *slog_msg;	// Ukoncujici SysLog zprava

	/* Prijimame zpravy, dokud to jde */
	while ((msg_size = recv(sockfdIRC, packet, IRC_BUF_SIZE - 1, 0)) > 0) {
		packet[msg_size] = '\0';

		/* Prochazeni znak po znaku prijateho packetu */
		int i = 0;
		for (i = 0; i < msg_size; i++) {
			/* Prekopirovani znaku do bufferu */
			msg_buf[msg_ptr++] = packet[i];

			/* Posledni precteny znak je Carriage Return */
			if (packet[i] == '\r')
				CR = 1;
			else {
				/* Mame konec zpravy (CR nasledovano LF) */
				if (packet[i] == '\n' && CR == 1) {
					msg_buf[msg_ptr] = '\0';
					printf("IRC <<< %s", msg_buf);

					parseIRCMsg(msg_buf, &msg);
					handleIRCMsg(&msg, &qCheck, &qIRC, &qSyslog);
					msg_ptr = 0;
				}
				CR = 0;
			}
		}
	}

	/* Ukonceni IRC spojeni */
	irc_msg = malloc(sizeof(struct IRCMessage));
	createIRCMsg(irc_msg, NULL, "", "", TYPE_QUIT);
	QThreadUp(&qIRC, (void *) irc_msg);

	/* Informujeme o ukonceni spojeni */
	slog_msg = malloc (sizeof (struct SLogMessage));
	createSysLogMsg(slog_msg, "", "Spojeni ukonceno", TYPE_MSG);
	QThreadUp(&qSyslog, (void *) slog_msg);

	/* Ukonceni SysLog spojeni */
	slog_msg = malloc (sizeof (struct SLogMessage));
	createSysLogMsg(slog_msg, "", "", TYPE_QUIT);
	QThreadUp(&qSyslog, (void *) slog_msg);

	return NULL;
}

/*
 * Odepisujici vlakno IRC
 */
void *IRCWriter (void *sockfd)
{
	int sockfdIRC = *((int *) sockfd);
	char buf[IRC_BUF_SIZE];
	struct IRCMessage *msg;

	int running = 1;

	while (running) {
		/* Vyjmeme zpravu z fronty */
		msg = (struct IRCMessage *) QThreadFront(&qIRC);

		if (msg->type == TYPE_MSG) {
			/* Zpracujeme zpravu a odesleme */
			createStringFromMsg(buf, msg);
			send(sockfdIRC, buf, strlen(buf), 0);
			printf("IRC >>> %s", buf);
		}
		/* Byla prijata zprava narizujici ukonceni */
		else {
			running = 0;
		}

		free(msg);
	}

	return NULL;
}

/*
 * Rozrezani prichozi zpravy
 */
void parseIRCMsg(char *data, struct IRCMessage *msg)
{
	msg->prefix = NULL;
	msg->free_ptr = msg->reserved;	// Nastaveni ukazatele volneho
					// mista na zacatek
	msg->params_cnt = 0;		// Nastaveni poctu parametru
	int start = 0;
	int end = start;

	/* Prefix (pokud existuje) */
	if (data[end] == ':') {
		start++;
		while (data[++end] != ' ') ;
		msg->prefix = msg->free_ptr;
		memcpy(msg->prefix, data + start, end - start);
		msg->prefix[end - start] = '\0';
		msg->free_ptr += end - start + 1;
		start = end;
	}

	while (data[start] == ' ')
		start++;

	/* Command */
	while (data[++end] != ' ') ;
	msg->command = msg->free_ptr;
	memcpy(msg->command, data + start, end - start);
	msg->command[end - start] = '\0';
	msg->free_ptr += end - start + 1;
	start = end;

	/* Params */
	int done = 0;
	while (!done) {
		while (data[start] == ' ')
			start++;

		end = start;
		if (data[start] == ':') {
			end = ++start;
			while (!(data[end] == '\r' && data[end + 1] == '\n'))
				end++;
			done = 1;
		}
		else {
			while (!(data[end] == ' ' || (data[end] == '\r' && data[end + 1] == '\n')))
				end++;
			if (data[end] != ' ')
				done = 1;

		}

		msg->params[msg->params_cnt] = msg->free_ptr;
		memcpy(msg->params[msg->params_cnt], data + start, end - start);
		msg->params[msg->params_cnt][end - start] = '\0';
		msg->free_ptr += end - start + 1;
		msg->params_cnt++;

		start = end;

	}

	return;
}

/*
 * Zjisteni jmena z prefixu IRC zpravy
 */
int getNameByPrefix(char *prefix, char *name, size_t len)
{
	char *end;
	int msg_size;

	if (len == 0 || prefix == NULL)
		return -1;

	if ((end = strchr(prefix, '!')) != NULL) {
		msg_size = (len - 1 < end - prefix) ? (len - 1) : (end - prefix);
		strncpy(name, prefix, msg_size);
	}
	else if ((end = strchr(prefix, '@')) != NULL) {
		msg_size = (len - 1 < end - prefix) ? (len - 1) : (end - prefix);
		strncpy(name, prefix, msg_size);
	}
	else {
		msg_size = len - 1;
		strncpy(name, prefix, msg_size);
	}

	name[msg_size] = '\0';

	return 0;
}

/*
 * Zpracovani prijate zpravy
 */
void handleIRCMsg(struct IRCMessage *msg, struct Queue *q_check, struct QueueThread *q_IRC, struct QueueThread *q_Syslog)
{
	struct IRCMessage *msg_out;
	struct SLogMessage *slog_msg;
	char nickname[SYSLOG_NICK_SIZE];

	/* Hledani retezcu */
	if ((strcmp("NOTICE", msg->command) == 0) || (strcmp("PRIVMSG", msg->command) == 0)) {
		if (msg->params_cnt > 1) {
			struct QueueElement *elem = (QEmpty(q_check)) ? NULL : q_check->first;
			unsigned text_param = msg->params_cnt - 1;
			short found = 0;
			char *text = msg->params[text_param];

			/* Zkusime vsechny hledane retezce, dokud nejaky nenajdeme */
			while (elem != NULL && !found) {
				if (strstr(text, (char *) elem->data) != NULL) {
					/* Nalezeno, zalogovani na SysLog */
					slog_msg = malloc (sizeof (struct SLogMessage));
					getNameByPrefix(msg->prefix, nickname, SYSLOG_NICK_SIZE);
					createSysLogMsg(slog_msg, nickname, msg->params[text_param], TYPE_MSG);
					QThreadUp(q_Syslog, (void *) slog_msg);
					found = 1;
				}
				elem = elem->p_next;
			}
		}
	}

	/* Prichozi PING  <server1> */
	else if (strcmp("PING", msg->command) == 0) {
		msg_out = malloc (sizeof (struct IRCMessage));
		createIRCMsg(msg_out, NULL, "PONG", msg->params[0], TYPE_MSG);

		QThreadUp(q_IRC, (void *) msg_out);
	}
	/* Zbytek ignorujeme */

	return;
}
