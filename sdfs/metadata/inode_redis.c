#include <sys/types.h>
#include <string.h>
#include <stdint.h>
#include <dirent.h>
#include <errno.h>

#define DBG_SUBSYS S_YFSMDS

#include "dir.h"
#include "schedule.h"
#include "net_global.h"
#include "redis_conn.h"
#include "md_lib.h"
#include "redis.h"
#include "redis_pipeline.h"
#include "quota.h"
#include "attr_queue.h"
#include "md.h"
#include "md_db.h"
#include "dbg.h"

static inodeop_t *inodeop = &__inodeop__;

static int __inode_childcount(const volid_t *volid, const fileid_t *fid, uint64_t *_count);
static int __inode_remove(const volid_t *volid, const fileid_t *fileid, md_proto_t *_md);

static int __md_set(const volid_t *volid, const md_proto_t *md, int flag)
{
        int ret;

        ret = hset(volid, &md->fileid, SDFS_MD, md, md->md_size, flag);
        if (ret)
                GOTO(err_ret, ret);

        DBUG(CHKID_FORMAT" nlink %d, size %ju\n", CHKID_ARG(&md->fileid),
              md->at_nlink, md->at_size);
        
        if (mdsconf.ac_timeout) {
                attr_cache_update(volid, &md->fileid, md);
        }
        
        return 0;
err_ret:
        return ret;
}

static int __inode_setlock(const volid_t *volid, const fileid_t *fileid,
                           const void *opaque, size_t len, int flag)
{
        int ret;

        if (fileid->type != ftype_file) {
                ret = EPERM;
                GOTO(err_ret, ret);
        }
        
        ret = hset(volid, fileid, SDFS_LOCK, opaque, len, flag);
        if (ret)
                GOTO(err_ret, ret);
        
        return 0;
err_ret:
        return ret;
}

static int __inode_getlock(const volid_t *volid, const fileid_t *fileid,
                           void *opaque, size_t *len)
{
        int ret;

        if (fileid->type != ftype_file) {
                ret = EPERM;
                GOTO(err_ret, ret);
        }
        
        ret = hget(volid, fileid, SDFS_LOCK, opaque, len);
        if (ret)
                GOTO(err_ret, ret);
        
        return 0;
err_ret:
        return ret;
}

static void __inode_getxattrid(const fileid_t *fileid, fileid_t *xattrid, int flag)
{
        (void) flag;

        *xattrid = *fileid;
        xattrid->type = ftype_xattr;
}

static int __inode_create(const volid_t *volid, const fileid_t *parent,
                          const setattr_t *setattr,
                          int type, fileid_t *_fileid)
{
        int ret;
        char buf[MAX_BUF_LEN], buf1[MAX_BUF_LEN];
        fileid_t fileid;
        md_proto_t *md_parent, *md;

        ANALYSIS_BEGIN(0);
        
        md_parent = (md_proto_t *)buf;
        ret = inodeop->getattr(volid, parent, md_parent);
        if (ret)
                GOTO(err_ret, ret);

        ret = md_attr_getid(&fileid, parent, type, volid);
        if (ret)
                GOTO(err_ret, ret);

        md = (void *)buf1;
        ret = md_attr_init((void *)md, setattr, type, md_parent, &fileid);
        if (ret)
                GOTO(err_ret, ret);

        ret = __md_set(volid, md, O_EXCL);
        if (ret)
                GOTO(err_ret, ret);

        if (_fileid) {
                *_fileid = fileid;
        }

        ANALYSIS_QUEUE(0, IO_WARN, NULL);
        
        return 0;
err_ret:
        return ret;
}

static int __md_get(const volid_t *volid, const fileid_t *fileid, md_proto_t *md)
{
        int ret;
        size_t len;
        char buf[MAX_BUF_LEN] = {0};
        uint64_t count;

        DBUG("getattr "CHKID_FORMAT"\n", CHKID_ARG(fileid));
        
        //DWARN("--------------pipeline test--------------------\n");
        len = MAX_BUF_LEN;
        ret = hget(volid, fileid, SDFS_MD, buf, &len);
        if (ret) {
                if (ret == ENOENT) {
                        memset(md, 0x0, sizeof(*md));
                        md->fileid = *fileid;
                        goto out;
                } else
                        GOTO(err_ret, ret);
        }

        memcpy(md, buf, len);
        YASSERT(md->md_size == len);

        if (S_ISDIR(stype(fileid->type))) {
                ret = __inode_childcount(volid, fileid, &count);
                if (ret)
                        GOTO(err_ret, ret);

                md->at_nlink = count + 2;
        }

out:
        return 0;
err_ret:
        return ret;
}

