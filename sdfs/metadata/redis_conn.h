#ifndef __REDIS_CONN_H__
#define __REDIS_CONN_H__

#include <hiredis/hiredis.h>
//#include <hircluster.h>
#include "sdfs_id.h"
#include "redis_util.h"
#include "cJSON.h"
#include "zlib.h"

typedef struct{
        int magic;
        int erased;
        int used;
        redis_conn_t *conn;
} __conn_t;

typedef struct{
        pthread_rwlock_t lock;
        int sequence;
        int count;
        __conn_t *conn;
} __conn_sharding_t;

typedef struct {
        pthread_rwlock_t lock;
        int sequence;
        int sharding;
        __conn_sharding_t *shardings;
        volid_t volid;
        char volume[MAX_NAME_LEN];
} redis_vol_t;

typedef struct {
        int magic;
        int sharding;
        int idx;
        volid_t volid;
        redis_conn_t *conn;
} redis_handler_t;

int redis_conn_init();
int redis_conn_release(const redis_handler_t *handler);
int redis_conn_get(const volid_t *volid, int sharding, uint32_t worker,
                   redis_handler_t *handler);
int redis_conn_new(const volid_t *volid, uint8_t *idx);
int redis_conn_close(const redis_handler_t *handler);
int redis_conn_vol(const volid_t *volid);
void redis_conn_vol_close(void *vol);

int redis_vol_get(const volid_t *volid, void **conn);
int redis_vol_release(const volid_t *volid);
int redis_vol_insert(const volid_t *volid, void *conn);
int redis_vol_init();
int redis_vol_private_init();
void redis_vol_private_destroy(func_t func);

#endif
