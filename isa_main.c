/*
 * ISA 2014/2015
 * IRC bot s logováním přes SYSLOG protokol
 *
 * Jan Havran <xhavra13@stud.fit.vutbr.cz>
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>

#include <sys/socket.h>
#include <netdb.h>
#include <signal.h>
#include <unistd.h>

#include <pthread.h>

#include "isa_common.h"
#include "isa_queue.h"
#include "isa_irc.h"
#include "isa_syslog.h"

#define LOGIN		"xhavra13"
#define IRC_PORT	"6667"
#define SYSLOG_PORT	"514"
#define PARAM_MIN_CNT	4
#define PARAM_MAX_CNT	5

/* IRC socket */
static int sockfdIRC;

/* Parametry programu */
enum {
	PARAM_IRC_SRV = 1,
	PARAM_IRC_CHAN,
	PARAM_LOG_SRV,
	PARAM_WORD_LIST
};

struct IRCSession {
	char *host;
	char *port;
	char *channel;
};

struct LogSession {
	char *host;
	char *port;
};

struct Queue qCheck;		/* Seznam-fronta sledovanych slov */
struct QueueThread qIRC;	/* Fronta odchozich IRC zprav */
struct QueueThread qSyslog;	/* Fronta odchozich SysLog zprav */

/*
 * Naplneni struktury IRCSession z parametru predanych programu
 */
struct IRCSession * getIRCSession(char *server, char *channel)
{
	struct IRCSession *irc = (void *) malloc (sizeof(struct IRCSession));
	char *str_delim;

	/* Byl zadan port serveru */
	if ((str_delim = strchr(server, ':')) != NULL) {
		unsigned str_delim_pos = str_delim - server;

		irc->host = (char *) malloc(str_delim_pos + 1);
		irc->port = (char *) malloc(strlen(server) - str_delim_pos);

		strncpy(irc->host, server, str_delim_pos);
		strcpy(irc->port, server + str_delim_pos + 1);
	}
	else {
		irc->host = (char *) malloc (strlen(server) + 1);
		irc->port = (char *) malloc (strlen(IRC_PORT) + 1);

		strcpy(irc->host, server);
		strcpy(irc->port, IRC_PORT);
	}

	irc->channel = (char *) malloc (strlen(channel) + 1);
	strcpy(irc->channel, channel);

	return irc;
}

/*
 * Naplneni struktury LogSession z parametru predanem programu
 */
struct LogSession *getLogSession(char *server)
{
	struct LogSession *log = (void *) malloc (sizeof (struct LogSession));

	log->host = (char *) malloc (strlen(server) + 1);
	log->port = (char *) malloc (strlen(SYSLOG_PORT) + 1);

	strcpy(log->host, server);
	strcpy(log->port, SYSLOG_PORT);

	return log;
}

/*
 * Naplneni fronty "q" hledanymi slovy z parametru programu
 * oddelovac slov je strednik ";"
 */
void getCheckList(char *list, struct Queue *q)
{
	int start = 0;
	int end = 0;
	char *word;

	/* Prohledani celeho retezce */
	while (list[end] != '\0') {
		if (list[end] == ';') {
			/* Slova nulove delky preskakujeme */
			if (start != end) {
				word = (char *) malloc(end - start + 1);
				memcpy(word, (list + start), end - start);
				word[end - start] = '\0';
				QUp(q, (void *) word);
				printf("Hledane slovo: %s\n", word);
			}
			start = end + 1;
		}
		end++;
	}

	/* Ulozeni posledniho slova */
	if (start != end) {
		word = (char *) malloc(end - start + 1);
		memcpy(word, (list + start), end - start);
		word[end - start] = '\0';
		QUp(q, (void *) word);
		printf("Hledane slovo: %s\n", word);
	}

	return;
}

/*
 * Tisk pouziti programu
 */
void usage()
{
	printf("pouziti: ircbot <host>[:<port>] <channel> <syslog_server> [list]\n"
		"host\t\tIRC server\n"
		"port\t\tcislo portu (default %s)\n"
		"channel\t\tIRC kanal\n"
		"syslog_server\tSYSLOG server\n"
		"list\t\tseznam sledovanych slov (oddelovac \';\')\n",
		IRC_PORT);
}

void getMyHostname(char *hostname, size_t len)
{
	if (gethostname(hostname, len) != 0)
		strncpy(hostname, "localhost", len);

	return;
}