static int __inode_getattr(const volid_t *volid, const fileid_t *fileid, md_proto_t *md)
{
        int ret;

        if (mdsconf.ac_timeout == 0) {
                ret = __md_get(volid, fileid, md);
                if (unlikely(ret))
                        GOTO(err_ret, ret);

                DBUG(CHKID_FORMAT" nlink %d, size %ju\n", CHKID_ARG(fileid),
                      md->at_nlink, md->at_size);
        } else {
                ret = attr_cache_get(volid, fileid, md);
                if (ret) {
                        if (ret == ENOENT) {
                                ret = __md_get(volid, fileid, md);
                                if (unlikely(ret))
                                        GOTO(err_ret, ret);

                                attr_cache_update(volid, fileid, md);
                        } else
                                GOTO(err_ret, ret);
                }

                DBUG(CHKID_FORMAT" nlink %d, size %ju\n", CHKID_ARG(fileid),
                      md->at_nlink, md->at_size);
        }

        return 0;
err_ret:
        return ret;
}

static int __inode_setattr(const volid_t *volid, const fileid_t *fileid,
                           const setattr_t *setattr, int force)
{
        int ret;
        char buf[MAX_BUF_LEN] = {0};
        md_proto_t *md;

        DBUG("setattr "CHKID_FORMAT", force %u\n", CHKID_ARG(fileid), force);
        
        ret = klock(volid, fileid, 10, force ? 1 : 0);
        if (ret) {
                if (ret == EAGAIN && force == 0) {
                        goto out;
                } else
                        GOTO(err_ret, ret);
        }
        
        md = (void *)buf;
        ret = __inode_getattr(volid, fileid, md);
        if (ret)
                GOTO(err_lock, ret);
        
        md_attr_update(md, setattr);
        DBUG(CHKID_FORMAT" nlink %d, size %ju\n", CHKID_ARG(fileid),
              md->at_nlink, md->at_size);
        YASSERT(md->at_mode);
        
        ret = __md_set(volid, md, 0);
        if (ret)
                GOTO(err_lock, ret);

        ret = kunlock(volid, fileid);
        if (ret)
                GOTO(err_ret, ret);

out:
        return 0;
err_lock:
        kunlock(volid, fileid);
err_ret:
        return ret;
}

static int __inode_extend(const volid_t *volid, const fileid_t *fileid, size_t size)
{
        int ret, retry = 0;
        char buf[MAX_BUF_LEN] = {0};
        md_proto_t *md;

        md = (void *)buf;
retry:
        ret = __inode_getattr(volid, fileid, md);
        if (ret)
                GOTO(err_ret, ret);

        if (md->at_size >= size) {
                return 0;
        }

        (void) retry;
        
        ret = klock(volid, fileid, 10, 0);
        if (ret) {
#if 1
                if (retry > 500 && retry % 100 == 0 ) {
                        DWARN("lock "CHKID_FORMAT", retry %u\n", CHKID_ARG(fileid), retry);
                }
                USLEEP_RETRY(err_ret, ret, retry, retry, 1000, (5 * 1000));
#else
                GOTO(err_ret, ret);
#endif
        }
        
        ret = __inode_getattr(volid, fileid, md);
        if (ret)
                GOTO(err_lock, ret);

        if (md->at_size < size) {
#if ENABLE_QUOTA
                ret = quota_space_increase(&md->parent, md->at_uid,
                                           md->at_gid, size - md->at_size);
                if (ret)
                        GOTO(err_lock, ret);
#endif

                md->at_size = size;
                md->chknum = _get_chknum(md->at_size, md->split);
                md->md_version++;
                ret = __md_set(volid, md, 0);
                if (ret)
                        GOTO(err_lock, ret);
        }

        ret = kunlock(volid, fileid);
        if (ret)
                GOTO(err_ret, ret);
        
        return 0;
err_lock:
        kunlock(volid, fileid);
err_ret:
        return ret;
}

