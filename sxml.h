#ifndef __SXML_H__
#define __SXML_H__


//查找兄弟节点不方便，孩子节点的个数，属性节点的个数，标记自己的排行（父节点的第几个孩子节点）
//查找兄弟属性,节点没有孩子打印处理（已完成）
//原始数据没有属性


//缩进设置
#define SXML_INDENT_COUNT		4

//定义一个指针数组类型
typedef void *QUEUE[2];

#ifdef      _WIN32
#define		XAPI	__declspec(dllexport)
#else
#define		XAPI
#endif

#ifdef __cplusplus
#define 	XEXPORT extern "C"
#else
#define		XEXPORT extern 
#endif

typedef enum __sxml_type_t
{
	SXML_NORMAL,
	SXML_COMMENT,
	SXML_INNERTEXT,
	SXML_RAWDATA,
	SXML_USERDEF
}sxml_type_t;

//节点别名
typedef struct __sxml_alias_t
{
	QUEUE aq;
	long long type;//0,对称节点,1不对称节点
	char* alias;
	char* append;
}sxml_alias_t;

typedef struct __sxml_parser_t
{
	QUEUE normal;//对称节点
	QUEUE special;//不对称节点
}sxml_parser_t;

//原始数据结构体，要不要类型
typedef struct __sxml_data_t
{
	long long 				size;//数据大小
	void        			*data;//数据指针
}sxml_data_t;

typedef struct __sxml_node_t
{
	char*					name;
	char*					append;//用于用户自定义节点
	long long				type;//普通节点、注释节点、内嵌文本节点、原始数据节点、空节点;0,1,2,3,4
	long long 				indent;//缩进
	long long 				index;//排行
	long long 				childCount;//孩子个数
	long long 				attrCount;//属性个数
	unsigned char 			reserved[8];//保留
	struct __sxml_node_t*	prevSibling;//上一个兄弟节点
	struct __sxml_node_t*	nextSibling;//下一个兄弟节点
	struct __sxml_node_t*	parent;//父节点指针
	void*					data;//存放数据
	QUEUE					children;//子节点链表头指针
	QUEUE					attrs;//属性链表头指针
	QUEUE					nq;
}sxml_node_t;

typedef struct __sxml_attr_t
{
	struct __sxml_attr_t*	prevSibling;//上一个兄弟属性
	struct __sxml_attr_t*	nextSibling;//下一个兄弟属性
	struct __sxml_node_t*	owner;//属性所属节点
	long long 				index;//排行
	char*					name;
	char*					value;
	long long				type;//字符串、数值、名域;0,1,2
	QUEUE					aq;
}sxml_attr_t;

typedef struct __sxml_doc_t
{
	char					filename[256];
	char					version[8];
	char					charset[8];
	QUEUE					dq;
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

typedef struct __sxml_file_line_t
{
	const char* start;
	long long line;
	long long len;
}sxml_file_line_t;

typedef struct __sxml_file_info_t
{
	sxml_file_line_t* line_info;
	long long line_count;
}sxml_file_info_t;

XEXPORT XAPI 	sxml_doc_t* 		sxml_doc_new(const char* filename, const char* version, const char* charset);
XEXPORT XAPI 	sxml_node_t* 		sxml_node_new(const char* name);
XEXPORT XAPI 	sxml_node_t* 		sxml_rawdata_new(const void* data, long long size);
XEXPORT XAPI 	sxml_node_t* 		sxml_userdef_new(const char* start, const char* end, const void* data, long long size);
XEXPORT XAPI 	sxml_node_t*		sxml_innertext_new(const char* innertext);
XEXPORT XAPI 	sxml_node_t* 		sxml_comment_new(const char* comment);
XEXPORT XAPI 	sxml_attr_t* 		sxml_attr_new(const char* name, const char* value);
XEXPORT XAPI 	sxml_parser_t* 		sxml_parser_new();
XEXPORT XAPI 	sxml_alias_t* 		sxml_alias_new(char* name, char* append);
	
XEXPORT XAPI 	int 				sxml_add_attr2node(sxml_node_t* node, sxml_attr_t* attr);
XEXPORT XAPI 	int 				sxml_add_node2doc(sxml_doc_t* doc, sxml_node_t* node);
XEXPORT XAPI 	int 				sxml_add_subnode2node(sxml_node_t* node, sxml_node_t* child);
XEXPORT XAPI 	long long int 		sxml_add_alias2parser(sxml_parser_t* parser, sxml_alias_t* alias);
	
XEXPORT XAPI 	int 				sxml_del_node4attr(sxml_node_t* node, char* name);
XEXPORT XAPI 	int 				sxml_del_doc4node(sxml_doc_t* doc, char* name);
XEXPORT XAPI 	int 				sxml_del_node4subnode(sxml_node_t* node, char* name);
XEXPORT XAPI 	long long int 		sxml_del_parser4alias(sxml_parser_t* parser, char* name);
	
XEXPORT XAPI 	char*				sxml_node_print_buffered(sxml_node_t* node,int size);
XEXPORT XAPI 	char*				sxml_doc_print_buffered(sxml_doc_t* doc,int size);
XEXPORT XAPI 	char*				sxml_doc_print(sxml_doc_t* doc);
	
XEXPORT XAPI 	void 				sxml_attr_free(sxml_attr_t* attr);
XEXPORT XAPI 	void 				sxml_node_free(sxml_node_t* node);
XEXPORT XAPI 	void 				sxml_doc_free(sxml_doc_t* doc);
	
XEXPORT XAPI 	int 				sxml_save2file(sxml_doc_t* doc, const char* filename);
XEXPORT XAPI 	int 				sxml_save(sxml_doc_t* doc);
	
XEXPORT XAPI 	sxml_file_info_t* 	sxml_get_file_info(const char* value);
XEXPORT XAPI 	void 				sxml_print_file_info(sxml_file_info_t* info);
XEXPORT XAPI 	void 				sxml_free_file_info(sxml_file_info_t** info);
	
XEXPORT XAPI 	sxml_doc_t* 		sxml_doc_parse(const char* filename, const char* value, sxml_parser_t* parser);
XEXPORT XAPI 	sxml_doc_t* 		sxml_parse(const char* filename, sxml_parser_t* parser);
	
XEXPORT XAPI 	sxml_node_t* 		sxml_node_nextSibling(sxml_node_t* node);
XEXPORT XAPI 	sxml_node_t* 		sxml_node_prevSibling(sxml_node_t* node);
XEXPORT XAPI 	sxml_attr_t* 		sxml_attr_nextSibling(sxml_attr_t* attr);
XEXPORT XAPI 	sxml_attr_t* 		sxml_attr_prevSibling(sxml_attr_t* attr);
			
XEXPORT XAPI 	sxml_node_t* 		sxml_node_getChildByName(sxml_node_t* node, char* name);
XEXPORT XAPI 	sxml_attr_t* 		sxml_node_getAttrByName(sxml_node_t* node, char* name);

#endif




