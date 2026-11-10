/*
**++
**  FACILITY:  utility_routines - regression tests
**
**  ABSTRACT:  The queue routines of utility_routines.h (insq*, remq*, movq*, clrqueue).
**	Pinned: the empty-queue rule (*ent == NULL, *count == 0), remqent of a stranger is an
**	error, the distinct status codes, the moves of the first, last and a middle entry, and a
**	random walk over every routine checked against a plain array model.
**
**  AUTHOR:  StarLet Squad and Ruslan R. Laishev (AKA: BadAss SysMan)
**
**  CREATION DATE:  31-OCT-2026
**
**  MODULE:  test_queue.c
**--
*/
#define __MODULE__ "QUET$"

#include <stdlib.h>
#include <string.h>
#include "test_harness.h"
#include "utility_routines.h"

typedef struct	{
	ENTRY	e;			/* must be the first member */
	int	id;
} ITEM;

#define	NITEM	12

static ITEM	s_it [NITEM];

static void s_reset (void)
{
	memset(s_it, 0, sizeof(s_it));

	for ( int i = 0; i < NITEM; i++ )
		s_it[i].id = i;
}

/* The queue holds exactly the items of a_ids[0..a_n-1], in this order, both chains consistent */
static int s_check (const __QUEUE *a_q, const int *a_ids, int a_n)
{
const ENTRY	*l_e, *l_prev = NULL;
int	l_k = 0;

	if ( a_q->count != (unsigned) a_n )
		return	0;

	for ( l_e = a_q->head; l_e; l_prev = l_e, l_e = l_e->right, l_k++ )
		{
		if ( (l_k >= a_n) || (l_e != &s_it[a_ids[l_k]].e) || (l_e->left != l_prev) || (l_e->queue != (void *) a_q) )
			return	0;
		}

	return	(l_k == a_n) && (a_q->tail == l_prev) && (a_n || !a_q->head);
}

static void s_load (__QUEUE *a_q, int a_n)
{
unsigned	l_cnt;

	s_reset();
	memset(a_q, 0, sizeof(*a_q));

	for ( int i = 0; i < a_n; i++ )
		$INSQTAIL(a_q, &s_it[i], &l_cnt);
}

