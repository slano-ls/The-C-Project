

#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <time.h>

#define DBG_SUBSYS S_LIBYLIB

#include "configure.h"
#include "confy.h"

#include "sdfs_conf.h"
#include "dbg_proto.h"
#include "cJSON.h"

extern FILE *yyin;

int line = 1;

struct sanconf_t sanconf;
struct nfsconf_t nfsconf;
struct webconf_t webconf;
struct mdsconf_t mdsconf;
struct cdsconf_t cdsconf;
struct gloconf_t gloconf;
struct logconf_t logconf;
struct yftp_conf_t yftp_conf;
netconf_t netconf;

extern uint32_t ylib_dbg;
extern uint32_t ylib_sub;

/*注意随机的种子是 time(NULL)*/
void _rand_str(char *dest, size_t length) {
        char charset[] = "0123456789"
                "abcdefghijklmnopqrstuvwxyz"
                "ABCDEFGHIJKLMNOPQRSTUVWXYZ";

        srand((unsigned)time(NULL));
        while (length-- > 0) {
                size_t index = (double) rand() / RAND_MAX * (sizeof charset - 1);
                *dest++ = charset[index];
        }
        *dest = '\0';
}

int get_nfs_export(const char *path, const char *ip, const char *permision)
{
        struct nfsconf_export_t _export;

#if 0
        DBUG("add path:%s, ip:%s, permision:%s\n",
                        path,
                        ip,
                        permision);
#endif

        strncpy(_export.path, path, strlen(path) + 1);

        strncpy(_export.ip, ip, strlen(ip) + 1);

        strncpy(_export.permision, permision, strlen(permision) + 1);

        memcpy(&nfsconf.nfs_export[nfsconf.export_size], &_export,
                        sizeof(struct nfsconf_export_t));

        nfsconf.export_size++;

        return 0;
}

int get_networks(const char *_ip)
{
        char addrs[MAX_NAME_LEN], *s;
        uint32_t mask, _mask = 0, i;

        strcpy(addrs, _ip);

        s = strchr(addrs, '/');

        if (s == NULL) {
                exit(2);
        }

        *s = '\0';

        mask = atoi(s + 1);

        if (strlen(_ip) >= MAX_NAME_LEN) {
                exit(2);
        }

        if (netconf.count >= MAX_NET_COUNT) {
                exit(2);
        }

        netconf.network[netconf.count].network = inet_addr(addrs);
        netconf.network[netconf.count].mask = 0;

        for (i = 0; i < mask; i++) {
                _mask |= 1 << (31 - i);
        }

        netconf.network[netconf.count].mask = htonl(_mask);
        netconf.count++;

        return 0;
}

void yyerror(const char *str)
{
        printf("config: parse %s error at line %d, %s\n",
                        YFS_CONFIGURE_FILE, line, str);

        exit(1);
}

int yywrap() {
        return 1;
}

