#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <float.h>
#include <limits.h>
#include <ctype.h>

#include "sxml.h"

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

static const char *sxml_error;
static void*(*sxml_alloc)(size_t) = malloc;
static void(*sxml_free)(void*) = free;

XSTATIC const char *skip(const char *in) {while (in && *in && (unsigned char)*in<=32) in++; return in;}

const char *sxml_error_pick(void) {return sxml_error;}
void sxml_error_clean(void) {sxml_error=NULL;}

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


XSTATIC sxml_doc_t* sxml_doc_item_new()
{
	sxml_doc_t* doc = (sxml_doc_t*)sxml_alloc(sizeof(sxml_doc_t));
	if(!doc)return NULL; 	
	memset(doc,0,sizeof(sxml_doc_t));
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
	memset(node,0,sizeof(sxml_node_t));
	return node;
}

XEXPORT XAPI sxml_node_t* sxml_node_type_new(long long type, const char* name)
{
	if(!name)return NULL;
	sxml_node_t* node = sxml_node_item_new();
	if(!node)return NULL;
	node->name = (char*)sxml_alloc(strlen(name)+1);
	if(!node->name){ sxml_free(node); return NULL; }
	memset(node->name,0,sizeof(strlen(name)+1));
	node->type = type;
	if(NULL != name)sprintf(node->name,"%s",name);
	node->parent = NULL;
	node->data = NULL;
	node->indent = 0;
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
	memset(node->data, 0, len+1);
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
	memset(node->data, 0, len+1);
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
	memset(datap,0,sizeof(sxml_data_t));
	datap->size = size;
	datap->data = sxml_alloc(size+1);
	if(!datap->data)
	{
		sxml_free(datap);
		return NULL;
	}
	memset(datap->data, 0, size+1);
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
	memset(attr,0,sizeof(sxml_attr_t));
	return attr;
}

XEXPORT XAPI sxml_attr_t* sxml_attr_new(const char* name, const char* value)
{
	if(!name)return NULL;
	sxml_attr_t* attr = sxml_attr_item_new();
	if(!attr)return NULL;
	attr->name = (char*)sxml_alloc(strlen(name)+1);
	if(!attr->name){ sxml_free(attr); return NULL; }
	memset(attr->name,0,sizeof(strlen(name)+1));
	attr->value = (char*)sxml_alloc(strlen(value)+1);
	if(!attr->value){ sxml_free(attr->name); sxml_free(attr); return NULL; }
	memset(attr->value,0,sizeof(strlen(value)+1));
	
	if(!strncmp(name,"xmlns:",6))attr->type = 2;
	if(NULL != name)sprintf(attr->name,"%s",name);
	if(NULL != value)sprintf(attr->value,"%s",value);
	QUEUE_INIT(&attr->aq);
	return attr;
}

XEXPORT XAPI void sxml_attr_free(sxml_attr_t* attr)
{
	if(!attr) return;
	sxml_free(attr->name);
	sxml_free(attr->value);
	sxml_free(attr);
}

XEXPORT XAPI void sxml_node_free(sxml_node_t* node)
{
	QUEUE* q;
	sxml_attr_t* attr;
	sxml_node_t* child;
	if(!node) return;
	//先释放子节点和属性
	QUEUE_FOREACH(q, &node->attrs)
	{
		attr = (sxml_attr_t*)QUEUE_DATA(q,sxml_attr_t,aq);
		sxml_attr_free(attr);
	}
	QUEUE_FOREACH(q, &node->children)
	{
		child = (sxml_node_t*)QUEUE_DATA(q,sxml_node_t,nq);
		sxml_node_free(child);
	}
	//再释放自己
	if(node->name)
	{
		sxml_free(node->name);
	}
	if(node->data)
	{
		sxml_free(node->data);
	}
	sxml_free(node);
}

XEXPORT XAPI void sxml_doc_free(sxml_doc_t* doc)
{
	QUEUE* q;
	sxml_node_t* node;
	if(!doc) return;
	//先释放节点
	QUEUE_FOREACH(q, &doc->dq)
	{
		node = (sxml_node_t*)QUEUE_DATA(q,sxml_node_t,nq);
		sxml_node_free(node);
	}
	//再释放doc
	sxml_free(doc);
}

