#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <float.h>
#include <limits.h>
#include <ctype.h>

#include "sxml.h"

//����������ڵ���չ��#name,��Ӧ��ʼ�����ڵ��־�������ļ�������չ�����ⲿ�Լ�ʵ�֣�
//�û��Զ�����ԭʼ���ݵ����𡣣���ɣ�
//����OK��ת�塢ԭʼ���ݿ�ʼ��������ɣ�
//������μ�飨��ɣ�


//ת���ַ�����ȣ�����ɣ�
//itemΪ���ýڵ�
#define SXML_IS_REFERENCE 		128
//�ڵ�����Ϊ����
#define SXML_IS_STR_CONST		512


// Private macros
#define QUEUE_NEXT(q)       		(*(QUEUE **) &((*(q))[0]))
#define QUEUE_PREV(q)       		(*(QUEUE **) &((*(q))[1]))
#define QUEUE_PREV_NEXT(q)  		(QUEUE_NEXT(QUEUE_PREV(q)))
#define QUEUE_NEXT_PREV(q)  		(QUEUE_PREV(QUEUE_NEXT(q)))

//���ݽṹ���Ա������ȡ�ṹ���ַ
#define QUEUE_DATA(ptr, type, field)                                          		\
  ((type *) ((char *) (ptr) - ((char *) &((type *) 0)->field)))

//ѭ������hΪ����ͷ,q���α������е�ÿһ����Ա
#define QUEUE_FOREACH(q, h)                                                   		\
  for ((q) = QUEUE_NEXT(h); (q) != (h); (q) = QUEUE_NEXT(q))

//�ж϶����Ƿ�Ϊ��
#define QUEUE_ISEMPTY(q)                                                        	\
  ((const QUEUE *) (q) == (const QUEUE *) QUEUE_NEXT(q))

//����ͷ����ַ
#define QUEUE_HEAD(q)                                                         		\
  (QUEUE_NEXT(q))
  
//����β����ַ
#define QUEUE_TAIL(q)                                                         		\
  (QUEUE_PREV(q))

//���г�ʼ��
#define QUEUE_INIT(q)                                                         		\
  do {                                                                        		\
    QUEUE_NEXT(q) = (q);                                                      		\
    QUEUE_PREV(q) = (q);                                                      		\
  }                                                                           		\
  while (0)

//���кϲ�
#define QUEUE_ADD(h, n)                                                       		\
  do {                                                                        		\
    QUEUE_PREV_NEXT(h) = QUEUE_NEXT(n);                                       		\
    QUEUE_NEXT_PREV(n) = QUEUE_PREV(h);                                       		\
    QUEUE_PREV(h) = QUEUE_PREV(n);                                            		\
    QUEUE_PREV_NEXT(h) = (h);                                                 		\
  }                                                                           		\
  while (0)

//���зָ�
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

//head�ݴ����ͷ��β�ڵ��ַ,���м�ת��
//����ǰ��
#define QUEUE_INSERT_HEAD(h, q)                                               		\
  do {                                                                        		\
    QUEUE_NEXT(q) = QUEUE_NEXT(h);                                            		\
    QUEUE_PREV(q) = (h);                                                      		\
    QUEUE_NEXT_PREV(q) = (q);                                                 		\
    QUEUE_NEXT(h) = (q);                                                      		\
  }                                                                           		\
  while (0)

//���к��
#define QUEUE_INSERT_TAIL(h, q)                                               		\
  do {                                                                        		\
    QUEUE_NEXT(q) = (h);                                                      		\
    QUEUE_PREV(q) = QUEUE_PREV(h);                                            		\
    QUEUE_PREV_NEXT(q) = (q);                                                 		\
    QUEUE_PREV(h) = (q);                                                      		\
  }                                                                           		\
  while (0)

//�ѵ�ǰ�ڵ�������ڵĶ�����ɾ��
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
//�����հ��ַ�
XSTATIC const char *skip(const char *in) {while (in && *in && (unsigned char)*in<=32) in++; return in;}
//ѡ���������հ��ַ�
XSTATIC const char *pskip(const char *in) { const char* p=skip(in); return (*p == '<')?p:in; }
//�����ض��ַ���
XSTATIC const char* check_skip(const char* in, const char* str){ const char* start; long long len=strlen(str); if(start = strstr(in, str)){return start+len;}else{return NULL;}} 
//����Ƿ�����ַ���
XSTATIC long long check(const char* in, const char* str){ return strstr(in, str)?1:0;} 
//�������ַ���Ϊֹ
XSTATIC const char* copy_until(char* to, const char* from, const char* flag){ const char* pstr; const char* start; long long len=strlen(flag); if(start = strstr(from, flag)){ pstr = from; while(pstr < start)*to = *pstr, pstr++, to++; return start+len;}else{return NULL;}} 
//������һ��λ��
XSTATIC const char* skip_line(const char *in){while (in && *in && *in != '\n')in++; return (in && *in == '\n')?++in:NULL;}

XSTATIC char* trim(char *in){char* start; char* pstr; start = (char*)skip(in); pstr = start; while(pstr && *pstr != '\0')pstr++; if(!pstr)return NULL; while(*pstr <= 32){ *pstr = '\0'; pstr--; } return start; }


XSTATIC void*do_alloc(size_t size)
{
	void* retptr=NULL;
	if(size < 1) return NULL;
	size_t real_size = XALIGN(size, 8);
	retptr = malloc(real_size);
	if(!retptr)return NULL;
	memset(retptr, 0, real_size);
	return retptr;
}
XSTATIC const char* string_index_of_any(const char* str, const char* cs)
{
	long long i,j,len,count;
	if(!str || !cs) return NULL;
	len = strlen(str);
	count = strlen(cs);
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
	  if(!str) return NULL;
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
	if(!value) return NULL;
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
	g_sxml_parse_error = info;//���浽ȫ�ֱ�����
	return info;
}