/*
 * Odchyceni signalu
 * Program se odpoji od IRC serveru, informuje SysLog o ukonceni a pote se ukonci
 */
void int_handler(int arg)
{
	struct IRCMessage *irc_msg;
	struct SLogMessage *slog_msg;

	/* Odpojeni od IRC serveru */
	irc_msg = malloc(sizeof(struct IRCMessage));
	createIRCMsg(irc_msg, NULL, "QUIT", "Byl jsem zabit...", TYPE_MSG);
	QThreadUp(&qIRC, (void *) irc_msg);

	/* Informovani o zabiti bota */
	slog_msg = malloc (sizeof (struct SLogMessage));
	createSysLogMsg(slog_msg, "", "Proces byl ukoncen <SIGINT>", TYPE_MSG);
	QThreadUp(&qSyslog, (void *) slog_msg);

	/* Zavreni socketfd pro cteni */
	shutdown(sockfdIRC, SHUT_RD);

	return;
}

int main(int argc, char *argv[])
{
	int sockfdSyslog;		// SysLog "spojeni"
	pthread_t IRC_reader;		// IRC cteci vlakno
	pthread_t IRC_writer;		// IRC zapisovaci plakno
	pthread_t Syslog_writer;	// SysLog zapisovaci vlakno
	struct IRCMessage *msg;		// IRC zprava
	struct IRCSession *irc;		// Informace o IRC
	struct LogSession *log;		// Informace o SysLog
	void *stat;


	/* Kontrola parametru */
	if (argc < PARAM_MIN_CNT || argc > PARAM_MAX_CNT) {
		usage();
		return 1;
	}

	/* Inicializace front */
	QInit(&qCheck);
	QThreadInit(&qIRC);
	QThreadInit(&qSyslog);

	/* Ziskani IP adresy, portu, kanalu... z parametru */
	irc = getIRCSession(argv[PARAM_IRC_SRV], argv[PARAM_IRC_CHAN]);
	log = getLogSession(argv[PARAM_LOG_SRV]);

	/* Zpracovani seznamu hledanych slov, pokud zadano */
	if (argc == PARAM_MAX_CNT) {
		getCheckList(argv[PARAM_WORD_LIST], &qCheck);
	}

	/* Tisk strucnych informaci o spojeni */
	printf("%s : %s %s\n", irc->host, irc->port, irc->channel);
	printf("%s : %s\n", log->host, log->port);

	/* Pridani do fronty zprav NICK */
	msg = malloc(sizeof(struct IRCMessage));
	createIRCMsg(msg, NULL, "NICK", LOGIN, TYPE_MSG);
	QThreadUp(&qIRC, (void *) msg);

	/* Pridani do fronty zprav USER */
	msg = malloc(sizeof(struct IRCMessage));
	createIRCMsg(msg, NULL, "USER", LOGIN, TYPE_MSG);
	appendParam(msg, LOGIN);
	appendParam(msg, LOGIN);
	appendParam(msg, LOGIN);
	QThreadUp(&qIRC, (void *) msg);

	/* Pridani do fronty zprav JOIN */
	msg = malloc(sizeof(struct IRCMessage));
	createIRCMsg(msg, NULL, "JOIN", irc->channel, TYPE_MSG);
	QThreadUp(&qIRC, (void *) msg);

	/* Navazani spojeni */
	if (!(sockfdIRC = connectToIRC(irc->host, irc->port)))
		return 1;
	if (!(sockfdSyslog = connectToSyslog(log->host, log->port)))
		return 1;

	/* Cteci a zapisovaci vlakna pro IRC */
	pthread_create(&IRC_reader, NULL, IRCReader, (void *) &sockfdIRC);
	pthread_create(&IRC_writer, NULL, IRCWriter, (void *) &sockfdIRC);

	/* Zapisovaci vlakno pro SysLog */
	pthread_create(&Syslog_writer, NULL, SyslogWriter, (void *) &sockfdSyslog);

	/* Registrace odpojeni na SIGINT */
	signal(SIGINT, int_handler);

	/* Cekani na dokonceni vlaken */
	pthread_join(IRC_reader, &stat);
	pthread_join(IRC_writer, &stat);
	pthread_join(Syslog_writer, &stat);

	/* Uzavreni spojeni */
	close(sockfdIRC);
	close(sockfdSyslog);

	/* Zruseni front */
	QDestroy(&qCheck);
	QThreadDestroy(&qIRC);
	QThreadDestroy(&qSyslog);

	return 0;
}

