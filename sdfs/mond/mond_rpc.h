#ifndef __MOND_RPC_H__
#define __MOND_RPC_H__

#include <sys/statvfs.h>

#include "disk_proto.h"
#include "md_proto.h"

typedef struct {
        nid_t nid;
        int online;
} instat_t;  

typedef struct {
        int klen;
        int vlen;
        unsigned char type;
        unsigned char eof;
        uint64_t offset;
        char buf[0];
} mon_entry_t;


#define MON_ENTRY_MAX (64 * 1024)

int mond_rpc_init();
int mond_rpc_getstat(const nid_t *nid, instat_t *instat);
int mond_rpc_diskhb(const nid_t *nid, int tier, const uuid_t *uuid,
                    const diskinfo_stat_diff_t *diff,
                    const volinfo_t *volinfo);
int mond_rpc_statvfs(const nid_t *nid, const fileid_t *fileid, struct statvfs *stbuf);
int mond_rpc_diskjoin(const nid_t *nid, uint32_t tier, const uuid_t *uuid,
                      const diskinfo_stat_t *stat);
int mond_rpc_newdisk(const nid_t *nid, uint32_t tier, uint32_t repnum,
                     uint32_t hardend, diskid_t *disks);
int mond_rpc_null(const nid_t *mond);
int mond_rpc_set(const nid_t *nid, const char *path, const char *value, uint32_t valuelen);
int mond_rpc_get(const nid_t *nid, const char *path, uint64_t offset, void *value, int *valuelen);

#endif
