#include <pthread.h>
#include "rwlock.h"
#include "errors.h"

int rwl_init (rwlock_t *rwl)
{
	int status;
	
	rwl->r_active = 0;
  rwl->r_wait = rwl->w_wait = 0;
  rwl->w_active = 0;
	
	status = pthread_mutex_init(&rwl->mutex, NULL);
	if(status != 0)
	{
		return status;
	}
	
	status = pthread_cond_init(&rwl->read, NULL);
	if(status != 0)
	{
		pthread_mutex_destroy(&rwl->mutex);
		return status;
	}
	
	status = pthread_cond_init(&rwl->write, NULL);
	if(status != 0)
	{
		pthread_cond_destroy(&rwl->read);
		pthread_mutex_destroy(&rwl->mutex);
		return status;
	}
	rwl->valid = RWLOCK_VALID; /*����ħ�����ֱ�־*/
	
	return 0;
}

int rwl_destroy (rwlock_t *rwl)
{
	int status, status1, status2;

	/*���ж϶�д���Ƿ���ȷ��ʼ��*/
  if (rwl->valid != RWLOCK_VALID)
  	return EINVAL;
  	
  status = pthread_mutex_lock (&rwl->mutex);
  if (status != 0)
  	return status;
  	
  /*ȷ��û������ռ�еĶ�/д�̣߳�����У�����busy*/
  if(rwl->r_active > 0 || rwl->w_active)
  {
  	pthread_mutex_unlock (&rwl->mutex);
  	return EBUSY;
  }
  
  /*ȷ��û�����ڵȴ��Ķ�/д�̣߳�����У�����busy*/
  if(rwl->r_wait != 0 || rwl->w_wait != 0)
  {
  	pthread_mutex_unlock (&rwl->mutex);
  	return EBUSY;
  }
  
  rwl->valid = 0;
  status = pthread_mutex_unlock (&rwl->mutex);
  if (status != 0)
  	return status;
  	
  status = pthread_mutex_destroy (&rwl->mutex);
  status1 = pthread_cond_destroy (&rwl->read);
  status2 = pthread_cond_destroy (&rwl->write);
  
  return (status == 0 ? status : (status1 == 0 ? status1 : status2));
}

/*���߳�����ʱ������������������һ�����ڵȴ��Ķ��߳�ʱ��ʹ����������޵ȴ�*/
static void rwl_readcleanup (void *arg)
{
	rwlock_t *rwl = (rwlock_t *)arg;

  rwl->r_wait--;
  pthread_mutex_unlock (&rwl->mutex);
}

int rwl_readlock (rwlock_t *rwl)
{
	int status;

  if (rwl->valid != RWLOCK_VALID)
  	return EINVAL;
  	
  status = pthread_mutex_lock (&rwl->mutex);
  if (status != 0)
  	return status;
  	
  /*�����д�߳�����ռ�У������ȴ�*/
  if(rwl->w_active)
  {
  	rwl->r_wait++;
  	pthread_cleanup_push(rwl_readcleanup, (void*)rwl); //�����߳�ȡ��ʱ��������
  	
  	while(rwl->w_active)
  	{
  		status = pthread_cond_wait(&rwl->read, &rwl->mutex);
  		if (status != 0)
      	break;
  	}
  	pthread_cleanup_pop(0);
  	rwl->r_wait--;
  }
  if (status == 0)
  	rwl->r_active++; //���Ӷ�ռ���߳���Ŀ
  
  pthread_mutex_unlock (&rwl->mutex);
  
  return status;
}

int rwl_readtrylock (rwlock_t *rwl)
{
	  int status, status2;

    if (rwl->valid != RWLOCK_VALID)
    	return EINVAL;
    
    status = pthread_mutex_lock (&rwl->mutex);
    if (status != 0)
    	return status;
    	
    /*��Ҫ�ж��Ƿ���д�߳�����ռ��д��*/
    if(rwl->w_active)
    	status = EBUSY;
    else
    	rwl->r_active++;
    
    status = pthread_mutex_unlock(&rwl->mutex);
    
    return (status2 != 0 ? status2 : status);
}

