/* SPDX-License-Identifier: GPL-3.0-or-later
 * Independent UDF reader: ECMA-167 third edition, physical partition
 * and descriptor layouts, OSTA compressed Unicode. Field layouts
 * cross-checked against Linux v6.12 fs/udf/ecma_167.h and the cdrkit
 * UDF writer. No kernel/VFS calls; canonical Common V2 host I/O only.
 * This bounded initial profile supports a single type-1 physical
 * partition, ordinary FE/EFE, inline/short/long allocation descriptors.
 * Virtual, sparable and metadata partition mappings fail closed.
 */
#include <infiltratr/filesystem_module_v2.h>
#include <stdint.h>
#include <stddef.h>
#include <stdatomic.h>
enum { MAX_BLOCK=4096,MAX_NAME=510,SLOTS=8,MAX_VDS=128,
       MAX_RUNS=4096,MAX_AED=128,MAX_FIDS=262144,MAX_FID=4096 };
typedef struct {
    IfsFsHostRuntimeV2 host;
    uint32_t block,blocks,start,length,free_blocks,root,variant;
    uint16_t revision,partition;
    char label[256];uint32_t label_bytes;
    uintptr_t token;
} Volume;
typedef struct {uint32_t id,type,ad,offset,length;uint64_t bytes;uint8_t raw[MAX_BLOCK];} Node;
typedef struct {Node node;uint32_t length;char name[MAX_NAME+1];} Entry;
typedef struct {uint64_t logical;uint32_t position,length;unsigned hole;} Run;
typedef struct {Run run[MAX_RUNS];uint32_t aed[MAX_AED],count,chains;uint64_t bytes;} Map;
typedef struct {uint32_t start,length,access,fsd,integrity,integrity_bytes,block;uint16_t part,revision;uint8_t label[128];} Layout;
static Volume slots[SLOTS];static uintptr_t generation;
static atomic_flag gate=ATOMIC_FLAG_INIT;
static int enter(void){return !atomic_flag_test_and_set_explicit(&gate,memory_order_acquire);}
static int leave(int rc){atomic_flag_clear_explicit(&gate,memory_order_release);return rc;}
static void copy(void *d,const void *s,size_t n){uint8_t *o=d;const uint8_t *p=s;while(n--)*o++=*p++;}
static void zero(void *d,size_t n){uint8_t *p=d;while(n--)*p++=0;}
static int same(const void *a,const void *b,size_t n)
{const uint8_t *p=a,*q=b;while(n--)if(*p++!=*q++)return 0;return 1;}
static uint32_t le16(const uint8_t *p){return p[0]|(uint32_t)p[1]<<8;}
static uint32_t le32(const uint8_t *p){return le16(p)|le16(p+2)<<16;}
static uint64_t le64(const uint8_t *p){return le32(p)|(uint64_t)le32(p+4)<<32;}
static uint32_t crc16(const uint8_t *p,uint32_t n)
{uint32_t c=0;while(n--){c^=(uint32_t)*p++<<8;for(unsigned i=0;i<8;++i)c=((c<<1)^((c&0x8000)?0x1021:0))&0xffff;}return c;}
static int cancel(const Volume *v){return v->host.cancelled&&v->host.cancelled(v->host.context);}
static int rd(const Volume *v,uint64_t off,void *p,uint32_t n)
{return ifs_fs_host_read_bytes_v2(&v->host,off,p,n);}
static int span(uint32_t blocks,uint32_t pos,uint64_t bytes,uint32_t b)
{return bytes&&pos<blocks&&bytes<=(uint64_t)(blocks-pos)*b;}
static int absblock(const Volume *v,uint32_t pos,uint8_t *raw)
{return span(v->blocks,pos,v->block,v->block)?rd(v,(uint64_t)pos*v->block,raw,v->block):IFS_FS_CORRUPT;}
static int partblock(const Volume *v,uint32_t pos,uint8_t *raw)
{return span(v->length,pos,v->block,v->block)?absblock(v,v->start+pos,raw):IFS_FS_CORRUPT;}
/* Descriptor checksum, CRC and location are independent checks. Require
 * the CRC-covered body to include every field the parser consumes. */