int conf_init(const char *conf_path)
{
        int ret;

        /* default */
        mdsconf.disk_keep = (100 * 1024 * 1024 * 1024LL); /*100G*/
        nfsconf.rsize = 1048576;
        nfsconf.wsize = 1048576;
        nfsconf.nfs_port = NFS_SERVICE_DEF;
        nfsconf.nlm_port = NLM_SERVICE_DEF;
        memset(sanconf.iqn, 0x0, MAXSIZE);
        sanconf.lun_blk_shift = 9;
        gloconf.write_back = 1;
        gloconf.coredump = 1;
        gloconf.backtrace = 0;
        gloconf.testing = 0;
        gloconf.rpc_timeout = 10;
        gloconf.hb_timeout = 5;
        gloconf.hb_retry = 3;
        strcpy(gloconf.nfs_srv, "native");
        gloconf.lease_timeout = 20;
        mdsconf.redis_sharding = 3;
        mdsconf.redis_replica = 2;
        mdsconf.redis_thread = 0;
        mdsconf.ac_timeout = ATTR_QUEUE_TMO * 2;
        //mdsconf.ac_timeout = 0;
        mdsconf.redis_baseport = REDIS_BASEPORT;

        snprintf(gloconf.workdir, MAX_PATH_LEN, "%s/data", SDFS_HOME);

        gloconf.rdma = 0;
        sanconf.tcp_discovery = 0;
        
        cdsconf.disk_timeout = 60;
        cdsconf.unlink_async = 1;
        cdsconf.prealloc_max = 64 * 4;
        cdsconf.io_sync = 1;
        cdsconf.aio_thread = 0;
        cdsconf.queue_depth = 128;
        cdsconf.cds_polling = 0;
        gloconf.network = 0;
        gloconf.solomode = 0;
        gloconf.memcache_count = 1024;
        gloconf.memcache_seg = 1024 * 1024 * 2;
        gloconf.mask = 0;
        gloconf.maxcore = 1;
        memset(gloconf.cluster_name, 0x0, MAXSIZE);
        strcpy(gloconf.cluster_name, "uss");
        strcpy(gloconf.uuid, "f7f67a9bf59e4f8096ff9222a12fa3c0");//fake uuid
        mdsconf.chknew_hardend = 1;
        mdsconf.main_loop_threads = 6;
        mdsconf.size_on_md = 0;

        gloconf.performance_analysis = 0;
        gloconf.cache_size = (128 * 1024 * 1024LL);
        gloconf.net_crc = 0;
        gloconf.check_mountpoint = 1;
        gloconf.check_license = 1;
        gloconf.check_version = 0;
        gloconf.io_dump = 1;
        gloconf.restart = 1;
        gloconf.valgrind = 0;
        strcpy(gloconf.master_vip, "\0");
        gloconf.polling_core = 8;
        gloconf.polling_timeout = 0; //秒
        gloconf.aio_core = 0;
        gloconf.wmem_max = SO_XMITBUF;
        gloconf.rmem_max = SO_XMITBUF;
        netconf.count = 0;
        gloconf.sdevents_threads = 4;
        gloconf.jobdock_size = 8192;

        gloconf.chunk_entry_max = 1024*102; //默认40M

        gloconf.disk_mt = 0; //默认 不开启
        gloconf.disk_mt_ssd = 128; //mt 线程数
        gloconf.disk_mt_hdd = 2;

        gloconf.disk_worker = 1;

        gloconf.hb = 2; //默认2秒
        gloconf.main_loop_threads = 4;
        gloconf.schedule_physical_package_id = -1;
        gloconf.max_lvm = 1024*8; //默认8K，最大64K

        yyin = fopen(conf_path, "r");
        if (yyin == NULL) {
                ret = errno;
                printf("open %s fail, ret %u\n", conf_path, ret);
                goto err_ret;
        }

        extern int yyparse(void);

        yyparse();

        ylib_dbg = ~0;
        ylib_sub = 0;
        ylib_sub |= (logconf.log_ylib == 1) ? S_LIBYLIB : 0;
        ylib_sub |= (logconf.log_yliblock == 1) ? S_LIBYLIBLOCK : 0;
        ylib_sub |= (logconf.log_ylibmem == 1) ? S_LIBYLIBMEM : 0;
        ylib_sub |= (logconf.log_ylibskiplist == 1) ? S_LIBYLIBSKIPLIST : 0;
        ylib_sub |= (logconf.log_ylibnls == 1) ? S_LIBYLIBNLS : 0;
        ylib_sub |= (logconf.log_ysock == 1) ? S_YSOCK : 0;
        ylib_sub |= (logconf.log_ynet == 1) ? S_LIBYNET : 0;
        ylib_sub |= (logconf.log_yrpc == 1) ? S_YRPC : 0;
        ylib_sub |= (logconf.log_yfscdc == 1) ? S_YFSCDC : 0;
        ylib_sub |= (logconf.log_yfsmdc == 1) ? S_YFSMDC : 0;
        ylib_sub |= (logconf.log_fsmachine == 1) ? S_FSMACHINE : 0;
        ylib_sub |= (logconf.log_yfslib == 1) ? S_YFSLIB : 0;
        ylib_sub |= (logconf.log_yiscsi == 1) ? S_YISCSI : 0;
        ylib_sub |= (logconf.log_ynfs == 1) ? S_YNFS : 0;
        ylib_sub |= (logconf.log_yfsmds == 1) ? S_YFSMDS : 0;
        ylib_sub |= (logconf.log_cdsmachine == 1) ? S_CDSMACHINE : 0;
        ylib_sub |= (logconf.log_yfscds == 1) ? S_YFSCDS : 0;
        ylib_sub |= (logconf.log_yfscds_robot == 1) ? S_YFSCDS_ROBOT : 0;
        ylib_sub |= (logconf.log_proxy == 1) ? S_PROXY : 0;
        ylib_sub |= (logconf.log_yftp == 1) ? S_YFTP: 0;
        ylib_sub |= (logconf.log_yfuse == 1) ? S_YFUSE: 0;

        if (gloconf.rpc_timeout < 5) {
                printf("CONFIG: reqtimeout < 5, value: %d\n", gloconf.rpc_timeout);
                ret = EINVAL;
                goto err_ret;
        }

        return 0;
err_ret:
        return ret;
}