static int __inode_del(const volid_t *volid, const fileid_t *fileid)
{
        int ret;
        fileid_t xattrid;

        //YASSERT(fileid->type != ftype_vol);
        DBUG("del "CHKID_FORMAT" \n", CHKID_ARG(fileid));
        
        __inode_getxattrid(fileid, &xattrid, 0);
        
        ret = kdel(volid, fileid);
        if (ret)
                GOTO(err_ret, ret);

        ret = kdel(volid, &xattrid);
        if (ret) {
                if (ret == ENOENT) {
                        //pass
                } else {
                        GOTO(err_ret, ret);
                }
        }
        
        return 0;
err_ret:
        return ret;
}

static int __inode_setxattr(const volid_t *volid, const fileid_t *id, const char *key,
                            const char *value, size_t size, int flag)
{
        int ret;
        fileid_t xattrid;

        __inode_getxattrid(id, &xattrid, O_CREAT);

        ret = hset(volid, &xattrid, key, value, size, flag);
        if (ret)
                GOTO(err_ret, ret);
        
        return 0;
err_ret:
        return ret;
}

static int __inode_getxattr(const volid_t *volid, const fileid_t *id, const char *key, char *value, size_t *value_len)
{
        int ret;
        fileid_t xattrid;

        __inode_getxattrid(id, &xattrid, 0);

        ret = hget(volid, &xattrid, key, value, value_len);
        if (ret) {
                ret = ENOENT ? ENOKEY : ret;
                GOTO(err_ret, ret);
        }

        return 0;
err_ret:
        return ret;
}

static int __inode_removexattr(const volid_t *volid, const fileid_t *id, const char *key)
{
        int ret;
        fileid_t xattrid;

        __inode_getxattrid(id, &xattrid, 0);

        ret = hdel(volid, &xattrid, key);
        if (ret)
                GOTO(err_ret, ret);
        
        return 0;
err_ret:
        return ret;
}

static int __inode_listxattr(const volid_t *volid, const fileid_t *id, char *list, size_t *size)
{
        int ret, len, left, i;
        fileid_t xattrid;
        redisReply *reply, *e1, *k1;

        __inode_getxattrid(id, &xattrid, 0);

        reply = hscan(volid, &xattrid, NULL, 0, -1);
        if (reply == NULL || reply->type != REDIS_REPLY_ARRAY) {
                ret = ECONNRESET;
                GOTO(err_ret, ret);
        }

        //e0 = reply->element[0];
        e1 = reply->element[1];

        list[0] = '\0';
        left = *size;
        for (i = 0; i < (int)e1->elements; i += 2) {
                k1 = e1->element[i];

                len = strlen(k1->str);
                if (left < len + 1) {
                        UNIMPLEMENTED(__DUMP__);
                }
                
                snprintf(list + len, left, "%s\n", k1->str);

                left -= (len + 1);
        }

        *size = strlen(list);
        
        freeReplyObject(reply);

        return 0;
//err_free:
        //freeReplyObject(reply);
err_ret:
        return ret;
}

static int __inode_childcount(const volid_t *volid, const fileid_t *fileid, uint64_t *_count)
{
        int ret;
        uint64_t count;

        if (!S_ISDIR(stype(fileid->type))) {
                ret = ENOTDIR;
                GOTO(err_ret, ret);
        }
        
        ret = hlen(volid, fileid, &count);
        if (ret)
                GOTO(err_ret, ret);

        *_count = count - 1;

        return 0;
err_ret:
        return ret;
}

static int __inode_link(const volid_t *volid, const fileid_t *fileid)
{
        int ret;
        md_proto_t *md;
        char buf[MAX_BUF_LEN];
        
        ret = klock(volid, fileid, 10, 1);
        if (ret)
                GOTO(err_ret, ret);
        
        md = (void *)buf;
        ret = __inode_getattr(volid, fileid, md);
        if (ret)
                GOTO(err_lock, ret);

        md->at_nlink++;
        md->md_version++;

        ret = __md_set(volid, md, 0);
        if (ret)
                GOTO(err_lock, ret);
        
        ret = kunlock(volid, fileid);
        if (ret)
                GOTO(err_ret, ret);

        return 0;
err_lock:
        kunlock(volid, fileid);
err_ret:
        return ret;
}