static int tag(const uint8_t *p,uint32_t available,uint32_t id,uint32_t loc,uint32_t required)
{
    if(available<16||required>available||le16(p)!=id||
       (le16(p+2)!=2&&le16(p+2)!=3)||p[5]||le32(p+12)!=loc)return 0;
    uint32_t n=le16(p+10);if(n>available-16||16+n<required)return 0;
    unsigned sum=0;for(unsigned i=0;i<16;++i)if(i!=4)sum+=p[i];
    return (sum&255)==p[4]&&crc16(p+16,n)==le16(p+8);
}
static int charset(const uint8_t *p)
{return p[0]==0&&same(p+1,"OSTA Compressed Unicode",23);}
static int domain(const uint8_t *p,uint16_t *rev)
{
    if(!same(p+1,"*OSTA UDF Compliant",19))return 0;
    unsigned r=le16(p+24);if(r!=0x102&&r!=0x150&&r!=0x200&&r!=0x201)return 0;
    *rev=(uint16_t)r;return 1;
}
static int unicode(const uint8_t *p,uint32_t n,char *out,uint32_t cap,uint32_t *len,int filename)
{
    *len=0;if(!n){if(filename)return IFS_FS_CORRUPT;out[0]=0;return IFS_FS_OK;}
    unsigned code=*p++;--n;
    if(code!=8&&code!=16)return IFS_FS_UNSUPPORTED;
    if(code==16&&n%2)return IFS_FS_CORRUPT;
    while(n){
        uint32_t c=*p++;--n;
        if(code==16){c=c<<8|*p++;--n;
            if(c>=0xd800&&c<=0xdbff){
                if(n<2)return IFS_FS_CORRUPT;
                uint32_t d=(uint32_t)p[0]<<8|p[1];
                if(d<0xdc00||d>0xdfff)return IFS_FS_CORRUPT;
                p+=2;n-=2;c=0x10000+((c-0xd800)<<10)+d-0xdc00;
            }else if(c>=0xdc00&&c<=0xdfff)return IFS_FS_CORRUPT;
        }
        if(!c||(filename&&(c=='/'||c==92)))return IFS_FS_CORRUPT;
        unsigned size=c<0x80?1:c<0x800?2:c<0x10000?3:4;
        if(*len+size>=cap)return IFS_FS_NO_SPACE;
        char *d=out+*len;*len+=size;
        if(size==1)d[0]=(char)c;
        else{
            for(unsigned j=size-1;j;--j){d[j]=(char)(0x80|(c&63));c>>=6;}
            d[0]=(char)((size==2?0xc0:size==3?0xe0:0xf0)|c);
        }
    }
    if(filename&&!*len)return IFS_FS_CORRUPT;
    out[*len]=0;return IFS_FS_OK;
}
static int long_addr(const Volume *v,const uint8_t *p,uint32_t *pos,uint32_t *bytes)
{
    *bytes=le32(p);*pos=le32(p+4);
    if(*bytes>>30||le16(p+8)||!span(v->length,*pos,*bytes,v->block))return IFS_FS_CORRUPT;
    return IFS_FS_OK;
}
static int scan_vds(const Volume *v,uint32_t pos,uint32_t bytes,Layout *o)
{
    if(bytes%v->block||bytes/v->block>MAX_VDS||!span(v->blocks,pos,bytes,v->block))return IFS_FS_CORRUPT;
    uint8_t raw[MAX_BLOCK],pd[MAX_BLOCK],lv[MAX_BLOCK];uint32_t ps=0,ls=0;int havep=0,havel=0,end=0;
    for(uint32_t i=0;i<bytes/v->block;++i){
        if(cancel(v))return IFS_FS_BUSY;
        int rc=absblock(v,pos+i,raw);if(rc)return rc;uint32_t id=le16(raw);
        if(!id)continue;
        if(!tag(raw,v->block,id,pos+i,16))return IFS_FS_CORRUPT;
        if(id==8){end=1;break;}
        if(id==3)return IFS_FS_UNSUPPORTED;
        if(id==5){
            if(!tag(raw,v->block,5,pos+i,512))return IFS_FS_CORRUPT;
            uint32_t seq=le32(raw+16);
            if(havep&&le16(pd+22)!=le16(raw+22))return IFS_FS_UNSUPPORTED;
            if(havep&&seq==ps&&!same(pd+16,raw+16,496))return IFS_FS_CORRUPT;
            if(!havep||seq>ps){copy(pd,raw,v->block);ps=seq;havep=1;}
        }else if(id==6){
            if(!tag(raw,v->block,6,pos+i,446))return IFS_FS_CORRUPT;
            uint32_t seq=le32(raw+16);
            if(havel&&seq==ls&&!same(lv+16,raw+16,430))return IFS_FS_CORRUPT;
            if(!havel||seq>ls){copy(lv,raw,v->block);ls=seq;havel=1;}
        }
    }
    if(!end||!havep||!havel)return IFS_FS_CORRUPT;
    Layout x={0};x.part=(uint16_t)le16(pd+22);x.block=le32(lv+212);
    if(!(le16(pd+20)&1)||(!same(pd+25,"+NSR02",6)&&!same(pd+25,"+NSR03",6))||
       x.block!=v->block||!charset(lv+20)||!domain(lv+216,&x.revision)||
       le32(lv+264)!=6||le32(lv+268)!=1||lv[440]!=1||lv[441]!=6||
       le16(lv+442)!=1||le16(lv+444)!=x.part)return IFS_FS_UNSUPPORTED;
    x.start=le32(pd+188);x.length=le32(pd+192);x.access=le32(pd+184);
    if(x.access!=1&&x.access!=3&&x.access!=4)return IFS_FS_UNSUPPORTED;
    if(!span(v->blocks,x.start,(uint64_t)x.length*v->block,v->block))return IFS_FS_CORRUPT;
    Volume temp=*v;temp.start=x.start;temp.length=x.length;uint32_t fsdlen;
    int rc=long_addr(&temp,lv+248,&x.fsd,&fsdlen);if(rc)return rc;
    if(fsdlen<512)return IFS_FS_CORRUPT;
    x.integrity_bytes=le32(lv+432);x.integrity=le32(lv+436);
    copy(x.label,lv+84,sizeof x.label);*o=x;return IFS_FS_OK;
}
static int same_layout(const Layout *a,const Layout *b)
{
    return a->start==b->start&&a->length==b->length&&a->access==b->access&&a->fsd==b->fsd&&
           a->integrity==b->integrity&&a->integrity_bytes==b->integrity_bytes&&a->block==b->block&&
           a->part==b->part&&a->revision==b->revision&&same(a->label,b->label,sizeof a->label);
}
static int integrity(Volume *v,const Layout *x)
{
    if(!x->integrity_bytes||x->integrity_bytes%v->block||x->integrity_bytes/v->block>MAX_VDS||
       !span(v->blocks,x->integrity,x->integrity_bytes,v->block))return IFS_FS_CORRUPT;
    uint8_t raw[MAX_BLOCK];int have=0;
    for(uint32_t i=0;i<x->integrity_bytes/v->block;++i){
        int rc=absblock(v,x->integrity+i,raw);if(rc)return rc;
        uint32_t id=le16(raw);if(!id)continue;
        if(!tag(raw,v->block,id,x->integrity+i,16))return IFS_FS_CORRUPT;
        if(id==8)break;
        if(id!=9||!tag(raw,v->block,9,x->integrity+i,88))return IFS_FS_CORRUPT;
        if(le32(raw+32)||le32(raw+72)!=1)return IFS_FS_UNSUPPORTED;
        uint32_t free=le32(raw+80),size=le32(raw+84);
        if(size!=v->length||free>size)return IFS_FS_UNSUPPORTED;
        if(le32(raw+28)!=1){have=0;continue;}
        v->free_blocks=free;have=1;
    }
    return have?IFS_FS_OK:IFS_FS_UNSUPPORTED;
}
static int inode(const Volume *v,uint32_t id,Node *out)
{
    Node n={.id=id};int rc=partblock(v,id,n.raw);if(rc)return rc;
    uint8_t *p=n.raw;uint32_t t=le16(p),base=t==261?176:t==266?216:0;
    if(!base||!tag(p,v->block,t,id,base)||le16(p+20)!=4||!le16(p+48))return IFS_FS_CORRUPT;
    uint32_t flags=le16(p+34);n.ad=flags&7;
    if(n.ad==2||n.ad>3||(flags&0x3800))return IFS_FS_UNSUPPORTED;
    if(p[27]==4)n.type=IFS_FS_ENTRY_DIRECTORY;
    else if(p[27]==5)n.type=IFS_FS_ENTRY_FILE;
    else if(p[27]==12)n.type=IFS_FS_ENTRY_LINK;
    else return IFS_FS_UNSUPPORTED;
    n.bytes=le64(p+56);uint32_t ea=le32(p+base-8),ad=le32(p+base-4);
    if(ea>v->block-base||ad>v->block-base-ea||
       !tag(p,v->block,t,id,base+ea+ad))return IFS_FS_CORRUPT;
    if(t==266&&(le32(p+152)||le64(p+64)<n.bytes))return IFS_FS_UNSUPPORTED;
    n.offset=base+ea;n.length=ad;
    if(n.ad==3&&n.bytes!=ad)return IFS_FS_CORRUPT;
    if(n.ad!=3&&ad%(n.ad==0?8:16))return IFS_FS_CORRUPT;
    *out=n;return IFS_FS_OK;
}
static int mapped(const Volume *v,const Node *n,Map *m)
{
    m->count=m->chains=0;m->bytes=0;
    if(n->ad==3){m->bytes=n->bytes;return IFS_FS_OK;}
    const uint8_t *p=n->raw+n->offset;uint32_t len=n->length,step=n->ad==0?8:16;
    uint8_t raw[MAX_BLOCK];uint32_t previous=n->id;
    for(;;){
        if(cancel(v))return IFS_FS_BUSY;
        int next=0;
        for(uint32_t i=0;i<len;i+=step){
            uint32_t field=le32(p+i),kind=field>>30,bytes=field&0x3fffffff,pos=le32(p+i+4);
            if(n->ad==1&&le16(p+i+8))return IFS_FS_UNSUPPORTED;
            if(!bytes){if(i+step!=len)return IFS_FS_CORRUPT;break;}
            if(kind==3){
                if(i+step!=len||bytes>v->block||bytes<24||pos==n->id||m->chains==MAX_AED)return IFS_FS_CORRUPT;
                for(uint32_t j=0;j<m->chains;++j)if(m->aed[j]==pos)return IFS_FS_CORRUPT;
                m->aed[m->chains++]=pos;
                int rc=partblock(v,pos,raw);if(rc)return rc;
                uint32_t more=le32(raw+20),prior=le32(raw+16);
                if(more>v->block-24||more%step||!tag(raw,v->block,258,pos,24+more)||
                   (prior&&prior!=previous))return IFS_FS_CORRUPT;
                p=raw+24;len=more;previous=pos;next=1;break;
            }
            if(m->count==MAX_RUNS||bytes>UINT64_MAX-m->bytes)return IFS_FS_UNSUPPORTED;
            if(kind&&n->type!=IFS_FS_ENTRY_FILE)return IFS_FS_CORRUPT;
            if(kind!=2&&!span(v->length,pos,bytes,v->block))return IFS_FS_CORRUPT;
            if(m->bytes%v->block)return IFS_FS_CORRUPT;
            if(kind!=2){
                uint32_t blocks=(bytes+v->block-1)/v->block;
                if(pos<=n->id&&n->id-pos<blocks)return IFS_FS_CORRUPT;
                for(uint32_t j=0;j<m->count;++j)if(m->run[j].hole!=2){
                    uint32_t q=m->run[j].position,count=(m->run[j].length+v->block-1)/v->block;
                    if(pos<(uint64_t)q+count&&q<(uint64_t)pos+blocks)return IFS_FS_CORRUPT;
                }
            }
            m->run[m->count++]=(Run){m->bytes,pos,bytes,kind};m->bytes+=bytes;
        }
        if(!next)break;
    }
    for(uint32_t i=0;i<m->count;++i)if(m->run[i].hole!=2)
        for(uint32_t j=0;j<m->chains;++j)
            if(m->aed[j]>=m->run[i].position&&m->aed[j]-m->run[i].position<(m->run[i].length+v->block-1)/v->block)return IFS_FS_CORRUPT;
    return m->bytes>=n->bytes&&m->bytes-n->bytes<v->block?IFS_FS_OK:IFS_FS_CORRUPT;
}
static int map_read(const Volume *v,const Node *n,const Map *m,uint64_t at,void *out,uint32_t bytes,uint32_t *location)
{
    if((!out&&bytes)||at>n->bytes||bytes>n->bytes-at)return IFS_FS_INVALID;
    if(n->ad==3){if(location)*location=n->id;copy(out,n->raw+n->offset+(size_t)at,bytes);return IFS_FS_OK;}
    uint8_t *dst=out;int initial=1;
    for(uint32_t i=0;i<m->count&&bytes;++i){
        const Run *r=&m->run[i];if(at>=r->logical+r->length)continue;
        if(at<r->logical)return IFS_FS_CORRUPT;
        uint32_t offset=(uint32_t)(at-r->logical),chunk=r->length-offset;
        if(chunk>bytes)chunk=bytes;
        if(initial&&location){if(r->hole)return IFS_FS_CORRUPT;*location=r->position+offset/v->block;}
        initial=0;if(cancel(v))return IFS_FS_BUSY;
        if(r->hole)zero(dst,chunk);
        else{int rc=rd(v,(uint64_t)(v->start+r->position)*v->block+offset,dst,chunk);if(rc)return rc;}
        dst+=chunk;at+=chunk;bytes-=chunk;
    }
    return bytes?IFS_FS_CORRUPT:IFS_FS_OK;
}
static int file_read(const Volume *v,const Node *n,uint64_t at,void *out,uint32_t bytes)
{
    Map *m=v->host.allocate(v->host.context,sizeof(*m));if(!m)return IFS_FS_NO_MEMORY;
    int rc=mapped(v,n,m);if(!rc)rc=map_read(v,n,m,at,out,bytes,NULL);
    v->host.release(v->host.context,m);return rc;
}
static int child(const Volume *v,const Node *dir,const char *name,uint32_t length,uint32_t index,Entry *out)
{
    if(dir->type!=IFS_FS_ENTRY_DIRECTORY)return IFS_FS_NOT_FOUND;
    Map *m=v->host.allocate(v->host.context,sizeof(*m));if(!m)return IFS_FS_NO_MEMORY;
    int rc=mapped(v,dir,m),found=0;uint32_t visited=0;Entry result;
    if(rc)goto done;
    for(uint64_t at=0;at<dir->bytes;){
        uint8_t raw[MAX_FID];uint32_t loc;
        if(++visited>MAX_FIDS){rc=IFS_FS_UNSUPPORTED;goto done;}
        if(dir->bytes-at<38){rc=IFS_FS_CORRUPT;goto done;}
        rc=map_read(v,dir,m,at,raw,38,&loc);if(rc)goto done;
        uint32_t chars=raw[18],n=raw[19],imp=le16(raw+36),size=(38+n+imp+3)&~3u;
        if(size>sizeof raw||size>dir->bytes-at){rc=IFS_FS_CORRUPT;goto done;}
        rc=map_read(v,dir,m,at,raw,size,NULL);if(rc)goto done;
        if(!tag(raw,size,257,loc,size)||le16(raw+16)!=1){rc=IFS_FS_CORRUPT;goto done;}
        at+=size;
        if(chars&4)continue;
        uint32_t id,bytes;rc=long_addr(v,raw+20,&id,&bytes);if(rc)goto done;
        if(bytes<v->block){rc=IFS_FS_CORRUPT;goto done;}
        if(chars&8){if(n){rc=IFS_FS_CORRUPT;goto done;}continue;}
        if(chars&16){rc=IFS_FS_UNSUPPORTED;goto done;}
        Entry e;rc=unicode(raw+38+imp,n,e.name,sizeof e.name,&e.length,1);if(rc)goto done;
        if((e.length==1&&e.name[0]=='.')||(e.length==2&&same(e.name,"..",2))){rc=IFS_FS_CORRUPT;goto done;}
        if(name?(e.length==length&&same(name,e.name,length)):index==0){
            if(found&&name){rc=IFS_FS_CORRUPT;goto done;}
            rc=inode(v,id,&e.node);if(rc)goto done;
            if(!!(chars&2)!=(e.node.type==IFS_FS_ENTRY_DIRECTORY)||
               (e.node.type==IFS_FS_ENTRY_DIRECTORY&&id==dir->id)){rc=IFS_FS_CORRUPT;goto done;}
            result=e;found=1;if(!name)break;
        }
        if(!name&&index)--index;
    }
    if(found){*out=result;rc=IFS_FS_OK;}else rc=name?IFS_FS_NOT_FOUND:IFS_FS_END;
done:
    v->host.release(v->host.context,m);return rc;
}
static int candidate(const Volume *host,uint32_t block,Volume *out)
{
    Volume v=*host;v.block=block;
    if(v.host.volume_bytes/block>UINT32_MAX)return IFS_FS_UNSUPPORTED;
    v.blocks=(uint32_t)(v.host.volume_bytes/block);
    if(v.blocks<=256)return IFS_FS_NOT_FOUND;
    uint8_t raw[MAX_BLOCK];int rc=absblock(&v,256,raw);if(rc)return rc;
    if(le16(raw)!=2)return IFS_FS_NOT_FOUND;
    if(!tag(raw,block,2,256,512))return IFS_FS_CORRUPT;
    Layout a,b;
    rc=scan_vds(&v,le32(raw+20),le32(raw+16),&a);if(rc)return rc;
    rc=scan_vds(&v,le32(raw+28),le32(raw+24),&b);if(rc)return rc;
    if(!same_layout(&a,&b))return IFS_FS_CORRUPT;
    v.start=a.start;v.length=a.length;v.partition=a.part;v.revision=a.revision;v.variant=a.revision;
    uint32_t label_len=a.label[127];if(label_len>127)return IFS_FS_CORRUPT;
    rc=unicode(a.label,label_len,v.label,sizeof v.label,&v.label_bytes,0);if(rc)return rc;
    rc=integrity(&v,&a);if(rc)return rc;
    rc=partblock(&v,a.fsd,raw);if(rc)return rc;
    uint16_t rev;uint32_t len;
    if(!tag(raw,block,256,a.fsd,512)||!charset(raw+240)||!domain(raw+416,&rev)||rev!=v.revision||
       le32(raw+448)||le32(raw+464))return IFS_FS_UNSUPPORTED;
    rc=long_addr(&v,raw+400,&v.root,&len);if(rc)return rc;
    if(len<block)return IFS_FS_CORRUPT;
    Node root;rc=inode(&v,v.root,&root);if(rc)return rc;
    if(root.type!=IFS_FS_ENTRY_DIRECTORY)return IFS_FS_CORRUPT;
    Entry e;rc=child(&v,&root,NULL,0,UINT32_MAX,&e);if(rc!=IFS_FS_END)return rc?rc:IFS_FS_CORRUPT;
    *out=v;return IFS_FS_OK;
}
static int discover(const IfsFsHostRuntimeV2 *h,Volume *out)
{
    if(!ifs_fs_host_v2_valid(h))return IFS_FS_INVALID;
    Volume host={0};size_t bytes=h->structure_bytes;if(bytes>sizeof(host.host))bytes=sizeof(host.host);
    copy(&host.host,h,bytes);host.host.structure_bytes=(uint32_t)bytes;
    int have=0,err=IFS_FS_NOT_FOUND;Volume winner;
    for(uint32_t b=512;b<=4096;b*=2){
        Volume v;int rc=candidate(&host,b,&v);
        if(!rc){if(have)return IFS_FS_CORRUPT;winner=v;have=1;}
        else if(rc==IFS_FS_IO_ERROR||rc==IFS_FS_NO_MEMORY||rc==IFS_FS_BUSY)return rc;
        else if(rc!=IFS_FS_NOT_FOUND)err=rc;
    }
    if(!have)return err;
    *out=winner;return IFS_FS_OK;
}
static Volume *mounted(void *p)
{if(p)for(unsigned i=0;i<SLOTS;++i)if(slots[i].token==(uintptr_t)p)return &slots[i];return NULL;}
static void info(const Volume *v,IfsFsVolumeInfoV2 *o)
{*o=(IfsFsVolumeInfoV2){.structure_bytes=sizeof(*o),.variant=v->variant,.max_name_bytes=MAX_NAME,
.block_bytes=v->block,.total_bytes=(uint64_t)v->length*v->block,.free_bytes=(uint64_t)v->free_blocks*v->block};}
static int probe_impl(const IfsFsHostRuntimeV2 *h,uint32_t *variant)
{if(!variant)return IFS_FS_INVALID;Volume v;int rc=discover(h,&v);if(!rc)*variant=v.variant;return rc;}
static int mount_impl(const IfsFsHostRuntimeV2 *h,void **out,IfsFsVolumeInfoV2 *o)
{
    if(!out||!o||o->structure_bytes<sizeof(*o))return IFS_FS_INVALID;
    *out=NULL;if(!ifs_fs_host_v2_valid(h))return IFS_FS_INVALID;
    if(!(h->capabilities&IFS_FS_HOST_READ_ONLY))return IFS_FS_ACCESS;
    unsigned i=0;while(i<SLOTS&&slots[i].token)++i;
    if(i==SLOTS||generation==UINTPTR_MAX)return IFS_FS_BUSY;
    int rc=discover(h,&slots[i]);if(rc)return rc;
    slots[i].token=++generation;*out=(void *)slots[i].token;info(&slots[i],o);return IFS_FS_OK;
}
static int unmount_impl(void *p)
{Volume *v=mounted(p);if(!v)return IFS_FS_INVALID;zero(v,sizeof(*v));return IFS_FS_OK;}
static int resolve(const Volume *v,const char *path,Entry *out)
{
    if(!path||*path!='/')return IFS_FS_INVALID;
    Entry e={.length=1,.name="/"};int rc=inode(v,v->root,&e.node);if(rc)return rc;
    ++path;
    for(unsigned depth=0;*path;++depth){
        if(depth>=256)return IFS_FS_INVALID;
        uint32_t n=0;while(n<=MAX_NAME&&path[n]&&path[n]!='/')++n;
        if(!n||n>MAX_NAME||(n==1&&path[0]=='.')||(n==2&&same(path,"..",2)))return IFS_FS_INVALID;
        Node dir=e.node;rc=child(v,&dir,path,n,0,&e);if(rc)return rc;
        path+=n;if(*path=='/'){++path;if(!*path)return IFS_FS_INVALID;}
    }
    *out=e;return IFS_FS_OK;
}
static int emit(const Entry *e,IfsFsEntryV2 *o)
{
    if(!o||o->structure_bytes<sizeof(*o)||!o->name)return IFS_FS_INVALID;
    if(o->name_capacity<=e->length)return IFS_FS_NO_SPACE;
    char *name=o->name;uint32_t cap=o->name_capacity;
    *o=(IfsFsEntryV2){.structure_bytes=sizeof(*o),.type=e->node.type,.size_bytes=e->node.bytes,
      .name=name,.name_capacity=cap,.name_bytes=e->length};
    copy(name,e->name,e->length+1);return IFS_FS_OK;
}
static int stat_impl(void *p,const char *path,IfsFsEntryV2 *o)
{Volume *v=mounted(p);if(!v)return IFS_FS_INVALID;Entry e;int rc=resolve(v,path,&e);return rc?rc:emit(&e,o);}
static int list_impl(void *p,const char *path,uint32_t i,IfsFsEntryV2 *o)
{Volume *v=mounted(p);if(!v)return IFS_FS_INVALID;Entry dir,e;int rc=resolve(v,path,&dir);if(!rc)rc=child(v,&dir.node,NULL,0,i,&e);return rc?rc:emit(&e,o);}
static int read_impl(void *p,const char *path,uint64_t at,void *out,uint32_t n)
{Volume *v=mounted(p);if(!v)return IFS_FS_INVALID;Entry e;int rc=resolve(v,path,&e);if(rc)return rc;return e.node.type==IFS_FS_ENTRY_FILE?file_read(v,&e.node,at,out,n):IFS_FS_UNSUPPORTED;}
static int query_impl(void *p,IfsFsVolumeInfoV2 *o)
{Volume *v=mounted(p);if(!v||!o||o->structure_bytes<sizeof(*o))return IFS_FS_INVALID;info(v,o);return IFS_FS_OK;}
static int label_impl(void *p,char *out,uint32_t cap,uint32_t *n)
{if(!n)return IFS_FS_INVALID;*n=0;Volume *v=mounted(p);if(!v||!out)return IFS_FS_INVALID;if(cap<=v->label_bytes)return IFS_FS_NO_SPACE;copy(out,v->label,v->label_bytes+1);*n=v->label_bytes;return IFS_FS_OK;}
/* UDF path components, not a POSIX byte string. Convert only the defined
 * local-root, parent, current and filename components. External roots
 * and unknown component types are unsupported. */
