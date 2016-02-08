#include <pthread.h>

typedef struct rwlock_tag {
    pthread_mutex_t     mutex;
    pthread_cond_t      read;           /* wait for read */
    pthread_cond_t      write;          /* wait for write */
    int                 valid;          /* set when valid */
    int                 r_active;       /* readers active */
    int                 w_active;       /* writer active */
    int                 r_wait;         /* readers waiting */
    int                 w_wait;         /* writers waiting */
} rwlock_t;

#define RWLOCK_VALID 0xfacade

#define RWL_INITIALIZER \
	{ PTHREAD_MUTEX_INITIALIZER, PTHREAD_COND_INITIALIZER, \
	PTHREAD_COND_INITIALIZER, RWLOCK_VALID, 0, 0, 0, 0}
	
extern int rwl_init (rwlock_t *rwl); //��ʼ����д��
extern int rwl_destroy (rwlock_t *rwl); //���ٶ�д��
extern int rwl_readlock (rwlock_t *rwl); //�Ӷ���
extern int rwl_readtrylock (rwlock_t *rwl); //trylock
extern int rwl_readunlock (rwlock_t *rwl); //�ͷŶ���
extern int rwl_writelock (rwlock_t *rwl); //��д��
extern int rwl_writetrylock (rwlock_t *rwl); //trylock
extern int rwl_writeunlock (rwlock_t *rwl); //�ͷ�д��