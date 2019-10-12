/*
 * ISA 2014/2015
 * IRC bot s logováním přes SYSLOG protokol
 *
 * Jan Havran <xhavra13@stud.fit.vutbr.cz>
 */

#ifndef ISA_QUEUE_H
#define ISA_QUEUE_H

#include <pthread.h>
#include "isa_common.h"

struct QueueElement {
	void *data;
	struct QueueElement *p_next;
};

struct Queue {
	struct QueueElement *first;	// Celo fronty
};

struct QueueThread {
	struct QueueElement *first;	// Celo fronty
	pthread_mutex_t	mutex;		// Mutex fronty
	pthread_cond_t contain;		// Podminka pro prazdnost fronty
};

/* Funkce fronty bez zamku */
extern void QInit (struct Queue *q);
extern void QDestroy (struct Queue *q);
extern int QEmpty (struct Queue *q);
extern void *QFront (struct Queue *q);
extern int QUp (struct Queue *q, void *data);

/* Funkce fronty obsahujici zamky pro praci s vice vlakny */
extern int QThreadInit (struct QueueThread *q);
extern void QThreadDestroy (struct QueueThread *q);
extern int QThreadEmpty (struct QueueThread *q);
extern void *QThreadFront (struct QueueThread *q);
extern int QThreadUp (struct QueueThread *q, void *data);

#endif	// ISA_QUEUE_H