XEXPORT XAPI int sxml_add_attr2node(sxml_node_t* node, sxml_attr_t* attr)
{
	if(!attr)return -1;
	QUEUE_INSERT_TAIL(&node->attrs,&attr->aq);
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
	if(!node || !child)return -1;
	QUEUE_INSERT_TAIL(&node->children,&child->nq);
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
		switch(node->type)
		{
			case 0: 
				needed = strlen(node->name)+2;
				str = string_ensure(p, needed);
				sprintf(str, "<%s", node->name); p->offset = string_update(p); if(!ret) ret = str; 
				sxml_attr_print(node, p);
				needed = 3; str = string_ensure(p, 3); str[0] = '>'; str[1] = '\n'; str[2] = '\0'; p->offset = string_update(p); 
				QUEUE_FOREACH(q, &node->children)
				{
					child = (sxml_node_t*)QUEUE_DATA(q,sxml_node_t,nq);
					sxml_node_print(child, p);
				}
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
				needed = strlen(node->name)+5; str = string_ensure(p, needed); sprintf(str, "</%s>\n", node->name); p->offset = string_update(p); 		
				break;//普通节点，节点头左+节点属性+节点头右+子节点+节点尾
			case 1: 
				needed = strlen((char*)node->data)+9; str = string_ensure(p, needed); sprintf(str, "<!--%s-->\n", (char*)node->data); p->offset = string_update(p); if(!ret) ret = str; 
				break;//注释节点，<!--+注释+-->
			case 2: 
				needed = strlen((char*)node->data)+2; str = string_ensure(p, needed); sprintf(str, "%s\n", (char*)node->data); p->offset = string_update(p); if(!ret) ret = str; 
				break;//内嵌文本，内容
			case 3: 
				data = (sxml_data_t*)node->data;
				if(!strcmp(node->name,"#rawdata"))
				{
					needed = data->size+14; str = string_ensure(p, needed); sprintf(str, "<![CDATA[%s]]>\n", (char*)data->data); p->offset = string_update(p); 
				}else
				{
					if(node->indent)
					{
						needed = data->size+4+2*strlen(node->name); str = string_ensure(p, needed); sprintf(str, "<%s>\n", node->name); p->offset = string_update(p); 
						
						//needed = node->indent*SXML_INDENT_COUNT+SXML_INDENT_COUNT; str = string_ensure(p, needed+1);
						//for(i = 0; i < needed; i++)
						//{
						//	str[i] = ' ';
						//}
						//str[i] = '\0';p->offset = string_update(p);					
						needed = data->size+2+2*strlen(node->name); str = string_ensure(p, needed); sprintf(str, "%s\n", (char*)data->data); p->offset = string_update(p); 
						
						needed = node->indent*SXML_INDENT_COUNT; str = string_ensure(p, needed+1);
						for(i = 0; i < needed; i++)
						{
							str[i] = ' ';
						}
						str[i] = '\0'; p->offset = string_update(p);
						needed = data->size+9+2*strlen(node->name); str = string_ensure(p, needed); sprintf(str, "</%s>\n", node->name); p->offset = string_update(p); 
					}else
					{
						needed = data->size+9+2*strlen(node->name); str = string_ensure(p, needed); sprintf(str, "<%s>\n%s\n</%s>\n", node->name, (char*)data->data, node->name); p->offset = string_update(p); 
					}
				}
				if(!ret) ret = str; 
				break;//原始数据，<![CDATA[+原始数据+]]>
			case 4: 
				needed = strlen((char*)node->name)+5; str = string_ensure(p, needed); sprintf(str, "<%s/>\n", (char*)node->name); p->offset = string_update(p); if(!ret) ret = str; 
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
				entries[i++] = sxml_attr_print(node, p); 
				QUEUE_FOREACH(q, &node->children)
				{
					child = (sxml_node_t*)QUEUE_DATA(q,sxml_node_t,nq);
					entries[i++] = sxml_node_print(child, p);
				}
				for(i = 0; i < numentries; i++)
				{
					needed += strlen(entries[i]);
				}
				needed += strlen(node->name)*2 + 8;	
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
				//前缩进
				if(node->indent)
				{
					for(i = 0; i < indent; i++)
					{
						*tmp = ' '; ++tmp;
					}
				}
				tmp += sprintf(tmp, "<%s%s>\n", node->name, entries[0]);
				for(i = 1; i < numentries; i++)
				{
					tmp += sprintf(tmp, "%s", entries[i]);
				}
				//后缩进
				if(node->indent)
				{
					for(i = 0; i < indent; i++)
					{
						*tmp = ' '; ++tmp;
					}
				}
				sprintf(tmp, "</%s>\n", node->name);
				//释放没用的内存
				for(i = 0; i < numentries; i++)
				{
					sxml_free(entries[i]);
				}
				sxml_free((void*)entries);
				break;//普通节点，节点头左+节点属性+节点头右+子节点+节点尾
			case 1: 
				needed = strlen((char*)node->data)+9; 
				indent = (node->indent)?node->indent*SXML_INDENT_COUNT:0; 
				needed += indent; 
				ret = (char*)sxml_alloc(needed); 
				if(!ret) return NULL;
				tmp = ret;
				if(node->indent)
				{
					for(i = 0; i < indent; i++)
					{
						*tmp = ' '; ++tmp;
					}
				}
				sprintf(tmp, "<!--%s-->\n", (char*)node->data); 
				break;//注释节点，<!--+注释+-->
			case 2: 
				needed = strlen((char*)node->data)+2; 
				indent = (node->indent)?node->indent*SXML_INDENT_COUNT:0; 
				needed += indent; 
				ret = (char*)sxml_alloc(needed); if(!ret) return NULL; 
				tmp = ret;
				if(node->indent)
				{
					for(i = 0; i < indent; i++)
					{
						*tmp = ' '; ++tmp;
					}
				}
				sprintf(tmp, "%s\n", (char*)node->data);
				break;//内嵌文本，内容
			case 3: 
				data = (sxml_data_t*)node->data;
				if(!strcmp(node->name,"#rawdata"))
				{
					needed = data->size+14; 
					indent = (node->indent)?node->indent*SXML_INDENT_COUNT:0; 
					needed += indent; 
					ret = (char*)sxml_alloc(needed); if(!ret) return NULL; 
					tmp = ret;
					if(node->indent)
					{
						for(i = 0; i < indent; i++)
						{
							*tmp = ' '; ++tmp;
						}
					}
					sprintf(tmp, "<![CDATA[%s]]>\n", (char*)data->data); 
				}else
				{
					needed = data->size+9+2*strlen(node->name); 
					indent = (node->indent)?node->indent*SXML_INDENT_COUNT:0; 
					needed += indent*3+SXML_INDENT_COUNT; 
					ret = (char*)sxml_alloc(needed); if(!ret) return NULL; 
					tmp = ret;
					if(node->indent)
					{
						for(i = 0; i < indent; i++)
						{
							*tmp = ' '; ++tmp;
						}
						tmp += sprintf(tmp, "<%s>\n", node->name);
						//for(i = 0; i < indent+SXML_INDENT_COUNT; i++)
						//{
						//	*tmp = ' '; ++tmp;
						//}
						tmp += sprintf(tmp, "%s\n", (char*)data->data);
						for(i = 0; i < indent; i++)
						{
							*tmp = ' '; ++tmp;
						}
						tmp += sprintf(tmp, "</%s>\n", node->name);
					}else
					{
						tmp += sprintf(tmp, "<%s>\n%s\n</%s>\n", node->name, (char*)data->data, node->name);
					}
				}
				break;//原始数据，<![CDATA[+原始数据+]]>
			case 4: 
				needed = strlen((char*)node->name)+5; 
				indent = (node->indent)?node->indent*SXML_INDENT_COUNT:0; 
				needed += indent; 
				ret = (char*)sxml_alloc(needed); if(!ret) return NULL; 
				tmp = ret;
				if(node->indent)
				{
					for(i = 0; i < indent; i++)
					{
						*tmp = ' '; ++tmp;
					}
				}
				sprintf(tmp, "<%s/>\n", (char*)node->name); 
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
	p.buffer=(char*)sxml_alloc(size);
	if(!p.buffer) return NULL;
	p.length=size;
	p.offset=0;
	needed = 27+16;
	str = string_ensure(&p, needed);
	sprintf(str, "<?xml version=\"%s\" encoding=\"%s\"?>\n", doc->version, doc->charset);
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
	tmp += sprintf(tmp, "<?xml version=\"%s\" encoding=\"%s\"?>\n", doc->version, doc->charset);
	
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


XEXPORT XAPI char* sxml_attr_parse(sxml_node_t* node, char* value)
{
	
}

XEXPORT XAPI char* sxml_node_parse(sxml_node_t* node, char* value)
{
	
	return NULL;
}


//yml找换行,这里找结束节点,自定义标签
XEXPORT XAPI char* sxml_head_parse(sxml_node_t* node, char* value)
{
	
	return NULL;
}


XEXPORT XAPI sxml_doc_t* sxml_doc_parse(const char* filename, const char* value)
{
	char* ret;
	long long len;
	sxml_node_t* node=NULL;
	sxml_doc_t* doc=sxml_doc_item_new();
	if(!doc) return NULL;
	len = strlen(filename);
	doc->filename = sxml_alloc(len+1);
	if(!doc->filename)
	{
		sxml_free(doc);
		return NULL;
	}
	sprintf(doc->filename, "%s", filename);
	ret = sxml_head_parse(doc, value);
	if(!ret)
	{
		sxml_free(doc->filename);
		sxml_free(doc);
		return NULL;
	}
	while(!ret)
	{
		node = sxml_node_item_new();
		ret = sxml_node_parse(node, ret);
		if(!ret)
		{
			sxml_free(node);
		}
	}
	if(QUEUE_ISEMPTY(doc->dq))
	{
		sxml_free(doc->filename);
		sxml_free(doc);
		return NULL;
	}
	return doc;
}

XEXPORT XAPI sxml_doc_t* sxml_parse(const char* filename)
{
	sxml_doc_t* doc=NULL;
	FILE *f;long len;char *data;	
	f=fopen(filename,"rb");fseek(f,0,SEEK_END);len=ftell(f);fseek(f,0,SEEK_SET);
	data=(char*)sxml_alloc(XALIGN(len+1,4));fread(data,1,len,f);fclose(f);
	doc=sxml_doc_parse(filename, data);
	sxml_free(data);
	if (!doc) 
	{
		printf("Failed to parse file: %s, error:[%s]\n",filename, sxml_error_pick());
	}
	return doc;
}