static int __inode_unlink(const volid_t *volid, const fileid_t *fileid, md_proto_t *_md)
{
        int ret;
        md_proto_t *md;
        char buf[MAX_BUF_LEN];

        if (S_ISDIR(stype(fileid->type))) {
                return __inode_remove(volid, fileid, _md);
        }
        
        ret = klock(volid, fileid, 10, 1);
        if (ret)
                GOTO(err_ret, ret);

        md = (void *)buf;
        ret = __inode_getattr(volid, fileid, md);
        if (ret)
                GOTO(err_lock, ret);

        md->at_nlink--;
        md->md_version++;

        DBUG(CHKID_FORMAT" nlink %d\n", CHKID_ARG(fileid), md->at_nlink);

        if (_md) {
                memcpy(_md, md, md->md_size);
        }

#if 1
        ret = __md_set(volid, md, 0);
        if (ret)
                GOTO(err_lock, ret);
#else
        if (md->at_nlink == 0) {
                ret = __inode_del(fileid);
                if (ret)
                        GOTO(err_lock, ret);
        } else {
                ret = __md_set(volid, md, 0);
                if (ret)
                        GOTO(err_lock, ret);
        }
#endif
        
        ret = kunlock(volid, fileid);
        if (ret)
                GOTO(err_ret, ret);

        return 0;
err_lock:
        kunlock(volid, fileid);
err_ret:
        return ret;
}

static int __inode_symlink(const volid_t *volid, const fileid_t *fileid, const char *link_target)
{
        int ret;
        symlink_md_t *md;
        char buf[MAX_BUF_LEN];

        md = (void *)buf;
        ret = __inode_getattr(volid, fileid, (void *)md);
        if (ret)
                GOTO(err_ret, ret);

        strcpy(md->name, link_target);
        md->md_size += (strlen(link_target) + 1);
        md->md_version++;

        DBUG(CHKID_FORMAT" link %s\n", CHKID_ARG(fileid), md->name);
        
        ret = __md_set(volid, (void *)md, 0);
        if (ret)
                GOTO(err_ret, ret);

        return 0;
err_ret:
        return ret;
}

static int __inode_readlink(const volid_t *volid, const fileid_t *fileid, char *link_target)
{
        int ret;
        symlink_md_t *md;
        char buf[MAX_BUF_LEN];

        md = (void *)buf;
        ret = __inode_getattr(volid, fileid, (void *)md);
        if (ret)
                GOTO(err_ret, ret);

        DBUG(CHKID_FORMAT" link %s\n", CHKID_ARG(fileid), md->name);
        
        strcpy(link_target, md->name);

        return 0;
err_ret:
        return ret;
}

static int __inode_mkvol(const volid_t *volid, const fileid_t *fileid, const setattr_t *setattr)
{
        int ret;
        char buf1[MAX_BUF_LEN];
        md_proto_t *md;

        md = (void *)buf1;
        ret = md_attr_init((void *)md, setattr, ftype_vol, NULL, fileid);
        if (ret)
                GOTO(err_ret, ret);

        ret = __md_set(volid, md, O_EXCL);
        if (ret)
                GOTO(err_ret, ret);

        return 0;
err_ret:
        return ret;
}

static int __inode_remove(const volid_t *volid, const fileid_t *fileid, md_proto_t *md)
{
        int ret;

        if (md) {
                ret = __inode_getattr(volid, fileid, md);
                if (ret)
                        GOTO(err_ret, ret);
        }

        ret = __inode_del(volid, fileid);
        if (ret)
                GOTO(err_ret, ret);

        return 0;
err_ret:
        return ret;
}

inodeop_t __inodeop__ = {
        .create = __inode_create,
        .getattr = __inode_getattr,
        .setattr = __inode_setattr,
        .extend = __inode_extend,
        .getxattr = __inode_getxattr,
        .setxattr = __inode_setxattr,
        .listxattr = __inode_listxattr,
        .removexattr = __inode_removexattr,
        .childcount = __inode_childcount,
        //.init = __inode_init,
        .link = __inode_link,
        .unlink = __inode_unlink,
        .symlink = __inode_symlink,
        .readlink = __inode_readlink,
        .mkvol = __inode_mkvol,
        .remove = __inode_remove,
        .setlock = __inode_setlock,
        .getlock = __inode_getlock,
};
