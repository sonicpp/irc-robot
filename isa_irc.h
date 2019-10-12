/*
 * ISA 2014/2015
 * IRC bot s logováním přes SYSLOG protokol
 *
 * Jan Havran <xhavra13@stud.fit.vutbr.cz>
 */

#ifndef ISA_IRC_H
#define ISA_IRC_H

#define IRC_BUF_SIZE	513		// IRC maximalni delka zpravy

struct IRCMessage {
	char reserved[IRC_BUF_SIZE];	// Rezervovane misto pro data
	char *prefix;			// IRC prefix zpravy
	char *command;			// IRC prikaz
	char *params[15];		// Pole parametru zpravy (podle IRC max 15)
	char *free_ptr;			// Ukazatel na zacatek volneho mista
	unsigned params_cnt;		// Pocet parametru
	unsigned type;
};

extern int connectToIRC(const char *host, const char *port);
extern void createIRCMsg(struct IRCMessage *msg, char *prefix, char *command, char *first_param, unsigned type);
extern void appendParam(struct IRCMessage *msg, char *param);
extern void createStringFromMsg(char *str, struct IRCMessage *msg);
extern void *IRCReader (void *sockfd);
extern void *IRCWriter (void *sockfd);
extern int getNameByPrefix(char *prefix, char *name, size_t len);

#endif	// ISA_IRC_H