static int link_impl(void *p,const char *path,char *out,uint32_t cap,uint32_t *n)
{
    if(!n)return IFS_FS_INVALID;
    *n=0;Volume *v=mounted(p);if(!v||!out)return IFS_FS_INVALID;
    Entry e;int rc=resolve(v,path,&e);if(rc)return rc;
    if(e.node.type!=IFS_FS_ENTRY_LINK||!e.node.bytes||e.node.bytes>4096)return IFS_FS_UNSUPPORTED;
    uint8_t raw[4096];char target[4096];uint32_t used=0;
    rc=file_read(v,&e.node,0,raw,(uint32_t)e.node.bytes);if(rc)return rc;
    for(uint32_t at=0;at<e.node.bytes;){
        if(e.node.bytes-at<4)return IFS_FS_CORRUPT;
        unsigned type=raw[at],len=raw[at+1];
        if(len>e.node.bytes-at-4)return IFS_FS_CORRUPT;
        if(type==2){if(at||len)return IFS_FS_CORRUPT;target[used++]='/';}
        else{
            if(used&&target[used-1]!='/'){if(used==sizeof target-1)return IFS_FS_NO_SPACE;target[used++]='/';}
            if(type==3||type==4){
                if(len||used+2>=sizeof target)return IFS_FS_CORRUPT;
                target[used++]='.';if(type==3)target[used++]='.';
            }else if(type==5){
                uint32_t decoded;rc=unicode(raw+at+4,len,target+used,sizeof target-used,&decoded,1);if(rc)return rc;
                if(!decoded||(decoded==1&&target[used]=='.')||(decoded==2&&same(target+used,"..",2)))return IFS_FS_CORRUPT;
                used+=decoded;
            }else return IFS_FS_UNSUPPORTED;
        }
        at+=4+len;
    }
    if(!used||used>=cap)return IFS_FS_NO_SPACE;
    copy(out,target,used);out[used]=0;*n=used;return IFS_FS_OK;
}
#define WRAP(name,args,call) static int name args{if(!enter())return IFS_FS_BUSY;return leave(call);}
WRAP(probe,(const IfsFsHostRuntimeV2 *h,uint32_t *v),probe_impl(h,v))
WRAP(mount,(const IfsFsHostRuntimeV2 *h,void **p,IfsFsVolumeInfoV2 *o),mount_impl(h,p,o))
WRAP(unmount,(void *p),unmount_impl(p))
WRAP(stat_path,(void *p,const char *s,IfsFsEntryV2 *o),stat_impl(p,s,o))
WRAP(list_path,(void *p,const char *s,uint32_t i,IfsFsEntryV2 *o),list_impl(p,s,i,o))
WRAP(read_path,(void *p,const char *s,uint64_t a,void *b,uint32_t n),read_impl(p,s,a,b,n))
WRAP(query,(void *p,IfsFsVolumeInfoV2 *o),query_impl(p,o))
WRAP(label_path,(void *p,char *b,uint32_t c,uint32_t *n),label_impl(p,b,c,n))
WRAP(readlink_path,(void *p,const char *s,char *b,uint32_t c,uint32_t *n),link_impl(p,s,b,c,n))
static const IfsFsCoreOpsV2 core={.header={2,sizeof(core),IFS_FS_GROUP_CORE,0,IFS_FS_CORE_PROBE|IFS_FS_CORE_MOUNT|IFS_FS_CORE_STAT|IFS_FS_CORE_LIST|IFS_FS_CORE_READ},.probe=probe,.mount=mount,.unmount=unmount,.stat=stat_path,.directory_entry=list_path,.read=read_path};
static const IfsFsVolumeOpsV2 volume={.header={2,sizeof(volume),IFS_FS_GROUP_VOLUME,0,IFS_FS_VOLUME_QUERY|IFS_FS_VOLUME_LABEL_GET},.query=query,.label=label_path};
static const IfsFsMetadataOpsV2 meta={.header={2,sizeof(meta),IFS_FS_GROUP_METADATA,0,IFS_FS_META_READLINK},.readlink=readlink_path};
static const IfsFsGroupHeaderV2 *group(uint32_t id,uint32_t lo,uint32_t hi)
{if(lo>2||hi<2)return NULL;if(id==IFS_FS_GROUP_CORE)return &core.header;if(id==IFS_FS_GROUP_VOLUME)return &volume.header;if(id==IFS_FS_GROUP_METADATA)return &meta.header;return NULL;}
static const IfsFsModuleApiV2 api={.abi_version=2,.structure_bytes=sizeof(api),.format_id="udf",
#ifdef IFS_BOOT_FIRMWARE
.runtime_profiles=IFS_FS_PROFILE_FREESTANDING|IFS_FS_PROFILE_FIRMWARE,
#else
.runtime_profiles=IFS_FS_PROFILE_HOSTED,
#endif
.group_mask=IFS_FS_GROUP_BIT(IFS_FS_GROUP_CORE)|IFS_FS_GROUP_BIT(IFS_FS_GROUP_VOLUME)|IFS_FS_GROUP_BIT(IFS_FS_GROUP_METADATA),.query_group=group};
__attribute__((visibility("default")))const IfsFsModuleApiV2 *infiltrator_filesystem_api_v2(void){return &api;}