XEXPORT XAPI void sxml_free_file_info(sxml_file_info_t** info)
{
	if(!info || !(*info)) return;
	sxml_free((*info)->line_info);
	sxml_free(*info);
	*info = NULL;
}

XEXPORT XAPI void sxml_print_file_info(sxml_file_info_t* info)
{
	long long i;
	if(!info) return;
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
	if(!charset)return NULL;//charset����Ϊ��
	sxml_doc_t* doc = sxml_doc_item_new();
	if(!doc)return NULL;//�ڴ�����ʧ��
	
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

XEXPORT XAPI sxml_node_t* sxml_node_type_new(long long type, const char* name, const char* append)
{
	if(!name)return NULL;
	sxml_node_t* node = sxml_node_item_new();
	if(!node)return NULL;
	node->name = (char*)sxml_alloc(strlen(name)+1);
	if(!node->name){ sxml_free(node); return NULL; }
	node->type = type;
	if(SXML_USERDEF == node->type)
	{
		if(append)
		{
			node->append = (char*)sxml_alloc(strlen(append)+1);
			if(!node->append){ sxml_free(node->name); sxml_free(node); return NULL; }
			sprintf(node->append,"%s",append);
		}else
		{ 
			node->append = NULL;
		}
	}else
	{
		node->append = NULL;
	}
	sprintf(node->name,"%s",name);
	//printf("node->name malloc: %p, %s\n", node->name, node->name);
	node->parent = NULL;
	node->data = NULL;
	node->indent = 0;
	node->index = 0;
	node->childCount = 0;
	node->attrCount = 0;
	node->prevSibling = NULL;
	node->nextSibling = NULL;
	QUEUE_INIT(&node->children);
	QUEUE_INIT(&node->attrs);
	QUEUE_INIT(&node->nq);
	return node;
}

XEXPORT XAPI sxml_node_t* sxml_node_new(const char* name)
{
	if(!name) return NULL;
	return sxml_node_type_new(SXML_NORMAL,name,NULL);
}

XEXPORT XAPI sxml_node_t* sxml_comment_new(const char* comment)
{
	long long len;
	sxml_node_t* node;
	if(!comment) return NULL;
	len = strlen(comment);
	node = sxml_node_type_new(SXML_COMMENT,"#comment",NULL);
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
	long long len;
	sxml_node_t* node;
	if(!innertext) return NULL;
    len	= strlen(innertext);
	node = sxml_node_type_new(SXML_INNERTEXT,"#innertext",NULL);
	node->data = (char*)sxml_alloc(len+1);
	if(!node->data)
	{
		sxml_node_free(node);
		return NULL;
	}
	memcpy(node->data, innertext, len);
	return node;
}

//nameΪNULL����Ĭ�Ͻڵ㣬��������Զ���ڵ�
XEXPORT XAPI sxml_node_t* sxml_rawdata_new(const void* data, long long size)
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
	node = sxml_node_type_new(SXML_RAWDATA,"#rawdata",NULL);	
	if(!node)
	{
		sxml_free(datap);return NULL;
	}
	node->data = datap;
	return node;
}

XEXPORT XAPI sxml_node_t* sxml_userdef_new(const char* start, const char* end, const void* data, long long size)
{
	sxml_node_t* node; 
	if(!start || !data || !size)
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
	node = sxml_node_type_new(SXML_USERDEF, start, end);
	if(!node)
	{
		sxml_free(datap);return NULL;
	}
	node->data = datap;
	return node;
}

XSTATIC sxml_attr_t* sxml_attr_item_new()
{
	sxml_attr_t* attr = (sxml_attr_t*)sxml_alloc(sizeof(sxml_attr_t));
	if(!attr)return NULL;
	QUEUE_INIT(&attr->aq);
	attr->prevSibling = NULL;
	attr->nextSibling = NULL;
	attr->owner = NULL;
	attr->index = 0;
	return attr;
}

