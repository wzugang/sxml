#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include "sxml.h"


#define LUA_SCRIPT "function fun()\n\
	int a,b,c;\n\
	a = 10;\n\
	b = 5;\n\
	if a < 10 && b >5 then\n\
	c = 100;\n\
	return a;\n\
end"

int main()
{
	sxml_file_info_t* info;
	char* tmp,*tmp2;
	const char* pstr;
	int line=1;
	char buff[128]={0};
	sxml_doc_t* parser=NULL;
	sxml_doc_t* doc = sxml_doc_new("a.xml","1.0","GB2312");
	sxml_node_t* node = sxml_node_new("root");
	sxml_attr_t* attr = sxml_attr_new("age","25");
	
	sxml_node_t* subnode = sxml_node_new("score");
	sxml_node_t* subnode2 = sxml_node_new("subnode");
	sxml_node_t* rawdata = sxml_rawdata_new("hello world!",strlen("hello world!")); 
	sxml_node_t* rawdata2 = sxml_rawdata_new("hello world!",strlen("hello world!")); 
	sxml_node_t* rawdata3 = sxml_rawdata_new("hello world!",strlen("hello world!")); 
	sxml_node_t* userdef = sxml_userdef_new("raw",NULL,"rawdata",strlen("rawdata")); 
	sxml_node_t* userdef2 = sxml_userdef_new("lua",NULL,LUA_SCRIPT,strlen(LUA_SCRIPT)); 
	sxml_node_t* userdef3 = sxml_userdef_new("<?lua ","?>",LUA_SCRIPT,strlen(LUA_SCRIPT)); 
	sxml_node_t* innertext = sxml_innertext_new("内部字符串");
	sxml_node_t* innertext2 = sxml_innertext_new("内部字符串");
	sxml_node_t* comment = sxml_comment_new("注释>>><<<");
	sxml_node_t* empty = sxml_node_new("empty");
	sxml_parser_t* parser_t = sxml_parser_new();
	sxml_alias_t* alias = sxml_alias_new("raw", NULL);
	sxml_alias_t* alias2 = sxml_alias_new("lua", NULL);
	sxml_alias_t* alias3 = sxml_alias_new("<?lua ", "?>");
	sxml_add_alias2parser(parser_t, alias);
	sxml_add_alias2parser(parser_t, alias2);
	sxml_add_alias2parser(parser_t, alias3);
	
	sxml_add_attr2node(node,attr);
	attr = sxml_attr_new("sex","man");
	sxml_add_attr2node(node,attr);

	attr = sxml_attr_new("语文","95");
	sxml_add_attr2node(subnode,attr);
	attr = sxml_attr_new("数学","100");	
	sxml_add_attr2node(subnode,attr);
	attr = sxml_attr_new("英语","98");	
	sxml_add_attr2node(subnode,attr);
	
	sxml_add_subnode2node(node,subnode);
	sxml_add_subnode2node(node,rawdata);
	sxml_add_subnode2node(node,rawdata2);
	sxml_add_subnode2node(node,rawdata3);
	sxml_add_subnode2node(node,userdef);
	sxml_add_subnode2node(node,userdef2);
	sxml_add_subnode2node(node,userdef3);
	sxml_add_subnode2node(node,innertext);
	sxml_add_subnode2node(node,comment);
	sxml_add_subnode2node(node,empty);
	sxml_add_subnode2node(node,subnode2);
	sxml_add_subnode2node(subnode2,innertext2);
	
	sxml_add_node2doc(doc,node);
	
	tmp = sxml_doc_print(doc);
	tmp2 = sxml_doc_print_buffered(doc,16);
	pstr = tmp;
	printf("%s\n", tmp);
	printf("%s\n", tmp2);
	
	sxml_save(doc);
	
	sxml_doc_free(doc);
	doc = NULL;
	free(tmp);
	free(tmp2);
	
	parser = sxml_parse("a.xml",parser_t);
	
	tmp = sxml_doc_print_buffered(parser,16);
	tmp2 = sxml_doc_print(parser);
	printf("%s\n", tmp);
	printf("%s\n", tmp2);
	free(tmp);
	free(tmp2);
	sxml_save2file(parser,"b.xml");
	sxml_doc_free(parser);
	
	return 0;
}

