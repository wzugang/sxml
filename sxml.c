#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <float.h>
#include <limits.h>
#include <ctype.h>

#include "sxml.h"

//名域处理、特殊节点扩展（#name,对应开始结束节点标志），innertext与rawdata转换，节点查找，多文件引用扩展。后期完善
//完善入参检查，转义字符处理等（待完成）
//没有子节点与空节点差异，需要进一步处理。用户自定义与原始数据的区别。

//item为引用节点
#define SXML_IS_REFERENCE 		128
//节点名称为常量
#define SXML_IS_STR_CONST		512


// Private macros
#define QUEUE_NEXT(q)       		(*(QUEUE **) &((*(q))[0]))
#define QUEUE_PREV(q)       		(*(QUEUE **) &((*(q))[1]))
#define QUEUE_PREV_NEXT(q)  		(QUEUE_NEXT(QUEUE_PREV(q)))
#define QUEUE_NEXT_PREV(q)  		(QUEUE_PREV(QUEUE_NEXT(q)))

//根据结构体成员变量获取结构体地址
#define QUEUE_DATA(ptr, type, field)                                          		\
  ((type *) ((char *) (ptr) - ((char *) &((type *) 0)->field)))

//循环队列h为队列头,q依次遍历队列的每一个成员
#define QUEUE_FOREACH(q, h)                                                   		\
  for ((q) = QUEUE_NEXT(h); (q) != (h); (q) = QUEUE_NEXT(q))

//判断队列是否为空
#define QUEUE_ISEMPTY(q)                                                        	\
  ((const QUEUE *) (q) == (const QUEUE *) QUEUE_NEXT(q))

//返回头结点地址
#define QUEUE_HEAD(q)                                                         		\
  (QUEUE_NEXT(q))
  
//返回尾结点地址
#define QUEUE_TAIL(q)                                                         		\
  (QUEUE_PREV(q))

//队列初始化
#define QUEUE_INIT(q)                                                         		\
  do {                                                                        		\
    QUEUE_NEXT(q) = (q);                                                      		\
    QUEUE_PREV(q) = (q);                                                      		\
  }                                                                           		\
  while (0)

//队列合并
#define QUEUE_ADD(h, n)                                                       		\
  do {                                                                        		\
    QUEUE_PREV_NEXT(h) = QUEUE_NEXT(n);                                       		\
    QUEUE_NEXT_PREV(n) = QUEUE_PREV(h);                                       		\
    QUEUE_PREV(h) = QUEUE_PREV(n);                                            		\
    QUEUE_PREV_NEXT(h) = (h);                                                 		\
  }                                                                           		\
  while (0)

//队列分割
#define QUEUE_SPLIT(h, q, n)                                                  		\
  do {                                                                        		\
    QUEUE_PREV(n) = QUEUE_PREV(h);                                            		\
    QUEUE_PREV_NEXT(n) = (n);                                                 		\
    QUEUE_NEXT(n) = (q);                                                      		\
    QUEUE_PREV(h) = QUEUE_PREV(q);                                            		\
    QUEUE_PREV_NEXT(h) = (h);                                                 		\
    QUEUE_PREV(q) = (n);                                                      		\
  }                                                                           		\
  while (0)

//head暂存队列头、尾节点地址,做中间转换
//队列前插
#define QUEUE_INSERT_HEAD(h, q)                                               		\
  do {                                                                        		\
    QUEUE_NEXT(q) = QUEUE_NEXT(h);                                            		\
    QUEUE_PREV(q) = (h);                                                      		\
    QUEUE_NEXT_PREV(q) = (q);                                                 		\
    QUEUE_NEXT(h) = (q);                                                      		\
  }                                                                           		\
  while (0)

//队列后插
#define QUEUE_INSERT_TAIL(h, q)                                               		\
  do {                                                                        		\
    QUEUE_NEXT(q) = (h);                                                      		\
    QUEUE_PREV(q) = QUEUE_PREV(h);                                            		\
    QUEUE_PREV_NEXT(q) = (q);                                                 		\
    QUEUE_PREV(h) = (q);                                                      		\
  }                                                                           		\
  while (0)

//把当前节点从其所在的队列中删除
#define QUEUE_REMOVE(q)                                                       		\
  do {                                                                        		\
    QUEUE_PREV_NEXT(q) = QUEUE_NEXT(q);                                       		\
    QUEUE_NEXT_PREV(q) = QUEUE_PREV(q);                                       		\
  }                                                                           		\
  while (0)

#define XZERO(op) 			memset(op, 0, sizeof(__typeof__(*op)))
#define XZERO_LEN(op,len) 	memset(op, 0, len)
#define XALIGN(x,align)		(((x) + (align) - 1) & ~((align)-1))

#if (defined(GCC) || defined(GPP) || defined(__cplusplus))
#define XSTATIC static inline
#else
#define XSTATIC static 
#endif

#define IS_NUM(c)		((c) <= '9' && (c) >= '0')
#define IS_A2F(c)		((c) <= 'F' && (c) >= 'A')
#define IS_a2f(c)		((c) <= 'f' && (c) >= 'a')

