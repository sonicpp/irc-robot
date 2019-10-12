/*
 * ISA 2014/2015
 * IRC bot s logováním přes SYSLOG protokol
 *
 * Jan Havran <xhavra13@stud.fit.vutbr.cz>
 */

#ifndef ISA_COMMON_H
#define ISA_COMMON_H

extern struct Queue qCheck;
extern struct QueueThread qIRC;
extern struct QueueThread qSyslog;

extern void getMyHostname(char *hostname, size_t len);

/* Typ udalosti */
enum event_type {
	TYPE_MSG = 1,	/* Normalni zprava */
	TYPE_QUIT	/* Prikaz k ukonceni */
};

#endif	// ISA_COMMON_H