int nfs_config_init(const char *conf_path)
{
        int ret;

        FILE *fp = fopen(conf_path, "r");
        if (fp == NULL) {
                ret = errno;
                printf("open %s fail, ret %u\n", conf_path, ret);
                goto err_ret;
        }

        extern void yyrestart  (FILE * input_file );
        yyrestart(fp);
        extern int yyparse(void);

        yyparse();
        if(fp)
                fclose(fp);

        return 0;
err_ret:
        return ret;
}

int read_json(const char *filename, cJSON **root)
{
        int ret;
        FILE *f;
        long len;
        char *data;
        cJSON *json;

        f = fopen(filename, "rb");
        if(f == NULL) {
                ret = errno;
                goto err_ret;
        }

        fseek(f, 0, SEEK_END);
        len = ftell(f);
        fseek(f, 0, SEEK_SET);

        data = (char*)malloc(len+1);
        if(data == NULL) {
                ret = ENOMEM;
                goto err_close;
        }

        fread(data, 1, len, f);
        data[len] = '\0';
        json = cJSON_Parse(data);
        if (!json) {
                ret = EINVAL;
                free(data);
                goto err_close;
        } else {
                *root = json;
        }

        fclose(f);
        free(data);
        return 0;
err_close:
        fclose(f);
err_ret:
        return ret;
}

int get_string_by_key(cJSON *json, const char *key, char *value)
{
        int ret;
        cJSON *json_ret;

        if(json) {
                json_ret = cJSON_GetObjectItem(json, key);
                if(json_ret) {
                        strcpy(value, json_ret->valuestring);
                } else {
                        ret = EINVAL;
                        goto err_ret;
                }
        } else {
                ret = EINVAL;
                goto err_ret;
        }

        return 0;
err_ret:
        return ret;
}

int yftp_config_init(const char *conf_path)
{
        int ret, export_size;
        cJSON *root, *tmp_record;

        if(conf_path == NULL){
                ret = ENOENT;
                goto err_ret;
        }

        ret = access(conf_path, F_OK);
        if(ret){
                ret = ENOENT;
                goto err_ret;
        }

        ret = read_json(conf_path, &root);
        if(ret) {
                goto err_ret;
        } else {
                if(root != NULL){
                        int i;
                        export_size = cJSON_GetArraySize(root);
                        yftp_conf.export_size = export_size;

                        for(i=0; i<yftp_conf.export_size; ++i){
                                tmp_record = cJSON_GetArrayItem(root, i);
                                if(tmp_record != NULL){
                                        if(get_string_by_key(tmp_record, "user", yftp_conf.yftp_export[i].user)) {
                                                ret = EINVAL;
                                                goto err_free;
                                        }

                                        if(get_string_by_key(tmp_record, "password", yftp_conf.yftp_export[i].password)) {
                                                ret = EINVAL;
                                                goto err_free;
                                        }

                                        if(get_string_by_key(tmp_record, "path", yftp_conf.yftp_export[i].path)) {
                                                ret = EINVAL;
                                                goto err_free;
                                        }

                                        if(get_string_by_key(tmp_record, "permision", yftp_conf.yftp_export[i].permision)) {
                                                ret = EINVAL;
                                                goto err_free;
                                        }
                                } else {
                                        ret = EINVAL;
                                        goto err_free;
                                }
                        }

                        cJSON_Delete(root);
                } else{
                        ret = EINVAL;
                        goto err_ret;
                }
        }

        return 0;
err_free:
        cJSON_Delete(root);
err_ret:
        return ret;
}

int conf_destroy(void)
{
        fclose(yyin);

        return 0;
}

LLU str2val(const char *str)
{
        LLU val;

        val = atoll(str);

        while (*str) {
                switch (*str) {
                        case 'k':
                        case 'K':
                                val *= 1024;
                                break;
                        case 'm':
                        case 'M':
                                val *= 1024 * 1024;
                                break;
                        case 'g':
                        case 'G':
                                val *= 1024 * 1024 * 1024;
                                break;
                        default:
                                break;
                }

                str += 1;
        }

        return val;
}


