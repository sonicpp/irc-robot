/*
 * ISA 2014/2015
 * IRC bot s logováním přes SYSLOG protokol
 *
 * Jan Havran <xhavra13@stud.fit.vutbr.cz>
 */

#ifndef ISA_SYSLOG_H
#define ISA_SYSLOG_H

#define SYSLOG_BUF_SIZE		1025		// SYSLOG maximalni delka zpravy
#define SYSLOG_NICK_SIZE	64		// SYSLOG maximalni delka loginu

struct SLogMessage {
	char login[SYSLOG_NICK_SIZE];	// SysLog CONTENT (cast)
	char msg[450];			// SysLog CONTENT (cast)
	unsigned type;			// Typ zpravy
};

extern int connectToSyslog(const char *host, const char *port);
extern void createSysLogMsg(struct SLogMessage *msg, char *login, char *text, unsigned type);
extern void *SyslogWriter (void *sockfd);

#endif	// ISA_SYSLOG_H