int main (void)
{
__QUEUE	l_q = QUEUE_INITIALIZER, l_q2 = QUEUE_INITIALIZER;
void	*l_ent;
unsigned l_cnt;
int	l_sts, l_ids [NITEM];

	printf("== queue regression ==\n");

	/* ---- Group 1: the empty queue */
	s_reset();
	l_cnt = 77;
	CHECK( 1 & $CLRQUE(&l_q, &l_cnt), "clrqueue of an empty queue succeeds");
	CHECK( 0 == l_cnt, "...and sets *count to 0 (it was left as it was), got %u", l_cnt);

	l_ent = (void *) &l_ent;
	l_cnt = 77;
	l_sts = $REMQHEAD(&l_q, &l_ent, &l_cnt);
	CHECK( 1 & l_sts, "remqhead on an empty queue is a success: %d", l_sts);
	CHECK( NULL == l_ent, "...with *ent set to NULL (it was left unset)");
	CHECK( 0 == l_cnt, "...and *count 0");

	l_ent = (void *) &l_ent;
	l_cnt = 77;
	l_sts = $REMQTAIL(&l_q, &l_ent, &l_cnt);
	CHECK( 1 & l_sts, "remqtail on an empty queue is a success: %d", l_sts);
	CHECK( NULL == l_ent, "...with *ent set to NULL");
	CHECK( 0 == l_cnt, "...and *count 0");

	l_cnt = 77;
	CHECK( !(1 & $REMQENT(&l_q, &s_it[0], &l_cnt)), "remqent of an entry of an empty queue is an error");
	CHECK( 0 == l_cnt, "...*count is 0");
	CHECK( !(1 & $MOVQHEAD(&l_q, &s_it[0])), "movqhead on an empty queue is an error");
	CHECK( !(1 & $MOVQTAIL(&l_q, &s_it[0])), "movqtail on an empty queue is an error");
	CHECK( s_check(&l_q, l_ids, 0), "the empty queue is still empty and consistent");

	/* ---- Group 2: status codes */
	CHECK( UTIL$S_INQUE != UTIL$S_NOLOCK, "UTIL$S_INQUE and UTIL$S_NOLOCK differ");
	CHECK( UTIL$S_INVARG != UTIL$S_INQUE && UTIL$S_INVARG != UTIL$S_NOLOCK, "...and differ from UTIL$S_INVARG");
	CHECK( !(1 & UTIL$S_INVARG) && !(1 & UTIL$S_INQUE) && !(1 & UTIL$S_NOLOCK), "...all of them have the low bit clear");

	s_reset();
	CHECK( UTIL$S_INVARG == $INSQTAIL(NULL, &s_it[0], &l_cnt), "insqtail: NULL queue is INVARG");
	CHECK( UTIL$S_INVARG == $INSQHEAD(NULL, &s_it[0], &l_cnt), "insqhead: NULL queue is INVARG");
	CHECK( UTIL$S_INVARG == $INSQTAIL(&l_q, NULL, &l_cnt), "insqtail: NULL entry is INVARG");
	CHECK( UTIL$S_INVARG == $INSQHEAD(&l_q, NULL, &l_cnt), "insqhead: NULL entry is INVARG");
	CHECK( UTIL$S_INVARG == $INSQTAIL(&l_q, &s_it[0], NULL), "insqtail: NULL count is INVARG");
	CHECK( UTIL$S_INVARG == $INSQHEAD(&l_q, &s_it[0], NULL), "insqhead: NULL count is INVARG");

	CHECK( STS$K_SUCCESS == $INSQTAIL(&l_q, &s_it[0], &l_cnt), "insqtail into the first queue");
	CHECK( STS$K_SUCCESS == $INSQTAIL(&l_q, &s_it[0], &l_cnt), "the same entry again: success, nothing changes");
	CHECK( STS$K_SUCCESS == $INSQHEAD(&l_q, &s_it[0], &l_cnt), "the same entry by insqhead: success, nothing changes");
	CHECK( 1 == l_q.count, "...the count is 1");
	CHECK( UTIL$S_INQUE == $INSQTAIL(&l_q2, &s_it[0], &l_cnt), "insqtail of an entry of another queue is INQUE");
	CHECK( UTIL$S_INQUE == $INSQHEAD(&l_q2, &s_it[0], &l_cnt), "insqhead of an entry of another queue is INQUE (same as insqtail)");
	CHECK( 0 == l_q2.count, "...and the other queue is untouched");

	/* ---- Group 3: movqtail */
	s_load(&l_q, 5);
	CHECK( 1 & $MOVQTAIL(&l_q, &s_it[2]), "movqtail of the middle entry");
	l_ids[0] = 0; l_ids[1] = 1; l_ids[2] = 3; l_ids[3] = 4; l_ids[4] = 2;
	CHECK( s_check(&l_q, l_ids, 5), "...order 0 1 3 4 2, head/tail/chains right");

	s_load(&l_q, 5);
	CHECK( 1 & $MOVQTAIL(&l_q, &s_it[0]), "movqtail of the head");
	l_ids[0] = 1; l_ids[1] = 2; l_ids[2] = 3; l_ids[3] = 4; l_ids[4] = 0;
	CHECK( s_check(&l_q, l_ids, 5), "...order 1 2 3 4 0, the head is re-pointed");

	s_load(&l_q, 5);
	CHECK( 1 & $MOVQTAIL(&l_q, &s_it[4]), "movqtail of the tail itself");
	l_ids[0] = 0; l_ids[1] = 1; l_ids[2] = 2; l_ids[3] = 3; l_ids[4] = 4;
	CHECK( s_check(&l_q, l_ids, 5), "...nothing changes");

	s_load(&l_q, 1);
	CHECK( 1 & $MOVQTAIL(&l_q, &s_it[0]), "movqtail on a queue of one");
	l_ids[0] = 0;
	CHECK( s_check(&l_q, l_ids, 1), "...nothing changes");

	s_load(&l_q, 2);
	CHECK( 1 & $MOVQTAIL(&l_q, &s_it[0]), "movqtail of the head of two");
	l_ids[0] = 1; l_ids[1] = 0;
	CHECK( s_check(&l_q, l_ids, 2), "...the two swap");

	s_load(&l_q, 3);
	CHECK( !(1 & $MOVQTAIL(&l_q, &s_it[7])), "movqtail of a stranger is an error");
	CHECK( !(1 & $MOVQTAIL(&l_q, NULL)), "movqtail of NULL is an error");
	l_ids[0] = 0; l_ids[1] = 1; l_ids[2] = 2;
	CHECK( s_check(&l_q, l_ids, 3), "...and the queue is as it was");

	/* ---- Group 4: movqhead */
	s_load(&l_q, 5);
	CHECK( 1 & $MOVQHEAD(&l_q, &s_it[4]), "movqhead of the tail");
	l_ids[0] = 4; l_ids[1] = 0; l_ids[2] = 1; l_ids[3] = 2; l_ids[4] = 3;
	CHECK( s_check(&l_q, l_ids, 5), "...order 4 0 1 2 3, the TAIL is re-pointed to 3");

	s_load(&l_q, 5);
	CHECK( 1 & $MOVQHEAD(&l_q, &s_it[2]), "movqhead of the middle entry");
	l_ids[0] = 2; l_ids[1] = 0; l_ids[2] = 1; l_ids[3] = 3; l_ids[4] = 4;
	CHECK( s_check(&l_q, l_ids, 5), "...order 2 0 1 3 4, the old head points back to the moved one");

	s_load(&l_q, 5);
	CHECK( 1 & $MOVQHEAD(&l_q, &s_it[0]), "movqhead of the head itself");
	l_ids[0] = 0; l_ids[1] = 1; l_ids[2] = 2; l_ids[3] = 3; l_ids[4] = 4;
	CHECK( s_check(&l_q, l_ids, 5), "...nothing changes");

	s_load(&l_q, 2);
	CHECK( 1 & $MOVQHEAD(&l_q, &s_it[1]), "movqhead of the tail of two");
	l_ids[0] = 1; l_ids[1] = 0;
	CHECK( s_check(&l_q, l_ids, 2), "...the two swap, tail is 0");

	s_load(&l_q, 3);
	CHECK( !(1 & $MOVQHEAD(&l_q, &s_it[9])), "movqhead of a stranger is an error");
	l_ids[0] = 0; l_ids[1] = 1; l_ids[2] = 2;
	CHECK( s_check(&l_q, l_ids, 3), "...and the queue is as it was");

	/* ---- Group 5: remq of the ends and of an entry, with a real queue */
	s_load(&l_q, 4);
	CHECK( (1 & $REMQHEAD(&l_q, &l_ent, &l_cnt)) && (l_ent == &s_it[0]) && (4 == l_cnt), "remqhead takes 0, count before = 4");
	CHECK( (1 & $REMQTAIL(&l_q, &l_ent, &l_cnt)) && (l_ent == &s_it[3]) && (3 == l_cnt), "remqtail takes 3, count before = 3");
	CHECK( (1 & $REMQENT(&l_q, &s_it[1], &l_cnt)) && (2 == l_cnt), "remqent takes 1 (the head now), count before = 2");
	l_ids[0] = 2;
	CHECK( s_check(&l_q, l_ids, 1), "only 2 is left");
	CHECK( (1 & $REMQHEAD(&l_q, &l_ent, &l_cnt)) && (l_ent == &s_it[2]) && (1 == l_cnt), "remqhead takes the last one");
	CHECK( (NULL == l_q.head) && (NULL == l_q.tail) && (0 == l_q.count), "head, tail, count are cleared");
	CHECK( NULL == s_it[2].e.queue, "the removed entry belongs to no queue");
	CHECK( 1 & $INSQHEAD(&l_q, &s_it[2], &l_cnt), "...and can be inserted again");

	s_load(&l_q, 4);
	CHECK( (1 & $CLRQUE(&l_q, &l_cnt)) && (4 == l_cnt), "clrqueue reports 4 and");
	CHECK( (NULL == l_q.head) && (NULL == l_q.tail) && (0 == l_q.count) && (NULL == s_it[1].e.queue), "empties the queue and releases the entries");

	/* ---- Group 6: a random walk against an array model */
	{
	int	l_model [NITEM], l_n = 0, l_in [NITEM], l_bad = 0;
	long	l_step;

	s_load(&l_q, 0);
	memset(l_in, 0, sizeof(l_in));
	srand(20261031);

	for ( l_step = 0; l_step < 20000 && !l_bad; l_step++ )
		{
		int	l_op = rand() % 7, l_id = rand() % NITEM, l_k, l_pos;

		switch ( l_op )
			{
			case 0:					/* insqtail */
			case 1:					/* insqhead */
				if ( l_in[l_id] )
					break;

				if ( l_op == 0 )
					{
					$INSQTAIL(&l_q, &s_it[l_id], &l_cnt);
					l_model[l_n++] = l_id;
					}
				else	{
					$INSQHEAD(&l_q, &s_it[l_id], &l_cnt);
					memmove(l_model + 1, l_model, sizeof(int) * (size_t) l_n);
					l_model[0] = l_id;
					l_n++;
					}
				l_in[l_id] = 1;
				break;

			case 2:					/* remqhead */
				$REMQHEAD(&l_q, &l_ent, &l_cnt);
				if ( l_n )
					{
					l_bad |= (l_ent != &s_it[l_model[0]]);
					l_in[l_model[0]] = 0;
					memmove(l_model, l_model + 1, sizeof(int) * (size_t) --l_n);
					}
				else	l_bad |= (l_ent != NULL);
				break;

			case 3:					/* remqtail */
				$REMQTAIL(&l_q, &l_ent, &l_cnt);
				if ( l_n )
					{
					l_bad |= (l_ent != &s_it[l_model[l_n - 1]]);
					l_in[l_model[--l_n]] = 0;
					}
				else	l_bad |= (l_ent != NULL);
				break;

			case 4:					/* remqent */
				l_sts = $REMQENT(&l_q, &s_it[l_id], &l_cnt);
				l_bad |= (!!(1 & l_sts) != l_in[l_id]);
				if ( l_in[l_id] )
					{
					for ( l_pos = 0; l_model[l_pos] != l_id; l_pos++ )
						;
					memmove(l_model + l_pos, l_model + l_pos + 1, sizeof(int) * (size_t) (l_n - l_pos - 1));
					l_n--;
					l_in[l_id] = 0;
					}
				break;

			default:				/* movqhead / movqtail */
				l_sts = (l_op == 5) ? $MOVQHEAD(&l_q, &s_it[l_id]) : $MOVQTAIL(&l_q, &s_it[l_id]);
				l_bad |= (!!(1 & l_sts) != l_in[l_id]);
				if ( l_in[l_id] )
					{
					for ( l_pos = 0; l_model[l_pos] != l_id; l_pos++ )
						;
					memmove(l_model + l_pos, l_model + l_pos + 1, sizeof(int) * (size_t) (l_n - l_pos - 1));
					if ( l_op == 5 )
						{
						memmove(l_model + 1, l_model, sizeof(int) * (size_t) (l_n - 1));
						l_model[0] = l_id;
						}
					else	l_model[l_n - 1] = l_id;
					}
				break;
			}

		for ( l_k = 0; l_k < l_n; l_k++ )
			l_ids[l_k] = l_model[l_k];

		if ( !s_check(&l_q, l_ids, l_n) )
			l_bad = 1;
		}

	CHECK( !l_bad, "20000 random operations agree with the model (stopped at step %ld)", l_step);
	}

	TEST_DONE();
}