int keyis(const char *src, const char *dst)
{
        int dstlen;

        dstlen = strlen(dst);

        if (strncmp(src, dst, dstlen) == 0)
                return 1;
        else
                return 0;
}

int set_value(const char* key, const char* value, int type)
{
        int vallen;
        LLU _value = 0;

        vallen = strlen(value);

#if 0
        int keylen;
        keylen = strlen(key);
        printf("%s : %s\n", key, value);
#endif

        if (type == V_NUMBER)
                _value = str2val(value);
        else if (type == V_STATE) {
                if (strncasecmp("on", value, vallen) == 0)
                        _value = 1;
                else
                        _value = 0;
        }

        /**
         * ynfs configure
         */
        if (keyis("use_export", key))
                nfsconf.use_export = _value;
        else if (keyis("rsize", key))
                nfsconf.rsize = _value;
        else if (keyis("wsize", key))
                nfsconf.wsize = _value;
        else if (keyis("nlm_port", key))
                nfsconf.nlm_port = _value;
        else if (keyis("nfs_port", key))
                nfsconf.nfs_port = _value;

        /**
         * yiscsi configure
         */
        else if (keyis("iqn", key))
                strncpy(sanconf.iqn, value, MAXSIZE);
        else if (keyis("lun_blk_shift", key)) {
                if (_value < 9 || _value > 12) {
                        printf("lun_blk_shift must between [9, 12]\n");
                } else
                        sanconf.lun_blk_shift = _value;
        }

        /**
         * yweb configure
         */
        else if (keyis("webport", key))
                webconf.webport = _value;
        else if (keyis("use_ratelimit", key))
                webconf.use_ratelimit = _value;

        /**
         * mds configure
         */
        else if (keyis("disk_keep", key))
                mdsconf.disk_keep = _value;
        else if (keyis("object_hardend", key))
                mdsconf.chknew_hardend = _value;
        else if (keyis("solomode", key))
                gloconf.solomode = _value;
        else if (keyis("memcache_count", key))
                gloconf.memcache_count = _value;
        else if (keyis("memcache_seg", key))
                gloconf.memcache_seg = _value;
        else if (keyis("redis_sharding", key))
                mdsconf.redis_sharding = _value;
        else if (keyis("redis_replica", key))
                mdsconf.redis_replica = _value;
        else if (keyis("redis_thread", key))
                mdsconf.redis_thread = _value;
        else if (keyis("ac_timeout", key))
                mdsconf.ac_timeout = _value;
        else if (keyis("main_loop_threads ", key)) {
                mdsconf.main_loop_threads = _value;
        }

        /**
         * cds configure
         */
        else if (keyis("unlink_async", key))
                cdsconf.unlink_async = _value;
        else if (keyis("queue_depth", key))
                cdsconf.queue_depth = _value;
        else if (keyis("cache_size", key))
                gloconf.cache_size = _value;
        else if (keyis("prealloc_max", key))
                cdsconf.prealloc_max = _value;
        else if (keyis("io_sync", key))
                cdsconf.io_sync = _value;
        else if (keyis("aio_thread", key))
                cdsconf.aio_thread = _value;
        else if (keyis("cds_polling", key))
                cdsconf.cds_polling = _value;
        /**
         * global configure
         */
        else if (keyis("write_back", key))
                gloconf.write_back = _value;
        else if (keyis("performance_analysis", key))
                gloconf.performance_analysis = _value;
        else if (keyis("rpc_timeout", key)) {
                gloconf.rpc_timeout = _value;
        } else if (keyis("backtrace", key)) {
                gloconf.backtrace = _value;
        } else if (keyis("testing", key)) {
                gloconf.testing = _value;
        } else if (keyis("coredump", key)) {
                gloconf.coredump = _value;
        } else if (keyis("chunk_rep", key)) {
                gloconf.chunk_rep = _value;

                if (gloconf.chunk_rep > YFS_CHK_REP_MAX)
                        gloconf.chunk_rep = YFS_CHK_REP_MAX;
        } else if (keyis("home", key))
                strncpy(gloconf.workdir, value, MAXSIZE);
        else if (keyis("check_mountpoint", key))
                gloconf.check_mountpoint = _value;
        else if (keyis("check_license", key))
                gloconf.check_license = _value;
        else if (keyis("check_version", key))
                gloconf.check_version = _value;
        else if (keyis("io_dump", key))
                gloconf.io_dump = _value;
        else if (keyis("valgrind", key))
                gloconf.valgrind = _value;
        else if (keyis("restart", key))
                gloconf.restart = _value;
        else if (keyis("master_vip", key))
                strncpy(gloconf.master_vip, value, MAXSIZE);
        else if (keyis("maxcore", key))
                gloconf.maxcore = _value;
        else if (keyis("network", key))
                gloconf.network = ntohl(inet_addr(value));
        else if (keyis("mask", key))
                gloconf.mask = ntohl(inet_addr(value));
        else if (keyis("cluster_name", key))
                strncpy(gloconf.cluster_name, value, MAXSIZE);
        else if (keyis("nfs_srv", key))
                strncpy(gloconf.nfs_srv, value, MAXSIZE);
        else if (keyis("net_crc", key))
                gloconf.net_crc  = _value;
        else if (keyis("polling_core", key))
                gloconf.polling_core = _value;
        else if (keyis("polling_timeout", key))
                gloconf.polling_timeout = _value * 1000;
        else if (keyis("aio_core", key))
                gloconf.aio_core = _value;
        else if (keyis("max_lvm", key))
                gloconf.max_lvm = _value > 65536 ? 65536 : _value;

        /**
         * log configure
         */
        else if (keyis("log_ylib", key))
                logconf.log_ylib = _value;
        else if (keyis("log_yliblock", key))
                logconf.log_yliblock = _value;
        else if (keyis("log_ylibmem", key))
                logconf.log_ylibmem = _value;
        else if (keyis("log_ylibskiplist", key))
                logconf.log_ylibskiplist = _value;
        else if (keyis("log_ylibnls", key))
                logconf.log_ylibnls = _value;
        else if (keyis("log_ysock", key))
                logconf.log_ysock = _value;
        else if (keyis("log_ynet", key))
                logconf.log_ynet = _value;
        else if (keyis("log_yrpc", key))
                logconf.log_yrpc = _value;
        else if (keyis("log_yfscdc", key))
                logconf.log_yfscdc = _value;
        else if (keyis("log_yfsmdc", key))
                logconf.log_yfsmdc = _value;
        else if (keyis("log_fsmachine", key))
                logconf.log_fsmachine = _value;
        else if (keyis("log_yfslib", key))
                logconf.log_yfslib = _value;
        else if (keyis("log_yiscsi", key))
                logconf.log_yiscsi = _value;
        else if (keyis("log_ynfs", key))
                logconf.log_ynfs = _value;
        else if (keyis("log_yfsmds", key))
                logconf.log_yfsmds = _value;
        else if (keyis("log_cdsmachine", key))
                logconf.log_cdsmachine = _value;
        else if (keyis("log_yfscds", key))
                logconf.log_yfscds = _value;
        else if (keyis("log_yfscds_robot", key))
                logconf.log_yfscds_robot = _value;
        else if (keyis("log_proxy", key))
                logconf.log_proxy = _value;
        else if (keyis("log_yftp", key))
                logconf.log_yftp = _value;
        else if (keyis("log_yfuse", key))
                logconf.log_yfuse = _value;
        else if (keyis("sdevents_threads", key))
                gloconf.sdevents_threads = (_value > SDEVENTS_THREADS_MAX ? SDEVENTS_THREADS_MAX : _value);
        else if (keyis("jobdock_size", key))
                gloconf.jobdock_size = _value;
        else if (keyis("chunk_entry_max", key))
                gloconf.chunk_entry_max = _value;
        else if (keyis("disk_mt", key))
                gloconf.disk_mt = _value;
        else if (keyis("disk_mt_hdd", key))
                gloconf.disk_mt_hdd = _value;
        else if (keyis("disk_mt_ssd", key))
                gloconf.disk_mt_ssd = _value;
        else if (keyis("disk_worker", key))
                gloconf.disk_worker = (_value > DISK_WORKER_MAX? DISK_WORKER_MAX: _value);
        else if (keyis("hb", key))
                gloconf.hb = _value;
        else if (keyis("main_loop_threads", key))
                gloconf.main_loop_threads = _value;
        else if (keyis("schedule_physical_package_id", key))
                gloconf.schedule_physical_package_id = _value;
        /**
         * error.
         */
        else {
                printf("%s:%s no such key_value\n", key, value);
        }

        return 0;
}
