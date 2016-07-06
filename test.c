#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include "sxml.h"


#define LUA_SCRIPT "function fun()\n\
	int a;\n\
	a = 10;\n\
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
	sxml_node_t* rawdata = sxml_rawdata_new(NULL,"hello world!",strlen("hello world!")); 
	sxml_node_t* rawdata2 = sxml_rawdata_new(NULL,"hello world!",strlen("hello world!")); 
	sxml_node_t* rawdata3 = sxml_rawdata_new(NULL,"hello world!",strlen("hello world!")); 
	sxml_node_t* rawdata4 = sxml_rawdata_new("raw","rawdata",strlen("rawdata")); 
	sxml_node_t* rawdata5 = sxml_rawdata_new("lua",LUA_SCRIPT,strlen(LUA_SCRIPT)); 
	sxml_node_t* innertext = sxml_innertext_new("�ڲ��ַ���");
	sxml_node_t* comment = sxml_comment_new("ע��");
	sxml_node_t* empty = sxml_empty_new("empty");
	
	sxml_add_attr2node(node,attr);
	attr = sxml_attr_new("sex","man");
	sxml_add_attr2node(node,attr);

	attr = sxml_attr_new("����","95");
	sxml_add_attr2node(subnode,attr);
	attr = sxml_attr_new("��ѧ","100");	
	sxml_add_attr2node(subnode,attr);
	attr = sxml_attr_new("Ӣ��","98");	
	sxml_add_attr2node(subnode,attr);
	
	sxml_add_subnode2node(node,subnode);
	sxml_add_subnode2node(node,rawdata);
	sxml_add_subnode2node(node,rawdata2);
	sxml_add_subnode2node(node,rawdata3);
	sxml_add_subnode2node(node,rawdata4);
	sxml_add_subnode2node(node,rawdata5);
	sxml_add_subnode2node(node,innertext);
	sxml_add_subnode2node(node,comment);
	sxml_add_subnode2node(node,empty);	
	
	sxml_add_node2doc(doc,node);
	
	tmp = sxml_doc_print(doc);
	tmp2 = sxml_doc_print_buffered(doc,16);
	pstr = tmp;
	printf("%s\n", tmp);
	printf("%s\n", tmp2);
	//while((pstr = skip_line(pstr)))line++;
	//printf("\n\n%d\n\n", line);
	//printf("\n\n%s\n\n",check_skip("<?xml version=\"1.0\" encoding=\"GB2312\"?>","<?xml version=\\\"")?"YES":"NO");
	//copy_until(buff,"<?xml version=\"1.0\" encoding=\"GB2312\"?>","\" ");
	//printf("\n\n%s\n\n",buff);
	
	sxml_save(doc);

	
	sxml_doc_free(doc);
	doc = NULL;
	free(tmp);
	free(tmp2);
	
	//printf("aaaa...................\n");
	
	//getchar();
	
	parser = sxml_parse("a.xml");
	
	
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

