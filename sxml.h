#ifndef __SXML_H__
#define __SXML_H__

//缩进设置
#define SXML_INDENT_COUNT		4

//定义一个指针数组类型
typedef void *QUEUE[2];

#define		XAPI	__declspec(dllexport)

#ifdef __cplusplus
#define 	XEXPORT extern "C"
#else
#define		XEXPORT extern 
#endif

//原始数据结构体，要不要类型
typedef struct __sxml_data_t
{
	long long 	size;//数据大小
	void        *data;//数据指针
}sxml_data_t;

typedef struct __sxml_attr_t
{
	char*		name;
	char*		value;
	long long	type;//字符串、数值、名域;0,1,2
	QUEUE		aq;
}sxml_attr_t;

typedef struct __sxml_node_t
{
	char*					name;
	long long				type;//普通节点、注释节点、内嵌文本节点、原始数据节点、空节点;0,1,2,3,4
	long long 				indent;//缩进
	struct __sxml_node_t*	parent;//父节点指针
	void*					data;//存放数据
	QUEUE					children;//子节点链表头指针
	QUEUE					attrs;//属性链表头指针
	QUEUE					nq;
}sxml_node_t;

typedef struct __sxml_doc_t
{
	char		filename[256];
	char		version[8];
	char		charset[8];
	QUEUE		dq;
}sxml_doc_t;


typedef struct __sxml_hooks_t
{
    void *(*alloc)(unsigned int size);
    void (*free)(void *p);
}sxml_hooks_t,*sxml_hooks_ht;

typedef struct __sxml_buffer_t
{
	char *buffer; 
	int length; 
	int offset; 
}sxml_buffer_t,*sxml_buffer_ht;



XEXPORT XAPI sxml_doc_t* sxml_doc_new(const char* filename, const char* version, const char* charset);
XEXPORT XAPI sxml_node_t* sxml_node_new(const char* name);
XEXPORT XAPI sxml_node_t* sxml_rawdata_new(const char* name, const void* data, long long size);
XEXPORT XAPI sxml_node_t* sxml_innertext_new(const char* innertext);
XEXPORT XAPI sxml_node_t* sxml_comment_new(const char* comment);
XEXPORT XAPI sxml_node_t* sxml_empty_new(const char* name);
XEXPORT XAPI sxml_attr_t* sxml_attr_new(const char* name, const char* value);

XEXPORT XAPI int sxml_add_attr2node(sxml_node_t* node, sxml_attr_t* attr);
XEXPORT XAPI int sxml_add_node2doc(sxml_doc_t* doc, sxml_node_t* node);
XEXPORT XAPI int sxml_add_subnode2node(sxml_node_t* node, sxml_node_t* child);

XEXPORT XAPI char *sxml_node_print_buffered(sxml_node_t* node,int size);
XEXPORT XAPI char *sxml_doc_print_buffered(sxml_doc_t* doc,int size);
XEXPORT XAPI char *sxml_doc_print(sxml_doc_t* doc);

XEXPORT XAPI void sxml_attr_free(sxml_attr_t* attr);
XEXPORT XAPI void sxml_node_free(sxml_node_t* node);
XEXPORT XAPI void sxml_doc_free(sxml_doc_t* doc);

XEXPORT XAPI int sxml_save2file(sxml_doc_t* doc, const char* filename);
XEXPORT XAPI int sxml_save(sxml_doc_t* doc);

#endif