XEXPORT XAPI sxml_attr_t* sxml_attr_new(const char* name, const char* value)
{
	if(!name || !value)return NULL;
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

XEXPORT XAPI sxml_alias_t* sxml_alias_item_new()
{
	sxml_alias_t* alias = (sxml_alias_t*)sxml_alloc(sizeof(sxml_alias_t));
	if(!alias)return NULL;
	QUEUE_INIT(&alias->aq);
	alias->alias = NULL;
	alias->append = NULL;
	return alias;
}

XEXPORT XAPI sxml_alias_t* sxml_alias_new(char* name, char* append)
{
	long long len;
	sxml_alias_t* alias;
	if(!name)return NULL;
	len = strlen(name);
	alias = sxml_alias_item_new();
	alias->alias = (char*)sxml_alloc(len+1);
	if(!alias->alias)
	{
		sxml_free(alias);
		return NULL;
	}
	memcpy(alias->alias, name, len);
	if(append)
	{
		len = strlen(append);
		alias->append = (char*)sxml_alloc(len+1);
		if(!alias->append)
		{
			sxml_free(alias->alias);
			sxml_free(alias);
			return NULL;
		}
		memcpy(alias->append, append, len);
		alias->type = 1;
	}else
	{
		alias->type = 0;
	}
	return alias;
}

XEXPORT XAPI void sxml_alias_free(sxml_alias_t* alias)
{
	if(!alias) return;
	if(!alias->alias)sxml_free(alias->alias);
	sxml_free(alias);
}

XEXPORT XAPI sxml_parser_t* sxml_parser_new()
{
	sxml_parser_t* parser = (sxml_parser_t*)sxml_alloc(sizeof(sxml_parser_t));
	if(!parser)return NULL;
	QUEUE_INIT(&parser->normal);
	QUEUE_INIT(&parser->special);
	return parser;
}

XEXPORT XAPI void sxml_parser_free(sxml_parser_t* parser)
{
	if(!parser) return;
	sxml_free(parser);
}


XEXPORT XAPI long long int sxml_add_alias2parser(sxml_parser_t* parser, sxml_alias_t* alias)
{
	if(!parser)return -1;
	if(!alias)return -1;
	switch(alias->type)
	{
		case 0:QUEUE_INSERT_TAIL(&parser->normal, &alias->aq);break;
		case 1:QUEUE_INSERT_TAIL(&parser->special, &alias->aq);break;
		default:QUEUE_INSERT_TAIL(&parser->normal, &alias->aq);break;
	}
}

XEXPORT XAPI long long int sxml_del_parser4alias(sxml_parser_t* parser, char* name)
{
	sxml_alias_t* alias;
	QUEUE* q;
	if(!parser || !name) return -1;
	QUEUE_FOREACH(q, &parser->normal)
	{
		alias = (sxml_alias_t*)QUEUE_DATA(q,sxml_alias_t,aq);
		if(!strcmp(alias->alias, name))
		{
			QUEUE_REMOVE(q);
			sxml_alias_free(alias);
			return 0;
		}
	}
	QUEUE_FOREACH(q, &parser->special)
	{
		alias = (sxml_alias_t*)QUEUE_DATA(q,sxml_alias_t,aq);
		if(!strcmp(alias->alias, name) || !strcmp(alias->append, name))
		{
			QUEUE_REMOVE(q);
			sxml_alias_free(alias);
			return 0;
		}
	}
	return -1;
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
	if(!node) return;
	while(!QUEUE_ISEMPTY(&node->attrs))
	{
		QUEUE_FOREACH(q, &node->attrs)
		{
			QUEUE_REMOVE(q);
			attr = (sxml_attr_t*)QUEUE_DATA(q,sxml_attr_t,aq);
			sxml_attr_free(attr);
			break; //��ͷ��ʼ����
		}
	}
}

XEXPORT XAPI void sxml_node_free(sxml_node_t* node)
{
	QUEUE* q;
	sxml_attr_t* attr;
	sxml_node_t* child;
	if(!node) return;
	//���ͷ��ӽڵ������
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
	
	//���ͷ��Լ�
	if(node->name)
	{
		//printf("node->name free: %p, %s\n",node->name, node->name);
		sxml_free(node->name);
	}
	if(node->append)
	{
		sxml_free(node->append);
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
	if(!node) return;
	while(!QUEUE_ISEMPTY(&node->children))
	{
		QUEUE_FOREACH(q, &node->children)
		{
			QUEUE_REMOVE(q);
			child = (sxml_node_t*)QUEUE_DATA(q,sxml_node_t,nq);
			sxml_node_free(child);
			break; //��ͷ��ʼ����
		}
	}
}

XEXPORT XAPI void sxml_doc_free(sxml_doc_t* doc)
{
	QUEUE* q;
	sxml_node_t* node;
	if(!doc) return;
	//���ͷŽڵ�
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
	
	//���ͷ�doc
	sxml_free(doc);
}

XEXPORT XAPI int sxml_add_attr2node(sxml_node_t* node, sxml_attr_t* attr)
{
	QUEUE* q;
	sxml_attr_t* tail;
	if(!attr || !node)return -1;
	
	if(QUEUE_ISEMPTY(&node->attrs))
	{
		QUEUE_INSERT_TAIL(&node->attrs,&attr->aq);
		attr->prevSibling = NULL;
		attr->nextSibling = NULL;
		attr->index = 1;
	}else
	{
		q = QUEUE_TAIL(&node->attrs);
		tail = (sxml_attr_t*)QUEUE_DATA(q,sxml_attr_t,aq);
		QUEUE_INSERT_TAIL(&node->attrs,&attr->aq);
		tail->nextSibling = attr;
		attr->prevSibling = tail;
		attr->nextSibling = NULL;
		attr->index = tail->index + 1;
	}
	attr->owner = node;
	node->attrCount++;	
	return 0;
}

XEXPORT XAPI int sxml_del_node4attr(sxml_node_t* node, char* name)
{
	QUEUE* q;
	sxml_attr_t* attr;
	sxml_attr_t* del;
	if(!name || !node)return -1;
	
	if(QUEUE_ISEMPTY(&node->attrs))
	{
		return 0;
	}else
	{
		QUEUE_FOREACH(q, &node->attrs)
		{
			attr = (sxml_attr_t*)QUEUE_DATA(q,sxml_attr_t,aq);
			if(!strcmp(attr->name, name))
			{
				del = attr;
				attr->prevSibling->nextSibling = attr->nextSibling;
				if(attr->nextSibling)
				{
					attr->nextSibling->prevSibling = attr->prevSibling;
				}
				QUEUE_REMOVE(q);
				while(NULL != attr->nextSibling)
				{
					attr->nextSibling->index--;
					attr = attr->nextSibling;
				}
				sxml_attr_free(del);
				return 0;
			}
		}		
	}
	return -1;
}

XEXPORT XAPI int sxml_add_node2doc(sxml_doc_t* doc, sxml_node_t* node)
{
	if(!doc || !node)return -1;
	QUEUE_INSERT_TAIL(&doc->dq,&node->nq);
	node->parent = NULL;
	return 0;
}

XEXPORT XAPI int sxml_del_doc4node(sxml_doc_t* doc, char* name)
{
	QUEUE* q;
	sxml_node_t* node;
	sxml_node_t* del;
	if(!name || !doc)return -1;
	
	if(QUEUE_ISEMPTY(&doc->dq))
	{
		return 0;
	}else
	{
		QUEUE_FOREACH(q, &doc->dq)
		{
			node = (sxml_node_t*)QUEUE_DATA(q,sxml_node_t,nq);
			if(!strcmp(node->name, name))
			{
				del = node;
				node->prevSibling->nextSibling = node->nextSibling;
				if(node->nextSibling)
				{
					node->nextSibling->prevSibling = node->prevSibling;
				}
				QUEUE_REMOVE(q);
				while(NULL != node->nextSibling)
				{
					node->nextSibling->index--;
					node = node->nextSibling;
				}
				sxml_node_free(del);
				return 0;
			}
		}		
	}
	return -1;
}

XEXPORT XAPI int sxml_add_subnode2node(sxml_node_t* node, sxml_node_t* child)
{
	QUEUE* q;
	sxml_node_t* tail;
	if(!node || !child)return -1;
	if(QUEUE_ISEMPTY(&node->children))
	{
		QUEUE_INSERT_TAIL(&node->children,&child->nq);
		child->prevSibling = NULL;
		child->nextSibling = NULL;
		child->index = 1;
	}else
	{
		q = QUEUE_TAIL(&node->children);
		tail = (sxml_node_t*)QUEUE_DATA(q,sxml_node_t,nq);
		QUEUE_INSERT_TAIL(&node->children,&child->nq);
		tail->nextSibling = child;
		child->prevSibling = tail;
		child->nextSibling = NULL;
		child->index = tail->index + 1;
	}
	node->childCount++;
	
	child->parent = node;
	child->indent = node->indent + 1;
	
	return 0;
}

XEXPORT XAPI int sxml_del_node4subnode(sxml_node_t* node, char* name)
{
	QUEUE* q;
	sxml_node_t* subnode;
	sxml_node_t* del;
	if(!name || !node)return -1;
	
	if(QUEUE_ISEMPTY(&node->children))
	{
		return 0;
	}else
	{
		QUEUE_FOREACH(q, &node->children)
		{
			subnode = (sxml_node_t*)QUEUE_DATA(q,sxml_node_t,nq);
			if(!strcmp(subnode->name, name))
			{
				del = subnode;
				subnode->prevSibling->nextSibling = subnode->nextSibling;
				if(subnode->nextSibling)
				{
					subnode->nextSibling->prevSibling = subnode->prevSibling;
				}
				QUEUE_REMOVE(q);
				while(NULL != subnode->nextSibling)
				{
					subnode->nextSibling->index--;
					subnode = subnode->nextSibling;
				}
				sxml_node_free(del);
				return 0;
			}
		}		
	}
	return -1;
}

XEXPORT XAPI char* sxml_attr_print(sxml_node_t* node, sxml_buffer_ht p)
{
	QUEUE* q;
	sxml_attr_t* attr;
	char* tmp=NULL;
	char* ret=NULL;
	int needed=0,numentries=0,i=0;
	if(!node) return NULL;
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
	if(!node)
	{
		return NULL;
	}
	if(p)
	{
		switch(node->type)
		{
			case 0: 
				if(!node->prevSibling || 2 != node->prevSibling->type)
				{
					str = string_ensure(p, 2); str[0] = '\n'; str[1] = '\0'; p->offset = string_update(p);//��ͨ�ڵ㣬��Ҫ����
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
					str = string_ensure(p, 2); str[0] = '>'; str[1] = '\0'; p->offset = string_update(p); //����ȡ�����ӽڵ����ͣ���Ӧ�������ﻻ��str[2] = '\0';,�ӽڵ㲻ΪInnertext����
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
				break;//��ͨ�ڵ㣬�ڵ�ͷ��+�ڵ�����+�ڵ�ͷ��+�ӽڵ�+�ڵ�β
			case 1:
				if(!node->prevSibling || 2 != node->prevSibling->type)
				{
					str = string_ensure(p, 2); str[0] = '\n'; str[1] = '\0'; p->offset = string_update(p);//ע�ͽڵ㣬��Ҫ����
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
				break;//ע�ͽڵ㣬<!--+ע��+-->
			case 2: //��Ƕ�ı���ʲô������Ҫ��
				needed = strlen((char*)node->data)+1; str = string_ensure(p, needed); sprintf(str, "%s", (char*)node->data); p->offset = string_update(p); if(!ret) ret = str; 
				break;//��Ƕ�ı�������
			case 3: 
				if(!node->prevSibling || 2 != node->prevSibling->type)
				{
					str = string_ensure(p, 2); str[0] = '\n'; str[1] = '\0'; p->offset = string_update(p);//ԭʼ���ݽڵ㣬��Ҫ����
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
				//if(!strcmp(node->name,"#rawdata"))
				//{
					needed = data->size+13; str = string_ensure(p, needed); sprintf(str, "<![CDATA[%s]]>", (char*)data->data); p->offset = string_update(p); 
				//}else
				//{
				//	if(node->indent)
				//	{
				//		needed = data->size+3+2*strlen(node->name); str = string_ensure(p, needed); sprintf(str, "<%s>", node->name); p->offset = string_update(p); 
				//		
				//		//needed = node->indent*SXML_INDENT_COUNT+SXML_INDENT_COUNT; str = string_ensure(p, needed+1);
				//		//for(i = 0; i < needed; i++)
				//		//{
				//		//	str[i] = ' ';
				//		//}
				//		//str[i] = '\0';p->offset = string_update(p);					
				//		needed = data->size+1+2*strlen(node->name); str = string_ensure(p, needed); sprintf(str, "%s", (char*)data->data); p->offset = string_update(p); 
				//		
				//		//needed = node->indent*SXML_INDENT_COUNT; str = string_ensure(p, needed+1);
				//		//for(i = 0; i < needed; i++)
				//		//{
				//		//	str[i] = ' ';
				//		//}
				//		//str[i] = '\0'; p->offset = string_update(p);
				//		
				//		needed = data->size+4+2*strlen(node->name); str = string_ensure(p, needed); sprintf(str, "</%s>", node->name); p->offset = string_update(p); 
				//	}else
				//	{
				//		needed = data->size+6+2*strlen(node->name); str = string_ensure(p, needed); sprintf(str, "<%s>%s</%s>", node->name, (char*)data->data, node->name); p->offset = string_update(p); 
				//	}
				//}
				if(!ret) ret = str; 
				break;//ԭʼ���ݣ�<![CDATA[+ԭʼ����+]]>
			case 4://�û��Զ���ڵ�û���ӽڵ�
				if(!node->prevSibling || 2 != node->prevSibling->type)
				{
					str = string_ensure(p, 2); str[0] = '\n'; str[1] = '\0'; p->offset = string_update(p);//��ͨ�ڵ㣬��Ҫ����
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
				if(node->append)
				{
					needed = data->size+strlen(node->name)+strlen(node->append)+1; str = string_ensure(p, needed); sprintf(str, "%s%s%s", node->name, (char*)data->data, node->append);
				}else
				{
					needed = data->size+strlen(node->name)*2+6; str = string_ensure(p, needed); sprintf(str, "<%s>%s</%s>", node->name, (char*)data->data, node->name);
				} 
				p->offset = string_update(p); 
				if(!ret) ret = str; 
				break;//�û��Զ���ڵ�	
			//case 4: 
			//	if(!node->prevSibling || 2 != node->prevSibling->type)
			//	{
			//		str = string_ensure(p, 2); str[0] = '\n'; str[1] = '\0'; p->offset = string_update(p);//�սڵ㣬��Ҫ����
			//		if(node->indent)
			//		{
			//			needed = node->indent*SXML_INDENT_COUNT;
			//			str = string_ensure(p, needed+1);
			//			for(i = 0; i < needed; i++)
			//			{
			//				str[i] = ' ';
			//			}
			//			str[i] = '\0';
			//			p->offset = string_update(p); ret = str; 
			//		}
			//	}
			//	needed = strlen((char*)node->name)+6; str = string_ensure(p, needed); sprintf(str, "<%s />", (char*)node->name); p->offset = string_update(p); if(!ret) ret = str; 
			//	break;//���ýڵ㣬�ڵ�
		}
	}else
	{
		switch(node->type)
		{
			case 0: 
				//���㳤��
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
				if(!node->prevSibling || 2 != node->prevSibling->type)
				{
					*tmp = '\n'; ++tmp;
					//ǰ����
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
						tmp += sprintf(tmp, "<%s%s>", node->name, entries[0]);//���в������ﴦ��
						for(i = 1; i < numentries; i++)
						{
							tmp += sprintf(tmp, "%s", entries[i]);
						}
					}else
					{
						tmp += sprintf(tmp, "<%s>", node->name);//���в������ﴦ��
						for(i = 0; i < numentries; i++)
						{
							tmp += sprintf(tmp, "%s", entries[i]);
						}
					}
					
					if(QUEUE_ISEMPTY(&node->children) || (!QUEUE_ISEMPTY(&node->children) && 2 != child->type))
					{
						*tmp = '\n'; ++tmp;
						//������
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
				//�ͷ�û�õ��ڴ�
				for(i = 0; i < numentries; i++)
				{
					sxml_free(entries[i]);
				}
				sxml_free(entries);
				break;//��ͨ�ڵ㣬�ڵ�ͷ��+�ڵ�����+�ڵ�ͷ��+�ӽڵ�+�ڵ�β
			case 1: 
				needed = strlen((char*)node->data)+10; 
				indent = (node->indent)?node->indent*SXML_INDENT_COUNT:0; 
				needed += indent; 
				ret = (char*)sxml_alloc(needed); 
				if(!ret) return NULL;
				tmp = ret;
				if(!node->prevSibling || 2 != node->prevSibling->type)
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
				break;//ע�ͽڵ㣬<!--+ע��+-->
			case 2: 
				needed = strlen((char*)node->data)+1;
				ret = (char*)sxml_alloc(needed); if(!ret) return NULL; 
				tmp = ret;
				sprintf(tmp, "%s", (char*)node->data);
				break;//��Ƕ�ı�������
			case 3: 
				data = (sxml_data_t*)node->data;
				//if(!strcmp(node->name,"#rawdata"))
				//{
					needed = data->size+15; 
					indent = (node->indent)?node->indent*SXML_INDENT_COUNT:0; 
					needed += indent; 
					ret = (char*)sxml_alloc(needed); if(!ret) return NULL; 
					tmp = ret;
					if(!node->prevSibling || 2 != node->prevSibling->type)//��һ���ӽڵ����ǰ��ڵ㲻����Ƕ�ı�
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
				//}else
				//{
				//	needed = data->size+8+2*strlen(node->name); 
				//	indent = (node->indent)?node->indent*SXML_INDENT_COUNT:0; 
				//	needed += indent*3+SXML_INDENT_COUNT; 
				//	ret = (char*)sxml_alloc(needed); if(!ret) return NULL; 
				//	tmp = ret;
				//	if(node->indent)
				//	{
				//		if(!node->prevSibling || 2 != node->prevSibling->type)
				//		{
				//			*tmp = '\n';++tmp;
				//			for(i = 0; i < indent; i++)
				//			{
				//				*tmp = ' '; ++tmp;
				//			}
				//		}
				//		tmp += sprintf(tmp, "<%s>", node->name);
				//		//for(i = 0; i < indent+SXML_INDENT_COUNT; i++)
				//		//{
				//		//	*tmp = ' '; ++tmp;
				//		//}
				//		tmp += sprintf(tmp, "%s", (char*)data->data);
				//		//for(i = 0; i < indent; i++)
				//		//{
				//		//	*tmp = ' '; ++tmp;
				//		//}
				//		tmp += sprintf(tmp, "</%s>", node->name);
				//	}else
				//	{
				//		if(!node->prevSibling || 2 != node->prevSibling->type)
				//		{
				//			*tmp = '\n';++tmp;
				//		}
				//		tmp += sprintf(tmp, "<%s>%s</%s>", node->name, (char*)data->data, node->name);
				//	}
				//}
				break;//ԭʼ���ݣ�<![CDATA[+ԭʼ����+]]>
				
			case 4:
				data = (sxml_data_t*)node->data;
				if(node->append)
				{
					needed = data->size+strlen(node->name)+strlen(node->append)+2; 
				}else
				{	
					needed = data->size+strlen(node->name)*2+7; 
				}
				indent = (node->indent)?node->indent*SXML_INDENT_COUNT:0; 
				needed += indent; 
				ret = (char*)sxml_alloc(needed); if(!ret) return NULL; 
				tmp = ret;
				if(!node->prevSibling || 2 != node->prevSibling->type)
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
				if(node->append)
				{
					sprintf(tmp, "%s%s%s", node->name, (char*)data->data, node->append);
				}else
				{
					sprintf(tmp, "<%s>%s</%s>", node->name, (char*)data->data, node->name);
				}
				break;//ԭʼ���ݣ�<![CDATA[+ԭʼ����+]]>
			//case 4: 
			//	
			//	if(!node->prevSibling || 2 != node->prevSibling->type)
			//	{
			//		needed = strlen((char*)node->name)+6; 
			//		indent = (node->indent)?node->indent*SXML_INDENT_COUNT:0; 
			//		needed += indent; 
			//		ret = (char*)sxml_alloc(needed); if(!ret) return NULL; 
			//		tmp = ret;
			//		
			//		*tmp = '\n';++tmp;
			//		if(node->indent)
			//		{
			//			for(i = 0; i < indent; i++)
			//			{
			//				*tmp = ' '; ++tmp;
			//			}
			//		}
			//		
			//		sprintf(tmp, "<%s />", (char*)node->name); 
			//	}else
			//	{
			//		needed = strlen((char*)node->name)+5; 
			//		ret = (char*)sxml_alloc(needed); if(!ret) return NULL; 
			//		tmp = ret;
			//		
			//		sprintf(tmp, "<%s />", (char*)node->name); 
			//	}
			//	break;//���ýڵ㣬�ڵ�
		}
		
	}
	
	return ret;
}

XEXPORT XAPI char *sxml_node_print_buffered(sxml_node_t* node,int size)
{
	sxml_buffer_t p;
	if(!node || size < 1) return NULL;
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
	if(!doc || size < 1)return NULL;
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
	if(!doc) return NULL;
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
	char* pbuff;
	if(!filename) return -1;
    pbuff = sxml_doc_print_buffered(doc,16); 
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
	if(!doc) return -1;
	return sxml_save2file(doc,doc->filename);
}

XSTATIC int sxml_error_log(const char* pstr)
{
	//���������У��ҵ������к�ƫ��
	long long i;
	long long position = 0;
	if(!pstr) return -1;
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
	//printf("%*.*s",N,M,str);//���ָ������N���ַ���������Mʱ�ضϣ�����ʱ�Ҷ��룬��߲��ո�
	//printf("%-*.*s",N,M,str);//���ָ������N���ַ���������Mʱ�ضϣ�����ʱ����룬�ұ߲��ո�
	//printf("%*s",N,str);//���ָ������N���ַ���������Nʱ�ض�
	//printf("%*d",N,str);//���ָ������N���֣��Ҷ���
	position =  pstr - g_sxml_parse_error->line_info[i].start;
	printf("parse error, line:%d, position:%d, errorstring:%*.*s\n",g_sxml_parse_error->line_info[i].line, position, g_sxml_parse_error->line_info[i].len-position, g_sxml_parse_error->line_info[i].len-position, pstr); //
	return 0;
}


XEXPORT XAPI const char* sxml_attr_parse(sxml_attr_t* attr, const char* value)
{
	const char* temp=value;
	const char* c=NULL;
	if(!attr || !value) return NULL;
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
		//snprintf(attr->name, c-temp+1, "%s", temp);//snprintf������
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


//node֮��Ĺ�ϵ��֯���ⲿ����
XEXPORT XAPI const char* sxml_node_parse(sxml_node_t* node, const char* value, sxml_parser_t* parser)
{
	const char* temp=value;
	const char* pstr;
	const char* c;
	sxml_attr_t* attr=NULL;
	const char* endp=NULL;
	sxml_data_t* datap=NULL;
	sxml_node_t* subnode=NULL;
	char endbuf[128]={0};
	QUEUE *q=NULL;
	sxml_alias_t* alias;
	
	if(!node || !value) return NULL;
	//<+node
	do
	{
		//CDATA����,����Ҫtrim
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
		
		//ע�ʹ���,����Ҫtrim
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
		//�û��Զ������ݴ������Գ�����
		if(parser && parser->special)
		{
			QUEUE_FOREACH(q, &parser->special)
			{
				alias = (sxml_alias_t*)QUEUE_DATA(q,sxml_alias_t,aq);
				if(strlen(temp) > strlen(alias->alias))
				{
					if(!strncmp(temp, alias->alias, strlen(alias->alias)))
					{
						node->type = SXML_USERDEF;break;
					}
				}
			}
		}
		
		if(SXML_USERDEF == node->type)
		{
			temp = temp + strlen(alias->alias);
			endp = strstr(temp,alias->append);
			if(!endp)break;			
			node->name = (char*)sxml_alloc(strlen(alias->alias)+1);
			if(!node->name)break;
			node->append = (char*)sxml_alloc(strlen(alias->append)+1);
			if(!node->append){ sxml_free(node->name); break; }
			sprintf(node->name,"%s",alias->alias);
			sprintf(node->append,"%s",alias->append);
			node->parent = NULL;
			node->indent = 0;
			datap = (sxml_data_t*)sxml_alloc(sizeof(sxml_data_t));
			if(!datap)
			{
				sxml_free(node->name);
				sxml_free(node->append);
				printf("sxml_alloc error\n");
				break;
			}
			datap->size = endp - temp;
			datap->data = sxml_alloc(datap->size+1);
			if(!datap->data)
			{
				sxml_free(node->name);
				sxml_free(node->append);
				sxml_free(datap);
				printf("sxml_alloc error\n");
				return NULL;
			}
			memcpy(datap->data, temp, datap->size);	
			node->data = datap;
			return endp+strlen(alias->append);
		}
		//�û��Զ������ݴ����Գ�����
		if(parser && parser->normal)
		{
			QUEUE_FOREACH(q, &parser->normal)
			{
				alias = (sxml_alias_t*)QUEUE_DATA(q,sxml_alias_t,aq);
				if(strlen(temp) > (strlen(alias->alias)+2))
				{
					sprintf(endbuf, "<%s>", alias->alias);
					if(!strncmp(temp, endbuf, strlen(alias->alias)+2))
					{
						node->type = SXML_USERDEF;break;
					}
				}
			}
		}
		if(SXML_USERDEF == node->type)
		{
			temp = temp + strlen(alias->alias)+2;
			sprintf(endbuf, "</%s>", alias->alias);
			endp = strstr(temp,endbuf);
			if(!endp)break;			
			node->name = (char*)sxml_alloc(strlen(alias->alias)+1);
			if(!node->name)break;
			sprintf(node->name,"%s",alias->alias);
			node->append = NULL;
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
			return endp+strlen(alias->alias)+3;
		}
		
		//innertext,����Ҫtrim
		pstr = skip(temp); // temp =
		if(*pstr != '<')//innertext,trim����
		{
			do
			{		
				c = strstr(pstr,"<");
				pstr = c+1;
			}while(c && *c && *(c-1) == '\\');
			if(!c)
			{
				printf("innertext parse error !\n"); //�����þ�̬ȫ�ֱ����滻
				//innertext,error
				break;
			}else
			{
				//�����������ڵ�,���굱ǰ��㷵��,һ��ֻ���һ���ڵ㡣
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
		//��ͨ�ڵ�
		c = string_index_of_any(value," \t\n>/");
		if(!c)break;
		node->name = sxml_alloc(c-value);
		if(!node->name)
		{
			printf("sxml_alloc error\n");
			break;
		}
		memcpy(node->name, value+1, c-value-1);
		
		////������Ҫ�ȱȽ�,���Ϊrawdata���⴦��󷵻أ����治��
		//QUEUE_FOREACH(q, &parser->normal)
		//{
		//	alias = (sxml_alias_t*)QUEUE_DATA(q,sxml_alias_t,aq);
		//	if(!strcmp(alias->alias, node->name))
		//	{
		//		node->type = SXML_RAWDATA;break;
		//	}
		//}
		//if(SXML_RAWDATA == node->type && *c == '>')//�Զ���ԭʼ���ݽڵ�,�ݶ�Ϊ����������,ʵ�����ǿ����е�,�Ժ������,����javascript�ȡ�
		//{
		//	sprintf(endbuf,"</%s>",node->name);//β�ڵ����ƹ���
		//	endp = strstr(c+1,endbuf);
		//	node->parent = NULL;
		//	node->indent = 0;
		//	datap = (sxml_data_t*)sxml_alloc(sizeof(sxml_data_t));
		//	if(!datap)
		//	{
		//		sxml_free(node->name);
		//		printf("sxml_alloc error\n");
		//		break;
		//	}
		//	datap->size = endp - c -1;
		//	datap->data = sxml_alloc(datap->size+1);
		//	if(!datap->data)
		//	{
		//		sxml_free(node->name);
		//		sxml_free(datap);
		//		printf("sxml_alloc error\n");
		//		break;
		//	}
		//	memcpy(datap->data, c+1, datap->size);	
		//	node->data = datap;
		//	return endp+strlen(endbuf);
		//}
		//snprintf(node->name, c-value, "%s",value+1);
		pstr = temp = c;
		if(*skip(c) != '>' && (*c == ' ' || *c == '\t' || *c == '\n'))//�����Խڵ�,β�ڵ�����ϸ��﷨,�ڵ��в����пհ��ַ�,ͷ�ڵ���ÿ����﷨��
		//if(*c == ' ' || *c == '\t' || *c == '\n')//�����Խڵ�,β�ڵ�����ϸ��﷨,�ڵ��в����пհ��ַ�,ͷ�ڵ���ÿ����﷨��
		{
			while(*c == ' ' || *c == '\t' || *c == '\n')
			{
				//���������
				pstr = c;
				while(*pstr == ' ' || *pstr == '\t' || *c == '\n')pstr++;
				if('/' == *pstr)
				{
					if(NULL != pstr+1 && *(pstr+1) == '>')
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
			//��ʼ����ڵ�,�ӽڵ�
			if(*pstr != '>')
			{
				//�ͷ��ڴ�
				sxml_free(node->name);
				sxml_attr_free(attr);
				break;
			}
			sprintf(endbuf,"</%s>",node->name);//β�ڵ����ƹ���
			endp = strstr(temp+1,endbuf);
			if(!endp)
			{
				sxml_free(node->name);
				sxml_attr_free(attr);
				break;
			}
			pstr=temp=pskip(temp+1);
			//�ӽڵ�,�ݹ����
			while(pstr < endp)
			{
				subnode = sxml_node_item_new();
				QUEUE_INIT(&subnode->children);
				QUEUE_INIT(&subnode->attrs);
				QUEUE_INIT(&subnode->nq);
				pstr = sxml_node_parse(subnode, temp, parser);
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
				//�ͷ����Ժ��ӽڵ㣬�˳�
				//todo
				sxml_node_attrs_free(node);
				sxml_node_children_free(node);
				break;
			}
			node->type = 0;
			return endp+strlen(endbuf);
			
			
		}else if(*c == '>' || *skip(c) == '>')//�������Խڵ�,�ڵ�ͷ���ҵ��ڵ�β�������м����
		//}else if(*c == '>')//�������Խڵ�,�ڵ�ͷ���ҵ��ڵ�β�������м����
		{
			sprintf(endbuf,"</%s>",node->name);//β�ڵ����ƹ���
			temp=skip(c);
			endp = strstr(temp+1,endbuf);
			if(!endp)
			{
				sxml_free(node->name);
				sxml_attr_free(attr);
				break;
			}
			pstr=temp= pskip(temp+1);
			//�ӽڵ�,�ݹ����
			while(pstr < endp)
			{
				subnode = sxml_node_item_new();
				QUEUE_INIT(&subnode->children);
				QUEUE_INIT(&subnode->attrs);
				QUEUE_INIT(&subnode->nq);
				pstr = sxml_node_parse(subnode, temp, parser);
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
				//�ͷ����Ժ��ӽڵ㣬�˳�
				//todo
				sxml_node_attrs_free(node);
				sxml_node_children_free(node);
				break;
			}
			node->type = 0;
			return endp+strlen(endbuf);
		}else if(*c == '/')//�������Խڵ�,�սڵ㣬���͸�ֵ������
		{
			if(*(c+1) == '>')
			{
				node->type = 0;
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
	if(!doc || !value) return NULL;
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


XEXPORT XAPI sxml_doc_t* sxml_doc_parse(const char* filename, const char* value, sxml_parser_t* parser)
{
	const char* ret;
	long long len;
	sxml_node_t* node=NULL;
	sxml_doc_t* doc;
	if(!value) return NULL;
	doc = sxml_doc_item_new();
	QUEUE_INIT(&doc->dq);
	if(!doc) return NULL;
	if(filename)sprintf(doc->filename, "%s", filename);
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
		ret = sxml_node_parse(node, ret, parser);
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

XEXPORT XAPI sxml_doc_t* sxml_parse(const char* filename, sxml_parser_t* parser)
{
	sxml_doc_t* doc=NULL;
	sxml_file_info_t* info=NULL;
	FILE *f;long len;char *data;
	if(!filename) return NULL;
	f=fopen(filename,"rb");fseek(f,0,SEEK_END);len=ftell(f);fseek(f,0,SEEK_SET);
	data=(char*)sxml_alloc(len+1);
	fread(data,1,len,f);fclose(f);
	info = sxml_get_file_info(data);
	doc=sxml_doc_parse(filename, data, parser);
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

XEXPORT XAPI sxml_node_t* sxml_node_nextSibling(sxml_node_t* node)
{
	if(!node) return NULL;
	return node->nextSibling;
}


XEXPORT XAPI sxml_node_t* sxml_node_prevSibling(sxml_node_t* node)
{
	if(!node) return NULL;
	return node->prevSibling;
}

XEXPORT XAPI sxml_node_t* sxml_node_getChildByName(sxml_node_t* node, char* name)
{
	sxml_node_t* child=NULL;
	QUEUE* q=NULL;
	if(!node || !name) return NULL;
	if(QUEUE_ISEMPTY(&node->children))
	{
		return NULL;
	}
	QUEUE_FOREACH(q,&node->children)
	{
		child = (sxml_node_t*)QUEUE_DATA(q,sxml_node_t,nq);
		if(!strcmp(child->name,name))
		{
			return child;
		}
	}
	return NULL;
}

XEXPORT XAPI sxml_attr_t* sxml_attr_nextSibling(sxml_attr_t* attr)
{
	if(!attr) return NULL;
	return attr->nextSibling;
}


XEXPORT XAPI sxml_attr_t* sxml_attr_prevSibling(sxml_attr_t* attr)
{
	if(!attr) return NULL;
	return attr->prevSibling;
}

XEXPORT XAPI sxml_attr_t* sxml_node_getAttrByName(sxml_node_t* node, char* name)
{
	sxml_attr_t* attr=NULL;
	QUEUE* q=NULL;
	if(!node || !name) return NULL;
	if(QUEUE_ISEMPTY(&node->attrs))
	{
		return NULL;
	}
	QUEUE_FOREACH(q,&node->attrs)
	{
		attr = (sxml_attr_t*)QUEUE_DATA(q,sxml_attr_t,aq);
		if(!strcmp(attr->name,name))
		{
			return attr;
		}
	}
	return NULL;
}
