int rwl_readunlock (rwlock_t *rwl)
{
	int status, status2;

  if (rwl->valid != RWLOCK_VALID)
  	return EINVAL;
  
  status = pthread_mutex_lock (&rwl->mutex);
  if (status != 0)
  	return status;
  	
  /*��ռ���߳���Ŀ���٣������ʱ�Ѿ�û�ж��߳�ռ�к͵ȴ���ͬʱ���ڵȴ���д�̣߳�����һ��д�߳�*/
  rwl->r_active--;
  if(rwl->r_active == 0 && rwl->w_wait > 0)
  	status = pthread_cond_signal(&rwl->write);
  
  status2 = pthread_mutex_unlock (&rwl->mutex);
  
  return (status2 == 0 ? status : status2);
}

/*д�߳�����ʱ������������������һ�����ڵȴ���д�߳�ʱ��ʹ����������޵ȴ�*/
static void rwl_writecleanup (void *arg)
{
	rwlock_t *rwl = (rwlock_t *)arg;

  rwl->w_wait--;
  pthread_mutex_unlock (&rwl->mutex);
}

int rwl_writelock (rwlock_t *rwl)
{
	int status;

  if (rwl->valid != RWLOCK_VALID)
  	return EINVAL;
    
  status = pthread_mutex_lock (&rwl->mutex);
  if (status != 0)
  	return status;
  	
  /*������ڶ�ռ���̻߳�дռ���̣߳������ȴ�*/
  if(rwl->w_active || rwl->r_active > 0)
  {
  	rwl->w_wait++;
  	pthread_cleanup_push(rwl_writecleanup, (void*)rwl); //�����߳�ȡ��ʱ��������
  	while(rwl->w_active || rwl->r_active > 0)
  	{
  		status = pthread_cond_wait(&rwl->write, &rwl->mutex);
  		if(status != 0)
  			break;
  	}
  	pthread_cleanup_pop(0);
  	rwl->w_wait--;
  }
  
  if(status == 0)
  {
  	rwl->w_active = 1;
  }
  pthread_mutex_unlock (&rwl->mutex);
  
  return status;
}

int rwl_writetrylock (rwlock_t *rwl)
{
	int status, status2;

  if (rwl->valid != RWLOCK_VALID)
  	return EINVAL;
  
  status = pthread_mutex_lock (&rwl->mutex);
  if (status != 0)
  	return status;
  	
  /*��Ҫ����Ƿ���ڶ�/дռ���߳�*/
  if(rwl->w_active || rwl->r_active > 0)
  	status = EBUSY;
  else
  	rwl->w_active = 1;
  	
  status2 = pthread_mutex_unlock (&rwl->mutex);
  
  return (status != 0 ? status : status2);
}

int rwl_writeunlock (rwlock_t *rwl)
{
	int status;

  if (rwl->valid != RWLOCK_VALID)
  	return EINVAL;
    
  status = pthread_mutex_lock (&rwl->mutex);
  if (status != 0)
  	return status;
  	
  /*�����Ƕ������ȣ��������ȼ���Ƿ���ڶ��ȴ��̣߳�������*/
  rwl->w_active = 0;
  if(rwl->r_wait > 0)
  {
  	status = pthread_cond_broadcast(&rwl->read);
  	if (status != 0) 
  	{
    	pthread_mutex_unlock (&rwl->mutex);
      return status;
    }
  }
  else if(rwl->w_wait > 0) /*���û�ж��ȴ��̣߳�ͬʱ����д�ȴ��߳�����һ��д�ȴ��߳�*/
  {
  	status = pthread_cond_signal(&rwl->write);
  	if(status != 0)
  	{
  		pthread_mutex_unlock(&rwl->mutex);
  		return status;
  	}
  }
  status = pthread_mutex_unlock(&rwl->mutex);
  
  return status;
}