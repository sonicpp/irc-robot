/*
 * ISA 2014/2015
 * IRC bot s logováním přes SYSLOG protokol
 *
 * Jan Havran <xhavra13@stud.fit.vutbr.cz>
 */

#include <stdio.h>
#include <stdlib.h>

#include "isa_queue.h"

static int QThreadEmptyNoSafe (struct QueueThread *q);
static void *QThreadFrontNoSafe (struct QueueThread *q);

/*
 * Inicializace fronty
 */
void QInit (struct Queue *q)
{
	q->first = NULL;
}

/*
 * Smazani vsech prvku z fronty
 */
void QDestroy (struct Queue *q)
{
	while (!QEmpty(q))
		free(QFront(q));
}

/*
 * Otestovani na prazdnost fronty
 */
int QEmpty (struct Queue *q)
{
	return q->first == NULL;
}

/*
 * Vyjmuti prvku z cela fronty
 */
void *QFront (struct Queue *q)
{
	void *data;
	struct QueueElement *elem;

	if (QEmpty(q))
		return NULL;
	else if (q->first->p_next == NULL) {
		data = q->first->data;
		free(q->first);
		q->first = NULL;
	}
	else {
		elem = q->first;
		q->first = elem->p_next;
		data = elem->data;
		free(elem);
	}

	return data;
}

/*
 * Pridani prvku do fronty
 */
int QUp (struct Queue *q, void *data)
{
	struct QueueElement *elemNew;
	struct QueueElement *elemLast;

	elemNew = (void *) malloc (sizeof (struct QueueElement));
	if (elemNew == NULL) return 1;

	elemNew->p_next = NULL;
	elemNew->data = data;

	if (QEmpty(q))
		q->first = elemNew;
	else if (q->first->p_next == NULL)
		q->first->p_next = elemNew;
	else {
		elemLast = q->first;
		while (elemLast->p_next != NULL)
			elemLast = elemLast->p_next;

		elemLast->p_next = elemNew;
	}

	return 0;
}

/*
 * Inicializace fronty
 */
int QThreadInit (struct QueueThread *q)
{
	if (pthread_mutex_init(&(q->mutex), NULL) != 0) return 1;
	if (pthread_cond_init(&(q->contain), NULL) != 0) {
		pthread_mutex_destroy(&(q->mutex));
		return 1;
	}

	q->first = NULL;

	return 0;
}

/*
 * Smazani prvku z fronty
 */
void QThreadDestroy (struct QueueThread *q)
{
	pthread_mutex_lock(&(q->mutex));

	while (!QThreadEmptyNoSafe(q))
		free(QThreadFrontNoSafe(q));

	pthread_mutex_unlock(&(q->mutex));

	pthread_mutex_destroy(&(q->mutex));
	pthread_cond_destroy(&(q->contain));
}

/*
 * Otestovani na prazdnost fronty
 */
int QThreadEmpty (struct QueueThread *q)
{
	int ret;

	pthread_mutex_lock(&(q->mutex));
	ret = QThreadEmptyNoSafe(q);
	pthread_mutex_unlock(&(q->mutex));

	return ret;
}

/*
 * Otestovani na prazdnost fronty
 * Neni Thread safe
 */
static int QThreadEmptyNoSafe (struct QueueThread *q)
{
	return q->first == NULL;
}

/*
 * Vyjmuti prvku z cela fronty
 */
void *QThreadFront (struct QueueThread *q)
{
	void *data;

	pthread_mutex_lock(&(q->mutex));
	data = QThreadFrontNoSafe(q);
	pthread_mutex_unlock(&(q->mutex));

	return data;
}

/*
 * Vyjmuti prvku z cela fronty
 * Neni Thread safe
 */
static void *QThreadFrontNoSafe (struct QueueThread *q)
{
	void *data;
	struct QueueElement *elem;

	/* Pokud fronta neobsahuje dalsi zpravu, tak na ni pockame */
	if (QThreadEmptyNoSafe(q))
		pthread_cond_wait(&(q->contain), &(q->mutex));

	if (q->first->p_next == NULL) {
		data = q->first->data;
		free(q->first);
		q->first = NULL;
	}
	else {
		elem = q->first;
		q->first = elem->p_next;
		data = elem->data;
		free(elem);
	}

	return data;
}


/*
 * Pridani prvku do fronty
 */
int QThreadUp (struct QueueThread *q, void *data)
{
	struct QueueElement *elemNew;
	struct QueueElement *elemLast;

	elemNew = (void *) malloc (sizeof (struct QueueElement));
	if (elemNew == NULL) return 1;

	elemNew->p_next = NULL;
	elemNew->data = data;

	pthread_mutex_lock(&(q->mutex));
	if (QThreadEmptyNoSafe(q))
		q->first = elemNew;
	else if (q->first->p_next == NULL)
		q->first->p_next = elemNew;
	else {
		elemLast = q->first;
		while (elemLast->p_next != NULL)
			elemLast = elemLast->p_next;

		elemLast->p_next = elemNew;
	}

	/* Zaslani signalu o novem prvku ve fronte */
	pthread_cond_signal(&(q->contain));

	pthread_mutex_unlock(&(q->mutex));

	return 0;
}

