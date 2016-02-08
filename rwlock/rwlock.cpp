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
	rwl->valid = RWLOCK_VALID; /*设置魔方数字标志*/
	
	return 0;
}

int rwl_destroy (rwlock_t *rwl)
{
	int status, status1, status2;

	/*先判断读写锁是否被正确初始化*/
  if (rwl->valid != RWLOCK_VALID)
  	return EINVAL;
  	
  status = pthread_mutex_lock (&rwl->mutex);
  if (status != 0)
  	return status;
  	
  /*确保没有正在占有的读/写线程，如果有，返回busy*/
  if(rwl->r_active > 0 || rwl->w_active)
  {
  	pthread_mutex_unlock (&rwl->mutex);
  	return EBUSY;
  }
  
  /*确保没有正在等待的读/写线程，如果有，返回busy*/
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

/*读线程销毁时的清理函数，避免销毁一个正在等待的读线程时，使程序进入无限等待*/
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
  	
  /*如果有写线程正在占有，则进入等待*/
  if(rwl->w_active)
  {
  	rwl->r_wait++;
  	pthread_cleanup_push(rwl_readcleanup, (void*)rwl); //设置线程取消时的清理函数
  	
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
  	rwl->r_active++; //增加读占有线程数目
  
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
    	
    /*主要判断是否有写线程正在占有写锁*/
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
  	
  /*读占有线程数目减少，如果此时已经没有读线程占有和等待，同时存在等待的写线程，则唤醒一个写线程*/
  rwl->r_active--;
  if(rwl->r_active == 0 && rwl->w_wait > 0)
  	status = pthread_cond_signal(&rwl->write);
  
  status2 = pthread_mutex_unlock (&rwl->mutex);
  
  return (status2 == 0 ? status : status2);
}

/*写线程销毁时的清理函数，避免销毁一个正在等待的写线程时，使程序进入无限等待*/
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
  	
  /*如果存在读占有线程或写占有线程，则进入等待*/
  if(rwl->w_active || rwl->r_active > 0)
  {
  	rwl->w_wait++;
  	pthread_cleanup_push(rwl_writecleanup, (void*)rwl); //设置线程取消时的清理函数
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
  	
  /*主要检测是否存在读/写占有线程*/
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
  	
  /*由于是读者优先，所以首先检测是否存在读等待线程，有则唤醒*/
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
  else if(rwl->w_wait > 0) /*如果没有读等待线程，同时存在写等待线程则唤醒一个写等待线程*/
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