#define XCHECK(func)\
do {\
int _s = (func);\
if (_s < 0)\
{\
fprintf(stderr, "Error: %s returned %d\n", #func, _s);\
exit(0);\
}\
} while (0)

	
static const char* CDATA_BEGIN             ="<![CDATA[";
static const char* CDATA_END               ="!]]>";
static const long long CDATA_BEGIN_LEN     =9;
static const long long CDATA_END_LEN       =4;
static const char* CDATA_NAME              ="#rawdata";
static const long long CDATA_LEN           =8;

static const char* COMMENT_BEGIN           ="<!--";
static const char* COMMENT_END             ="-->";
static const long long COMMENT_BEGIN_LEN   =4;
static const long long COMMENT_END_LEN     =3;
static const char* COMMENT_NAME            ="#comment";
static const long long COMMENT_LEN         =8;

static const char* INNER_NAME              ="#innertext";
static const long long INNER_LEN           =10;

//static char* g_sxml_rawdata_tab[]={"#cdata",}


static char sxml_hex_table[128]=
{
	-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
	-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
	-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
	0 , 1, 2, 3, 4, 5, 6, 7, 8, 9,-1,-1,-1,-1,-1,-1,
	-1,10,11,12,13,14,15,-1,-1,-1,-1,-1,-1,-1,-1,-1,
	-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
	-1,10,11,12,13,14,15,-1,-1,-1,-1,-1,-1,-1,-1,-1,
	-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1
};

XSTATIC void*do_alloc(size_t size);

static const char *sxml_error;
static void*(*sxml_alloc)(size_t) = do_alloc;
static void(*sxml_free)(void*) = free;
static sxml_file_info_t* g_sxml_parse_error;
//跳过空白字符
XSTATIC const char *skip(const char *in) {while (in && *in && (unsigned char)*in<=32) in++; return in;}
//选择性跳过空白字符
XSTATIC const char *pskip(const char *in) { const char* p=skip(in); return (*p == '<')?p:in; }
//跳过特定字符串
XSTATIC const char* check_skip(const char* in, const char* str){ const char* start; long long len=strlen(str); if(start = strstr(in, str)){return start+len;}else{return NULL;}} 
//检查是否包含字符串
XSTATIC long long check(const char* in, const char* str){ return strstr(in, str)?1:0;} 
//拷贝到字符串为止
XSTATIC const char* copy_until(char* to, const char* from, const char* flag){ const char* pstr; const char* start; long long len=strlen(flag); if(start = strstr(from, flag)){ pstr = from; while(pstr < start)*to = *pstr, pstr++, to++; return start+len;}else{return NULL;}} 
//跳到下一行位置
XSTATIC const char* skip_line(const char *in){while (in && *in && *in != '\n')in++; return (in && *in == '\n')?++in:NULL;}

XSTATIC char* trim(char *in){char* start; char* pstr; start = (char*)skip(in); pstr = start; while(pstr && *pstr != '\0')pstr++; if(!pstr)return NULL; while(*pstr <= 32){ *pstr = '\0'; pstr--; } return start; }


XSTATIC void*do_alloc(size_t size)
{
	void* retptr=NULL;
	size_t real_size = XALIGN(size, 8);
	retptr = malloc(real_size);
	if(!retptr)return NULL;
	memset(retptr, 0, real_size);
	return retptr;
}
XSTATIC const char* string_index_of_any(const char* str, const char* cs)
{
	long long i,j,len=strlen(str),count=strlen(cs);
	for(i = 0; i < len; i++)
	{
		for(j = 0; j < count; j++)
		{
			if(str[i] == cs[j])
			{
				return str+i;
			}
		}
	}
	return NULL;
}

XSTATIC const char *sxml_error_pick(void) {return sxml_error;}
XSTATIC void sxml_error_clean(void) {sxml_error=NULL;}

XSTATIC int string_expand(int x) { return XALIGN(x,4);}

XSTATIC int sxml_strcasecmp(const char *s1,const char *s2)
{
	if (!s1) return (s1==s2)?0:1;if (!s2) return 1;
	for(; tolower(*s1) == tolower(*s2); ++s1, ++s2)	if(*s1 == 0)	return 0;
	return tolower(*(const unsigned char *)s1) - tolower(*(const unsigned char *)s2);
}

XSTATIC char* string_strdup(const char* str)
{
      int len;
      char* copy;
      len = strlen(str) + 1;
      if (!(copy = (char*)sxml_alloc(len))) return NULL;
      memcpy(copy,str,len);
      return copy;
}

XSTATIC char* string_ensure(sxml_buffer_ht p,int needed)
{
	char *newbuffer;int newsize;
	if (!p || !p->buffer) return NULL;
	needed+=p->offset;
	if (needed<=p->length) return p->buffer+p->offset;
	newsize=string_expand(needed*2);
	newbuffer=(char*)sxml_alloc(newsize);
	if (!newbuffer) {sxml_free(p->buffer);p->length=0,p->buffer=NULL;return NULL;}
	if (newbuffer) memcpy(newbuffer,p->buffer,p->length);
	sxml_free(p->buffer);
	p->length=newsize;
	p->buffer=newbuffer;
	return newbuffer+p->offset;
}

XSTATIC int string_update(sxml_buffer_ht p)
{
	char *str;
	if (!p || !p->buffer) return 0;
	str=p->buffer+p->offset;
	return p->offset+strlen(str);
}

XEXPORT XAPI sxml_file_info_t* sxml_get_file_info(const char* value)
{
	long long i;
	long long line=1;
	const char* pline=value;
	const char* pnext=value;
	sxml_file_info_t* info = (sxml_file_info_t*)sxml_alloc(sizeof(sxml_file_info_t));
	if(!info)return NULL;
	while((pline = skip_line(pline)))line++;
	info->line_count = line;
	info->line_info = (sxml_file_line_t*)sxml_alloc(sizeof(sxml_file_line_t)*line);
	if(!info->line_info)
	{
		sxml_free(info);
		return NULL;
	}
	pline=value;
	for(i = 0; i < line; i++)
	{
		pnext = skip_line(pline);
		info->line_info[i].start = pline;
		info->line_info[i].line = i+1;
		info->line_info[i].len = ((pnext-pline)>0)?pnext-pline:0;
		pline = pnext;
	}
	g_sxml_parse_error = info;//保存到全局变量中
	return info;
}

XEXPORT XAPI void sxml_free_file_info(sxml_file_info_t** info)
{
	sxml_free((*info)->line_info);
	sxml_free(*info);
	*info = NULL;
}

XEXPORT XAPI void sxml_print_file_info(sxml_file_info_t* info)
{
	long long i;
	for(i = 0; i < info->line_count; i++)
	{
		printf("sxml line:%d,len:%d\n",info->line_info[i].line,info->line_info[i].len);
	}
}

XSTATIC sxml_doc_t* sxml_doc_item_new()
{
	sxml_doc_t* doc = (sxml_doc_t*)sxml_alloc(sizeof(sxml_doc_t));
	if(!doc)return NULL; 
	return doc;
}

XEXPORT XAPI sxml_doc_t* sxml_doc_new(const char* filename, const char* version, const char* charset)
{
	if(!charset)return NULL;//charset不能为空
	sxml_doc_t* doc = sxml_doc_item_new();
	if(!doc)return NULL;//内存申请失败
	
	if(NULL != filename)sprintf(doc->filename,"%s",filename);
	if(NULL != version)sprintf(doc->version,"%s",version);
	if(NULL != charset)sprintf(doc->charset,"%s",charset);
	QUEUE_INIT(&doc->dq);
	return doc;
}

XSTATIC sxml_node_t* sxml_node_item_new()
{
	sxml_node_t* node = (sxml_node_t*)sxml_alloc(sizeof(sxml_node_t));
	if(!node)return NULL; 
	return node;
}

XEXPORT XAPI sxml_node_t* sxml_node_type_new(long long type, const char* name)
{
	if(!name)return NULL;
	sxml_node_t* node = sxml_node_item_new();
	if(!node)return NULL;
	node->name = (char*)sxml_alloc(strlen(name)+1);
	if(!node->name){ sxml_free(node); return NULL; }
	node->type = type;
	sprintf(node->name,"%s",name);
	//printf("node->name malloc: %p, %s\n", node->name, node->name);
	node->parent = NULL;
	node->data = NULL;
	node->indent = 0;
	node->index = 0;
	node->childCount = 0;
	node->attrCount = 0;
	node->prevSubling = NULL;
	node->nextSubling = NULL;
	QUEUE_INIT(&node->children);
	QUEUE_INIT(&node->attrs);
	QUEUE_INIT(&node->nq);
	return node;
}

XEXPORT XAPI sxml_node_t* sxml_node_new(const char* name)
{
	return sxml_node_type_new(0,name);
}

XEXPORT XAPI sxml_node_t* sxml_empty_new(const char* name)
{
	return sxml_node_type_new(4,name);
}

XEXPORT XAPI sxml_node_t* sxml_comment_new(const char* comment)
{
	long long len = strlen(comment);
	sxml_node_t* node = sxml_node_type_new(1,"#comment");
	node->data = (char*)sxml_alloc(len+1);
	if(!node->data)
	{
		sxml_node_free(node);
		return NULL;
	}
	memcpy(node->data, comment, len);
	return node;
}

XEXPORT XAPI sxml_node_t* sxml_innertext_new(const char* innertext)
{
	long long len = strlen(innertext);
	sxml_node_t* node = sxml_node_type_new(2,"#innertext");
	node->data = (char*)sxml_alloc(len+1);
	if(!node->data)
	{
		sxml_node_free(node);
		return NULL;
	}
	memcpy(node->data, innertext, len);
	return node;
}

//name为NULL采用默认节点，否则采用自定义节点
XEXPORT XAPI sxml_node_t* sxml_rawdata_new(const char* name, const void* data, long long size)
{
	sxml_node_t* node; 
	if(NULL == data || !size)
	{
		return NULL;
	}
	sxml_data_t* datap = (sxml_data_t*)sxml_alloc(sizeof(sxml_data_t));
	if(!datap)return NULL;
	datap->size = size;
	datap->data = sxml_alloc(size+1);
	if(!datap->data)
	{
		sxml_free(datap);
		return NULL;
	}
	memcpy(datap->data, data, size);
	
	if(!name)
	{
		node = sxml_node_type_new(3,"#rawdata");
	}else
	{
		node = sxml_node_type_new(3,name);
	}
	node->data = datap;
	return node;
}

XSTATIC sxml_attr_t* sxml_attr_item_new()
{
	sxml_attr_t* attr = (sxml_attr_t*)sxml_alloc(sizeof(sxml_attr_t));
	if(!attr)return NULL;
	QUEUE_INIT(&attr->aq);
	attr->prevSubling = NULL;
	attr->nextSubling = NULL;
	attr->owner = NULL;
	attr->index = 0;
	return attr;
}

XEXPORT XAPI sxml_attr_t* sxml_attr_new(const char* name, const char* value)
{
	if(!name)return NULL;
	sxml_attr_t* attr = sxml_attr_item_new();
	QUEUE_INIT(&attr->aq);
	if(!attr)return NULL;
	attr->name = (char*)sxml_alloc(strlen(name)+1);
	if(!attr->name){ sxml_free(attr); return NULL; }
	attr->value = (char*)sxml_alloc(strlen(value)+1);
	if(!attr->value){ sxml_free(attr->name); sxml_free(attr); return NULL; }
	
	if(!strncmp(name,"xmlns:",6))attr->type = 2;
	if(NULL != name)sprintf(attr->name,"%s",name);
	if(NULL != value)sprintf(attr->value,"%s",value);
	return attr;
}

XEXPORT XAPI void sxml_attr_free(sxml_attr_t* attr)
{
	if(!attr) return;
	if(!attr->name)sxml_free(attr->name);
	if(!attr->value)sxml_free(attr->value);
	sxml_free(attr);
}

XSTATIC void sxml_node_attrs_free(sxml_node_t* node)
{
	QUEUE* q;
	sxml_attr_t* attr;
	while(!QUEUE_ISEMPTY(&node->attrs))
	{
		QUEUE_FOREACH(q, &node->attrs)
		{
			QUEUE_REMOVE(q);
			attr = (sxml_attr_t*)QUEUE_DATA(q,sxml_attr_t,aq);
			sxml_attr_free(attr);
			break; //从头开始遍历
		}
	}
}


XEXPORT XAPI void sxml_node_free(sxml_node_t* node)
{
	QUEUE* q;
	sxml_attr_t* attr;
	sxml_node_t* child;
	if(!node) return;
	//先释放子节点和属性
	sxml_node_attrs_free(node);
	
	while(!QUEUE_ISEMPTY(&node->children))
	{
		QUEUE_FOREACH(q, &node->children)
		{
			QUEUE_REMOVE(q);
			child = (sxml_node_t*)QUEUE_DATA(q,sxml_node_t,nq);
			sxml_node_free(child);
			break;
		}
	}
	
	//再释放自己
	if(node->name)
	{
		//printf("node->name free: %p, %s\n",node->name, node->name);
		sxml_free(node->name);
	}
	if(node->data)
	{
		sxml_free(node->data);
	}
	sxml_free(node);
}

XSTATIC void sxml_node_children_free(sxml_node_t* node)
{
	QUEUE* q;
	sxml_node_t* child;
	while(!QUEUE_ISEMPTY(&node->children))
	{
		QUEUE_FOREACH(q, &node->children)
		{
			QUEUE_REMOVE(q);
			child = (sxml_node_t*)QUEUE_DATA(q,sxml_node_t,nq);
			sxml_node_free(child);
			break; //从头开始遍历
		}
	}
}

XEXPORT XAPI void sxml_doc_free(sxml_doc_t* doc)
{
	QUEUE* q;
	sxml_node_t* node;
	if(!doc) return;
	//先释放节点
	while(!QUEUE_ISEMPTY(&doc->dq))
	{
		QUEUE_FOREACH(q, &doc->dq)
		{
			QUEUE_REMOVE(q);
			node = (sxml_node_t*)QUEUE_DATA(q,sxml_node_t,nq);
			sxml_node_free(node);
			break;
		}
	}
	
	//再释放doc
	sxml_free(doc);
}

XEXPORT XAPI int sxml_add_attr2node(sxml_node_t* node, sxml_attr_t* attr)
{
	QUEUE* q;
	sxml_attr_t* tail;
	if(!attr)return -1;
	
	if(QUEUE_ISEMPTY(&node->attrs))
	{
		QUEUE_INSERT_TAIL(&node->attrs,&attr->aq);
		attr->prevSubling = NULL;
		attr->nextSubling = NULL;
		attr->index = 1;
	}else
	{
		q = QUEUE_TAIL(&node->attrs);
		tail = (sxml_attr_t*)QUEUE_DATA(q,sxml_attr_t,aq);
		QUEUE_INSERT_TAIL(&node->attrs,&attr->aq);
		tail->nextSubling = attr;
		attr->prevSubling = tail;
		attr->nextSubling = NULL;
		attr->index = tail->index + 1;
	}
	attr->owner = node;
	node->attrCount++;	
	return 0;
}

XEXPORT XAPI int sxml_add_node2doc(sxml_doc_t* doc, sxml_node_t* node)
{
	if(!doc || !node)return -1;
	QUEUE_INSERT_TAIL(&doc->dq,&node->nq);
	node->parent = NULL;
	return 0;
}

XEXPORT XAPI int sxml_add_subnode2node(sxml_node_t* node, sxml_node_t* child)
{
	QUEUE* q;
	sxml_node_t* tail;
	if(!node || !child)return -1;
	if(QUEUE_ISEMPTY(&node->children))
	{
		QUEUE_INSERT_TAIL(&node->children,&child->nq);
		child->prevSubling = NULL;
		child->nextSubling = NULL;
		child->index = 1;
	}else
	{
		q = QUEUE_TAIL(&node->children);
		tail = (sxml_node_t*)QUEUE_DATA(q,sxml_node_t,nq);
		QUEUE_INSERT_TAIL(&node->children,&child->nq);
		tail->nextSubling = child;
		child->prevSubling = tail;
		child->nextSubling = NULL;
		child->index = tail->index + 1;
	}
	node->childCount++;
	
	child->parent = node;
	child->indent = node->indent + 1;
	
	return 0;
}


XEXPORT XAPI char* sxml_attr_print(sxml_node_t* node, sxml_buffer_ht p)
{
	QUEUE* q;
	sxml_attr_t* attr;
	char* tmp=NULL;
	char* ret=NULL;
	int needed=0,numentries=0,i=0;
	if(p)
	{
		tmp = string_ensure(p, 0); ret = tmp; 
		QUEUE_FOREACH(q, &node->attrs)
		{
			attr = (sxml_attr_t*)QUEUE_DATA(q,sxml_attr_t,aq);
			needed = strlen(attr->name)+strlen(attr->value)+5;
			tmp = string_ensure(p, needed); 
			sprintf(tmp, " %s=\"%s\"", attr->name, attr->value);
			p->offset = string_update(p);
		}
	}else
	{
		QUEUE_FOREACH(q, &node->attrs)
		{
			attr = (sxml_attr_t*)QUEUE_DATA(q,sxml_attr_t,aq);
			needed += strlen(attr->name) + strlen(attr->value)+4;
		}
		ret = (char*)sxml_alloc(needed+1);
		if(!ret) return NULL;
		tmp = ret;
		QUEUE_FOREACH(q, &node->attrs)
		{
			attr = (sxml_attr_t*)QUEUE_DATA(q,sxml_attr_t,aq);
			tmp += sprintf(tmp," %s=\"%s\"",attr->name, attr->value);
		}
	}
	
	return ret;
}

XEXPORT XAPI char* sxml_node_print(sxml_node_t* node, sxml_buffer_ht p)
{
	int needed=0,numentries=0,i=0,indent=0;
	char* str;
	char** entries;
	QUEUE* q;
	sxml_node_t* child;
	sxml_data_t* data;
	char* ret=NULL;
	char* tmp;
	if(p)
	{
		switch(node->type)
		{
			case 0: 
				if(!node->prevSubling || 2 != node->prevSubling->type)
				{
					str = string_ensure(p, 2); str[0] = '\n'; str[1] = '\0'; p->offset = string_update(p);//普通节点，需要换行
					if(node->indent)
					{
						needed = node->indent*SXML_INDENT_COUNT;
						str = string_ensure(p, needed+1);
						for(i = 0; i < needed; i++)
						{
							str[i] = ' ';
						}
						str[i] = '\0';
						p->offset = string_update(p); ret = str; 
					}
				}
				
				needed = strlen(node->name)+2;
				str = string_ensure(p, needed);
				sprintf(str, "<%s", node->name); p->offset = string_update(p); if(!ret) ret = str; 
				sxml_attr_print(node, p);
				//if has no children
				if(QUEUE_ISEMPTY(&node->children))
				{
					str = string_ensure(p, 4); str[0] = ' '; str[1] = '/'; str[2] = '>'; str[3] = '\0'; p->offset = string_update(p);
				}else
				{
					str = string_ensure(p, 2); str[0] = '>'; str[1] = '\0'; p->offset = string_update(p); //换行取决于子节点类型，不应该在这里换行str[2] = '\0';,子节点不为Innertext则换行
					QUEUE_FOREACH(q, &node->children)
					{
						child = (sxml_node_t*)QUEUE_DATA(q,sxml_node_t,nq);
						sxml_node_print(child, p);
					}
					if(QUEUE_ISEMPTY(&node->children) || (!QUEUE_ISEMPTY(&node->children) && 2 != child->type))
					{
						str = string_ensure(p, 2); str[0] = '\n'; str[1] = '\0'; p->offset = string_update(p); 
						if(node->indent)
						{
							needed = node->indent*SXML_INDENT_COUNT;
							str = string_ensure(p, needed+1);
							for(i = 0; i < needed; i++)
							{
								str[i] = ' ';
							}
							str[i] = '\0';
							p->offset = string_update(p);
						}
					}
					needed = strlen(node->name)+5; str = string_ensure(p, needed); sprintf(str, "</%s>", node->name); p->offset = string_update(p); 	
				}
				break;//普通节点，节点头左+节点属性+节点头右+子节点+节点尾
			case 1:
				if(!node->prevSubling || 2 != node->prevSubling->type)
				{
					str = string_ensure(p, 2); str[0] = '\n'; str[1] = '\0'; p->offset = string_update(p);//注释节点，需要换行
					if(node->indent)
					{
						needed = node->indent*SXML_INDENT_COUNT;
						str = string_ensure(p, needed+1);
						for(i = 0; i < needed; i++)
						{
							str[i] = ' ';
						}
						str[i] = '\0';
						p->offset = string_update(p); ret = str; 
					}
				}
				needed = strlen((char*)node->data)+8; str = string_ensure(p, needed); sprintf(str, "<!--%s-->", (char*)node->data); p->offset = string_update(p); if(!ret) ret = str; 
				break;//注释节点，<!--+注释+-->
			case 2: //内嵌文本，什么都不需要做
				needed = strlen((char*)node->data)+1; str = string_ensure(p, needed); sprintf(str, "%s", (char*)node->data); p->offset = string_update(p); if(!ret) ret = str; 
				break;//内嵌文本，内容
			case 3: 
				if(!node->prevSubling || 2 != node->prevSubling->type)
				{
					str = string_ensure(p, 2); str[0] = '\n'; str[1] = '\0'; p->offset = string_update(p);//原始数据节点，需要换行
					if(node->indent)
					{
						needed = node->indent*SXML_INDENT_COUNT;
						str = string_ensure(p, needed+1);
						for(i = 0; i < needed; i++)
						{
							str[i] = ' ';
						}
						str[i] = '\0';
						p->offset = string_update(p); ret = str; 
					}
				}
				data = (sxml_data_t*)node->data;
				if(!strcmp(node->name,"#rawdata"))
				{
					needed = data->size+13; str = string_ensure(p, needed); sprintf(str, "<![CDATA[%s]]>", (char*)data->data); p->offset = string_update(p); 
				}else
				{
					if(node->indent)
					{
						needed = data->size+3+2*strlen(node->name); str = string_ensure(p, needed); sprintf(str, "<%s>", node->name); p->offset = string_update(p); 
						
						//needed = node->indent*SXML_INDENT_COUNT+SXML_INDENT_COUNT; str = string_ensure(p, needed+1);
						//for(i = 0; i < needed; i++)
						//{
						//	str[i] = ' ';
						//}
						//str[i] = '\0';p->offset = string_update(p);					
						needed = data->size+1+2*strlen(node->name); str = string_ensure(p, needed); sprintf(str, "%s", (char*)data->data); p->offset = string_update(p); 
						
						//needed = node->indent*SXML_INDENT_COUNT; str = string_ensure(p, needed+1);
						//for(i = 0; i < needed; i++)
						//{
						//	str[i] = ' ';
						//}
						//str[i] = '\0'; p->offset = string_update(p);
						
						needed = data->size+4+2*strlen(node->name); str = string_ensure(p, needed); sprintf(str, "</%s>", node->name); p->offset = string_update(p); 
					}else
					{
						needed = data->size+6+2*strlen(node->name); str = string_ensure(p, needed); sprintf(str, "<%s>%s</%s>", node->name, (char*)data->data, node->name); p->offset = string_update(p); 
					}
				}
				if(!ret) ret = str; 
				break;//原始数据，<![CDATA[+原始数据+]]>
			case 4: 
				if(!node->prevSubling || 2 != node->prevSubling->type)
				{
					str = string_ensure(p, 2); str[0] = '\n'; str[1] = '\0'; p->offset = string_update(p);//空节点，需要换行
					if(node->indent)
					{
						needed = node->indent*SXML_INDENT_COUNT;
						str = string_ensure(p, needed+1);
						for(i = 0; i < needed; i++)
						{
							str[i] = ' ';
						}
						str[i] = '\0';
						p->offset = string_update(p); ret = str; 
					}
				}
				needed = strlen((char*)node->name)+5; str = string_ensure(p, needed); sprintf(str, "<%s/>", (char*)node->name); p->offset = string_update(p); if(!ret) ret = str; 
				break;//空置节点，节点
		}
	}else
	{
		switch(node->type)
		{
			case 0: 
				//计算长度
				if(!QUEUE_ISEMPTY(&node->attrs))
				{
					++numentries;
				}
				QUEUE_FOREACH(q, &node->children)
				{
					++numentries;
				}
				entries = (char**)sxml_alloc(numentries*sizeof(char*));
				if(!entries) return NULL;
				if(!QUEUE_ISEMPTY(&node->attrs))
				{				
					entries[i++] = sxml_attr_print(node, p);
				}					
				QUEUE_FOREACH(q, &node->children)
				{
					child = (sxml_node_t*)QUEUE_DATA(q,sxml_node_t,nq);
					entries[i++] = sxml_node_print(child, p);
				}
				for(i = 0; i < numentries; i++)
				{
					needed += strlen(entries[i]);
				}
				if(QUEUE_ISEMPTY(&node->children))
				{
					needed += strlen(node->name)*2 + 6;	
				}else
				{
					needed += strlen(node->name)*2 + 8;	
				}
				indent = (node->indent)?node->indent*SXML_INDENT_COUNT:0; 
				needed += indent*2; 
				ret = (char*)sxml_alloc(needed);
				if(!ret)
				{
					for(i = 0; i < numentries; i++)
					{
						sxml_free(entries[i]);
					}
					sxml_free(entries);
					return NULL;
				}
				tmp = ret;
				if(!node->prevSubling || 2 != node->prevSubling->type)
				{
					*tmp = '\n'; ++tmp;
					//前缩进
					if(node->indent)
					{
						for(i = 0; i < indent; i++)
						{
							*tmp = ' '; ++tmp;
						}
					}
				}
				if(QUEUE_ISEMPTY(&node->children))
				{
					if(!QUEUE_ISEMPTY(&node->attrs))
					{
						tmp += sprintf(tmp, "<%s%s />", node->name, entries[0]);
					}else
					{
						tmp += sprintf(tmp, "<%s />", node->name);
					}
				}else
				{
					if(!QUEUE_ISEMPTY(&node->attrs))
					{
						tmp += sprintf(tmp, "<%s%s>", node->name, entries[0]);//换行不在这里处理
						for(i = 1; i < numentries; i++)
						{
							tmp += sprintf(tmp, "%s", entries[i]);
						}
					}else
					{
						tmp += sprintf(tmp, "<%s>", node->name);//换行不在这里处理
						for(i = 0; i < numentries; i++)
						{
							tmp += sprintf(tmp, "%s", entries[i]);
						}
					}
					
					if(QUEUE_ISEMPTY(&node->children) || (!QUEUE_ISEMPTY(&node->children) && 2 != child->type))
					{
						*tmp = '\n'; ++tmp;
						//后缩进
						if(node->indent)
						{
							for(i = 0; i < indent; i++)
							{
								*tmp = ' '; ++tmp;
							}
						}
					}
					
					sprintf(tmp, "</%s>", node->name);
				}
				//释放没用的内存
				for(i = 0; i < numentries; i++)
				{
					sxml_free(entries[i]);
				}
				sxml_free(entries);
				break;//普通节点，节点头左+节点属性+节点头右+子节点+节点尾
			case 1: 
				needed = strlen((char*)node->data)+10; 
				indent = (node->indent)?node->indent*SXML_INDENT_COUNT:0; 
				needed += indent; 
				ret = (char*)sxml_alloc(needed); 
				if(!ret) return NULL;
				tmp = ret;
				if(!node->prevSubling || 2 != node->prevSubling->type)
				{
					*tmp = '\n';++tmp;
					if(node->indent)
					{
						for(i = 0; i < indent; i++)
						{
							*tmp = ' '; ++tmp;
						}
					}
				}
				sprintf(tmp, "<!--%s-->", (char*)node->data); 
				break;//注释节点，<!--+注释+-->
			case 2: 
				needed = strlen((char*)node->data)+1;
				ret = (char*)sxml_alloc(needed); if(!ret) return NULL; 
				tmp = ret;
				sprintf(tmp, "%s", (char*)node->data);
				break;//内嵌文本，内容
			case 3: 
				data = (sxml_data_t*)node->data;
				if(!strcmp(node->name,"#rawdata"))
				{
					needed = data->size+15; 
					indent = (node->indent)?node->indent*SXML_INDENT_COUNT:0; 
					needed += indent; 
					ret = (char*)sxml_alloc(needed); if(!ret) return NULL; 
					tmp = ret;
					if(!node->prevSubling || 2 != node->prevSubling->type)
					{
						*tmp = '\n';++tmp;
						if(node->indent)
						{
							for(i = 0; i < indent; i++)
							{
								*tmp = ' '; ++tmp;
							}
						}
					}
					sprintf(tmp, "<![CDATA[%s]]>", (char*)data->data); 
				}else
				{
					needed = data->size+8+2*strlen(node->name); 
					indent = (node->indent)?node->indent*SXML_INDENT_COUNT:0; 
					needed += indent*3+SXML_INDENT_COUNT; 
					ret = (char*)sxml_alloc(needed); if(!ret) return NULL; 
					tmp = ret;
					if(node->indent)
					{
						if(!node->prevSubling || 2 != node->prevSubling->type)
						{
							*tmp = '\n';++tmp;
							for(i = 0; i < indent; i++)
							{
								*tmp = ' '; ++tmp;
							}
						}
						tmp += sprintf(tmp, "<%s>", node->name);
						//for(i = 0; i < indent+SXML_INDENT_COUNT; i++)
						//{
						//	*tmp = ' '; ++tmp;
						//}
						tmp += sprintf(tmp, "%s", (char*)data->data);
						//for(i = 0; i < indent; i++)
						//{
						//	*tmp = ' '; ++tmp;
						//}
						tmp += sprintf(tmp, "</%s>", node->name);
					}else
					{
						if(!node->prevSubling || 2 != node->prevSubling->type)
						{
							*tmp = '\n';++tmp;
						}
						tmp += sprintf(tmp, "<%s>%s</%s>", node->name, (char*)data->data, node->name);
					}
				}
				break;//原始数据，<![CDATA[+原始数据+]]>
			case 4: 
				needed = strlen((char*)node->name)+6; 
				indent = (node->indent)?node->indent*SXML_INDENT_COUNT:0; 
				needed += indent; 
				ret = (char*)sxml_alloc(needed); if(!ret) return NULL; 
				tmp = ret;
				if(!node->prevSubling || 2 != node->prevSubling->type)
				{
					*tmp = '\n';++tmp;
					if(node->indent)
					{
						for(i = 0; i < indent; i++)
						{
							*tmp = ' '; ++tmp;
						}
					}
				}
				sprintf(tmp, "<%s/>", (char*)node->name); 
				break;//空置节点，节点
		}
		
	}
	
	return ret;
}

XEXPORT XAPI char *sxml_node_print_buffered(sxml_node_t* node,int size)
{
	sxml_buffer_t p;
	p.buffer=(char*)sxml_alloc(size);
	if(!p.buffer) return NULL;
	p.length=size;
	p.offset=0;
	sxml_node_print(node, &p);
	return p.buffer;
}

XEXPORT XAPI char *sxml_doc_print_buffered(sxml_doc_t* doc,int size)
{
	long long needed=0;
	char* str;
	sxml_buffer_t p;
	QUEUE* q;
	sxml_node_t* child;
	if(!doc)return NULL;
	p.buffer=(char*)sxml_alloc(size);
	if(!p.buffer) return NULL;
	p.length=size;
	p.offset=0;
	needed = 26+16;
	str = string_ensure(&p, needed);
	sprintf(str, "<?xml version=\"%s\" encoding=\"%s\"?>", doc->version, doc->charset);
	p.offset = string_update(&p);
	
	QUEUE_FOREACH(q, &doc->dq)
	{
		child = (sxml_node_t*)QUEUE_DATA(q,sxml_node_t,nq);
		sxml_node_print(child, &p);
	}
	
	return p.buffer;
}

XEXPORT XAPI char *sxml_doc_print(sxml_doc_t* doc)
{
	char* pbuf=NULL,*tmp;
	char** entries;
	int len=0,numentries=0,i=0;
	QUEUE* q;
	sxml_node_t* child;
	len += strlen(doc->version)+strlen(doc->charset)+32;
	
	QUEUE_FOREACH(q, &doc->dq)
	{
		++numentries;
	}
	entries = (char**)sxml_alloc(numentries*sizeof(char*));
	if(!entries) return NULL;
	QUEUE_FOREACH(q, &doc->dq)
	{
		child = (sxml_node_t*)QUEUE_DATA(q,sxml_node_t,nq);
		entries[i++] = sxml_node_print(child, NULL);
	}
	
	for(i = 0; i < numentries; i++)
	{
		len += strlen(entries[i]);
	}
	pbuf = (char*)sxml_alloc(len);
	if(!pbuf) 
	{
		for(i = 0; i < numentries; i++)
		{
			sxml_free(entries[i]);
		}
		sxml_free(entries);
		return NULL;
	}
	tmp = pbuf;
	tmp += sprintf(tmp, "<?xml version=\"%s\" encoding=\"%s\"?>", doc->version, doc->charset);
	
	for(i = 0; i < numentries; i++)
	{
		sprintf(tmp, "%s", entries[i]);
	}
	for(i = 0; i < numentries; i++)
	{
		sxml_free(entries[i]);
	}
	sxml_free(entries);
	
	return pbuf;
}

XEXPORT XAPI int sxml_save2file(sxml_doc_t* doc, const char* filename)
{
	FILE* fp;
	unsigned long long len;
	char* pbuff = sxml_doc_print_buffered(doc,16); 
	if(!pbuff) return -1;
	fp=fopen(filename,"wb"); 
	if(!fp)
	{
		sxml_free(pbuff); 
		return -1; 
	}
	len = strlen(pbuff); 
	fwrite(pbuff,1,len,fp);fclose(fp); 
	sxml_free(pbuff); 
	return 0;
}

XEXPORT XAPI int sxml_save(sxml_doc_t* doc)
{
	return sxml_save2file(doc,doc->filename);
}

XSTATIC int sxml_error_log(const char* pstr)
{
	//遍历所有行，找到出错行和偏移
	long long i;
	long long position = 0;
	for(i = 0; i < g_sxml_parse_error->line_count; i++)
	{
		if(pstr < (g_sxml_parse_error->line_info[i].start+g_sxml_parse_error->line_info[i].len) && pstr >= g_sxml_parse_error->line_info[i].start)
		{
			break;
		}
	}
	if(i >= g_sxml_parse_error->line_count)
	{
		return -1;
	}
	//printf("%*.*s",N,M,str);//输出指定长度N的字符串，超长M时截断，不足时右对齐，左边补空格
	//printf("%-*.*s",N,M,str);//输出指定长度N的字符串，超长M时截断，不足时左对齐，右边补空格
	//printf("%*s",N,str);//输出指定长度N的字符串，超长N时截断
	//printf("%*d",N,str);//输出指定长度N数字，右对齐
	position =  pstr - g_sxml_parse_error->line_info[i].start;
	printf("parse error, line:%d, position:%d, errorstring:%*.*s\n",g_sxml_parse_error->line_info[i].line, position, g_sxml_parse_error->line_info[i].len-position, g_sxml_parse_error->line_info[i].len-position, pstr); //
	return 0;
}


XEXPORT XAPI const char* sxml_attr_parse(sxml_attr_t* attr, const char* value)
{
	const char* temp=value;
	const char* c=NULL;
	do
	{
		temp=skip(temp);
		c = string_index_of_any(temp,"=");
		if(!c)break;
		attr->name = sxml_alloc(c-temp+1);
		if(!attr->name)
		{
			break;
		}
		memcpy(attr->name, temp, c-temp);
		//snprintf(attr->name, c-temp+1, "%s", temp);//snprintf不靠谱
		temp = c;
		c = string_index_of_any(temp,"\"");
		if(!c)break;
		temp = c+1;
		c = string_index_of_any(temp,"\"");
		if(!c)break;
		attr->value = sxml_alloc(c-temp+1);
		if(!attr->value)
		{
			break;
		}
		memcpy(attr->value, temp, c-temp);
		//snprintf(attr->value, c-temp+1, "%s",temp);
		QUEUE_INIT(&attr->aq);
		return c+1;
	}while(0);
	sxml_error_log(temp);
	return NULL;
}


//node之间的关系组织在外部进行
XEXPORT XAPI const char* sxml_node_parse(sxml_node_t* node, const char* value)
{
	const char* temp=value;
	const char* pstr;
	const char* c;
	sxml_attr_t* attr=NULL;
	const char* endp=NULL;
	sxml_data_t* datap=NULL;
	sxml_node_t* subnode=NULL;
	char endbuf[128]={0};
	
	//<+node
	do
	{
		//CDATA处理,不需要trim
		if(strlen(temp) > CDATA_BEGIN_LEN)
		{
			if(!strncmp(temp,CDATA_BEGIN,CDATA_BEGIN_LEN))
			{
				temp = temp + CDATA_BEGIN_LEN;
				endp = strstr(temp,CDATA_END);
				if(!endp)break;			
				
				node->name = (char*)sxml_alloc(CDATA_LEN+1);
				if(!node->name)break;
				node->type = 3;
				sprintf(node->name,"%s",CDATA_NAME);
				node->parent = NULL;
				node->indent = 0;
				
				datap = (sxml_data_t*)sxml_alloc(sizeof(sxml_data_t));
				if(!datap)
				{
					sxml_free(node->name);
					printf("sxml_alloc error\n");
					break;
				}
				datap->size = endp - temp;
				datap->data = sxml_alloc(datap->size+1);
				if(!datap->data)
				{
					sxml_free(node->name);
					sxml_free(datap);
					printf("sxml_alloc error\n");
					return NULL;
				}
				memcpy(datap->data, temp, datap->size);	
				node->data = datap;
				return endp+CDATA_END_LEN;
			}
		}
		
		//注释处理,不需要trim
		if(strlen(temp) > COMMENT_BEGIN_LEN)
		{
			if(!strncmp(temp, COMMENT_BEGIN, COMMENT_BEGIN_LEN))
			{
				temp = temp + COMMENT_BEGIN_LEN;
				endp = strstr(temp,COMMENT_END);
				if(!endp)break;	
				
				node->name = (char*)sxml_alloc(COMMENT_LEN+1);
				if(!node->name)break;
				node->type = 1;
				sprintf(node->name,"%s",COMMENT_NAME);
				node->parent = NULL;
				node->indent = 0;
				
				node->data = sxml_alloc(endp - temp+1);
				if(!node->data)
				{
					sxml_free(node->name);
					printf("sxml_alloc error\n");
					break;
				}
				memcpy(node->data, temp, endp - temp);
					
				return endp+COMMENT_END_LEN;
			}
		}
		
		//innertext,不需要trim
		pstr = skip(temp); // temp =
		if(*pstr != '<')//innertext,trim操作
		{
			do
			{		
				c = strstr(pstr,"<");
				pstr = c+1;
			}while(c && *c && *(c-1) == '\\');
			if(!c)
			{
				printf("innertext parse error !\n"); //后期用静态全局变量替换
				//innertext,error
				break;
			}else
			{
				//后面有其它节点,做完当前结点返回,一次只完成一个节点。
				node->name = sxml_alloc(INNER_LEN+1);
				if(!node->name)
				{
					printf("sxml_alloc error\n");
					break;
				}
				memcpy(node->name, INNER_NAME, INNER_LEN);
				//snprintf(node->name, INNER_LEN+1, "%s",INNER_NAME);
				pstr = skip(value);
				//node->data = sxml_alloc(c-value+1);
				node->data = sxml_alloc(c-pstr+1);
				if(!node->data)
				{
					sxml_free(node->name);
					printf("sxml_alloc error\n");
					break;
				}
				//memcpy(node->data, value, c-value);
				memcpy(node->data, pstr, c-pstr);
				node->type = 2;
				node->parent = NULL;
				node->indent = 0;
				return c;
			}
		}
		//普通节点
		c = string_index_of_any(value," \t>/");
		if(!c)break;
		node->name = sxml_alloc(c-value);
		if(!node->name)
		{
			printf("sxml_alloc error\n");
			break;
		}
		memcpy(node->name, value+1, c-value-1);
		//snprintf(node->name, c-value, "%s",value+1);
		pstr = temp = c;
		if(*c == ' ' || *c == '\n')//带属性节点
		{
			while(*c == ' ')
			{
				//分情况处理
				pstr = c;
				while(*pstr == ' ' || *pstr == '\t')pstr++;
				if('/' == *pstr)
				{
					if(*(pstr+1) == '>')
					{
						node->type = 0;
						return pskip(pstr+2);
					}else
					{
						sxml_error_log(temp);
						return NULL;
					}
				}
				attr = sxml_attr_item_new();
				QUEUE_INIT(&attr->aq);
				c = sxml_attr_parse(attr,temp);
				sxml_add_attr2node(node,attr);
				temp = c;
			}
			pstr = temp = c;
			if(!pstr)break;
			pstr=temp=skip(temp);
			//开始处理节点,子节点
			if(*pstr != '>')
			{
				//释放内存
				sxml_free(node->name);
				sxml_attr_free(attr);
				break;
			}
			sprintf(endbuf,"</%s>",node->name);//尾节点名称构造
			endp = strstr(temp+1,endbuf);
			if(!endp)
			{
				sxml_free(node->name);
				sxml_attr_free(attr);
				break;
			}
			pstr=temp=pskip(temp+1);
			//子节点,递归调用
			while(pstr < endp)
			{
				subnode = sxml_node_item_new();
				QUEUE_INIT(&subnode->children);
				QUEUE_INIT(&subnode->attrs);
				QUEUE_INIT(&subnode->nq);
				pstr = sxml_node_parse(subnode,temp);
				if(!pstr)
				{
					sxml_node_free(subnode);
					break;
				}
				sxml_add_subnode2node(node,subnode);
				temp = pskip(pstr);
			}
			if(temp != endp)
			{
				//释放属性和子节点，退出
				//todo
				sxml_node_attrs_free(node);
				sxml_node_children_free(node);
				break;
			}
			node->type = 0;
			return endp+strlen(endbuf);
			
			
		}else if(*c == '>')//不带属性节点,节点头，找到节点尾，遍历中间解析
		{
			sprintf(endbuf,"</%s>",node->name);//尾节点名称构造
			endp = strstr(temp+1,endbuf);
			if(!endp)
			{
				sxml_free(node->name);
				sxml_attr_free(attr);
				break;
			}
			pstr=temp= pskip(temp+1);
			//子节点,递归调用
			while(pstr < endp)
			{
				subnode = sxml_node_item_new();
				QUEUE_INIT(&subnode->children);
				QUEUE_INIT(&subnode->attrs);
				QUEUE_INIT(&subnode->nq);
				pstr = sxml_node_parse(subnode,temp);
				if(!pstr)
				{
					sxml_node_free(subnode);
					break;
				}
				sxml_add_subnode2node(node,subnode);
				temp = pskip(pstr);
			}
			if(pstr != endp)
			{
				//释放属性和子节点，退出
				//todo
				sxml_node_attrs_free(node);
				sxml_node_children_free(node);
				break;
			}
			node->type = 0;
			return endp+strlen(endbuf);
		}else if(*c == '/')//不带属性节点,空节点，类型赋值，返回
		{
			if(*(c+1) == '>')
			{
				node->type = 4;
				return pskip(c+2);
			}else
			{
				break;
			}
		}else
		{
			printf("unknow error\n");
			break;
		}
	}while(0);
	
	sxml_error_log(temp);
	return NULL;
}


XEXPORT XAPI const char* sxml_doc_head_parse(sxml_doc_t* doc, const char* value)
{
	const char* pstr=NULL;
	const char* temp=value;
	do
	{
		pstr = check_skip(temp,"<?xml version=\"");
		if(!pstr)break;
		
		temp = pstr;
		if(NULL != (pstr = copy_until(doc->version, temp, "\"")))
		{
			if(!!strcmp(doc->version, "1.0"))break;
		}
		
		temp = pstr;
		pstr = check_skip(temp," encoding=\"");
		if(!pstr)break;
		
		temp = pstr;
		if(NULL != (pstr = copy_until(doc->charset, temp, "\"")))
		{
			if(!!strcmp(doc->charset, "GB2312") && !!strcmp(doc->charset, "UTF-8"))break;
		}
		
		temp = pstr;
		pstr = check_skip(temp,"?>\n");
		if(!pstr)break;
		return pstr;
	}while(0);
	
	sxml_error_log(temp);
	return NULL;
}


XEXPORT XAPI sxml_doc_t* sxml_doc_parse(const char* filename, const char* value)
{
	const char* ret;
	long long len;
	sxml_node_t* node=NULL;
	sxml_doc_t* doc=sxml_doc_item_new();
	QUEUE_INIT(&doc->dq);
	if(!doc) return NULL;
	sprintf(doc->filename, "%s", filename);
	ret = sxml_doc_head_parse(doc, value);
	if(!ret)
	{
		sxml_free(doc);
		return NULL;
	}
	while(!!ret && *skip(ret))
	{
		node = sxml_node_item_new();	
		QUEUE_INIT(&node->children);
		QUEUE_INIT(&node->attrs);
		QUEUE_INIT(&node->nq);
		ret = sxml_node_parse(node, ret);
		if(!ret)
		{
			sxml_free(node);
		}
		sxml_add_node2doc(doc, node);
	}
	if(QUEUE_ISEMPTY(&doc->dq))
	{
		sxml_free(doc);
		return NULL;
	}
	return doc;
}

XEXPORT XAPI sxml_doc_t* sxml_parse(const char* filename)
{
	sxml_doc_t* doc=NULL;
	sxml_file_info_t* info=NULL;
	FILE *f;long len;char *data;	
	f=fopen(filename,"rb");fseek(f,0,SEEK_END);len=ftell(f);fseek(f,0,SEEK_SET);
	data=(char*)sxml_alloc(len+1);
	fread(data,1,len,f);fclose(f);
	info = sxml_get_file_info(data);
	doc=sxml_doc_parse(filename, data);
	sxml_free(data);
	if(info)
	{
		sxml_free_file_info(&info);
		g_sxml_parse_error = NULL;
	}
	
	if (!doc) 
	{
		//printf("Failed to parse file: %s, error:[%s]\n",filename, sxml_error_pick());
		return NULL;
	}
	return doc;
}





