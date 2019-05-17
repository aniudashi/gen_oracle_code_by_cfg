/**
* \file		gen_table_datastruct.cpp
* \author	mart
* \version  1.0
* \date		2014/3/18 10:23
* \brief	自动生成代码工具，根据oracle表结构生成对应的c++数据结构，
*/


#include <sstream>
#include <iostream>
#include <fstream>
#include <ConnectionWrapper.h>
#include <ai/Exceptions.h>
#include <aialtibase.h>
#include <aidb3.h>
#include <ai/Get_Opt.h>
#include <myotl.h>
#include <vector>
#include <string>
#include "iniFile.h"

using namespace std;
using aidb3::Statement;
using aidb3::Connection;
using namespace asiainfo;

/*
程序支持的操作
1.根据条件查询 #配置由[select1]--[select10]
[select0]
fields=查询字段1;查询字段2 #没有配置为全字段
wheres=查询条件1;查询条件2 #没有条件就不配置
orders=排序字段1;排序字段2
rownum=最大记录 #生成sql为 rownum<maxcount
[select1]
fields=查询字段1;查询字段2 #没有配置为全字段
wheres=查询条件1;查询条件2
orders=排序字段1;排序字段2
rownum=最大记录 #生成sql为 rownum<maxcount

2.根据条件更新
[update0]
fields=更新字段1;更新字段2 #没有配置则为全字段
wheres=更新条件 #没有配置则为全字段匹配

3.根据条件删除
[delete0]
where=删除条件1;删除条件2 #无条件则根据全字段删除

说明：
根据accounting/interface下面的代码生成代码。
生成的函数应该为select1,select2,delete1,delete2,update1,update2
因为select,delete,update已经被使用了，且为虚拟基类虚拟函数，
一定需要select/delete/update 给个空的实现，故使用名字xxx1,xxx2,xxx3

配置文件 



成员变量前缀
m_str
m_n
m_Date

输入参数前缀
n
str
Date

文件名规则
classnameData.h
classnameData.cpp
classnameHanle.cpp
classnameHanle.cpp

类名规则prefix为配置文件配置的前缀名，若无，则同表名
Cprefix
CprefixHanleImp
CprefixHanle



*/

/*
配置例子
bash-3.00$ cat gen_data_struct.cfg 
[TABLE]
table_name=real_sms_event
prefix=RealSmsEvent
[SELECT0]
fields=
wheres=
orders=insert_timestamp
rownum=100
[SELECT1]
fields=
wheres=region_id
orders=insert_timestamp
rownum=100
[SELECT2]
fields=status
wheres=region_id;subscription_id
orders=insert_timestamp
rownum=100
[SELECT3]
fields=subscription_id;seq
wheres=region_id
orders=insert_timestamp
rownum=100
[DELETE0]
wheres=
[DELETE1]
wheres=seq
[DELETE2]
wheres=seq;subscription_id
[UPDATE0]
fields=
wheres=
[UPDATE1]
fields=status;insert_timestamp
wheres=subscription_id
[UPDATE2]
fields=status
wheres=subscription_id;status

*/
int usage(const char* program) 
{
	printf("Usage: %s [options]\n",program);
	printf("Options:\n");
	printf(" -c	configure file name \n");
	printf("Examples:\n");
	printf("    Regular Flow:\n");
	printf("    %s -c gen_data_struct.cfg \n",program);
	return 1;
}

struct CColumnInfo 
{
	string column_name; //oracle 中的cloumn name
	string data_type;
	int data_length; 
	string member_suffix;//cpp class中的成员的后缀
};

struct CSelectInfo 
{
	vector<string> vecResultFields; //需要查询出的字段
	vector<string> vecWhereFields;  //查询条件
	vector<string> vecOrderFields;  //查询order by 字段
	int maxSelectCounts;            //rownum<=
	string modField;				//取模字段
};

struct CDeleteInfo 
{
	vector<string> vecWhereFields;//删除需要的条件
};

struct CUpdateInfo
{
	vector<string> vecUpdateFields; //需要更新的字段
	vector<string> vecWhereFields;  //更新条件
};



struct CGlobalConfig
{
	string str_config_name;
	string str_table_name;
	string str_prefix_name;
	string str_class_name; //data struct class name
	string str_handleimp_class_name;
	string str_handle_class_name;
	string str_struct_h_name;
	string str_struct_cpp_name;
	string str_handle_h_name;
	string str_handle_cpp_name;
	vector<CColumnInfo> vecClounmInfo;//
	vector<CSelectInfo> vecSelectInfo;//select info vector
	vector<CDeleteInfo> vecDeleteInfo;//delete info vector
	vector<CUpdateInfo> vecUpdateInfo;//update info vector
	/*
	int n_all_select; //是否实现全量无条件查询
	int n_all_select_max; //全量无条件查询每次最多条数 rowcount<=n_all_select_max
	string str_all_select_order;//全量 order by 
	*/
	vector<string> select_where;

	bool GetColumnByName(const string& sName,CColumnInfo& info)
	{
		bool bRes=false;
		for ( int i=0;i<vecClounmInfo.size();i++ )
		{
			if ( sName==vecClounmInfo[i].column_name )
			{
				info=vecClounmInfo[i];
				bRes=true;
				break;
			}
		}
		return bRes;
	}
};


void GenHandleBodyFile(ofstream& ofs,CGlobalConfig& config)
{
	string strClassName=config.str_class_name;
	string strStructName=config.str_struct_h_name;
	string strHandleImpName=config.str_handleimp_class_name;
	string strHanleName=config.str_handle_class_name;
	string strHanleHFileName=config.str_handle_h_name;
	vector<CColumnInfo>& m_vecColumn = config.vecClounmInfo;
	
	//include
	ofs<<"\n\n"
		<<"#include \""
		<<strHanleHFileName
		<<"\" \n"
		<<"#include \"UcsSubsGsmHandle.h\" \n\n ";
		
	//destruct
	ofs<<strHandleImpName
		<<"::~"
		<<strHandleImpName
		<<"(void)\n"
		<<"{\n"
		<<"\t if ( s_insert!=NULL) delete s_insert;\n";
	for ( int i=0;i<config.vecSelectInfo.size();i++ )
	{
		ofs<<"\t if(s_select"
			<<i
			<<"!=NULL) delete s_select"
			<<i
			<<";\n";
	}
	for ( int i=0;i<config.vecDeleteInfo.size();i++ )
	{
		ofs<<"\t if(s_delete"
			<<i
			<<"!=NULL) delete s_delete"
			<<i
			<<";\n";
	}

	for ( int i=0;i<config.vecUpdateInfo.size();i++ )
	{
		ofs<<"\t if(s_update"
			<<i
			<<"!=NULL) delete s_update"
			<<i
			<<";\n";
	}

	
		ofs<<"}\n\n";

	//insert
	ofs<<"int "
		<<strHandleImpName
		<<"::insert(const "
		<<strClassName
		<<"& v)\n"
		<<"{\n"
		<<"\t init1(INSERT);\n"
		<<"\t s_insert->clearParams();\n"
		<<"\t (const_cast<"
		<<strClassName
		<<"&>(v)).flush(*s_insert);\n"
		<<"\t return s_insert->execute();\n"
		<<"}\n\n";

	//init function
	ofs<<"void "
		<<strHandleImpName
		<<"::init1(int flag,int moddividend,int modresult,int rownum)\n"
		<<"{\n"
		<<"\tif(handle_ & flag) return;\n"
		<<"\tif(flag==INSERT)\n"
		<<"\t{\n"
		<<"\t s_insert=new Statement;\n"
		<<"\t *s_insert=conn_->createStatement(); \n"
		<<"\t string sqlinsert=\"insert into \";\n"
		<<"\t sqlinsert+=tableName_;\n"
		<<"\t sqlinsert+=\"(";
	for ( int i=0;i<m_vecColumn.size();i++ )
	{
		ofs<<m_vecColumn[i].column_name;
		if ( i==m_vecColumn.size()-1 )
		{
			ofs<<")";
		}
		else
		{
			ofs<<",";
		}
	}
	ofs<<"values(";
	for ( int i=0;i<m_vecColumn.size();i++ )
	{
		char sTmp[100];
		sprintf(sTmp,":f%d",i);
		if ( i==m_vecColumn.size()-1 )
		{
			ofs<<sTmp
				<<")\";\n";
		}
		else
		{
			ofs<<sTmp
				<<",";
		}
	}
	ofs<<"\t s_insert->prepare(sqlinsert); \n";
	ofs<<"\t}\n";
	
	ofs<<"\tif(flag==SELECT)\n"
		<<"\t{\n";
	for(int i=0;i<config.vecSelectInfo.size();i++)
	{

		vector<string>& vecResultFields=config.vecSelectInfo[i].vecResultFields;
		vector<string>& vecWhereFields=config.vecSelectInfo[i].vecWhereFields;
		vector<string>& vecOrderFields=config.vecSelectInfo[i].vecOrderFields;
		string strModField=config.vecSelectInfo[i].modField;
		int nRownum=config.vecSelectInfo[i].maxSelectCounts;
		ofs<<"\t s_select"
		   <<i
		   <<"=new Statement;\n"
		   <<"\t *s_select"
		   <<i
		   <<"=conn_->createStatement();\n"
		   <<"\t string strSelect"
		   <<i
		   <<"=\"select ";
		for ( int j=0;j<vecResultFields.size();j++ )
		{
			ofs<<" "
			   <<vecResultFields[j];
			if (  j!= vecResultFields.size()-1 && vecResultFields.size()>1 )
			{
				ofs<<",";
			}
		}
		ofs<<" from \";\n"
			<<"\t strSelect"
			<<i
			<<"+=tableName_;\n";
		
		ofs<<"\t strSelect"
			<<i
			<<"+= \" ";
		if ( vecWhereFields.size()>0 )
		{
			ofs<<" where ";
			for(int z=0;z<vecWhereFields.size();z++)
			{
				if ( z!=0 )
				{
					ofs<<" and ";
				}
				ofs<<vecWhereFields[z]
				<<"=:f"
				   <<z;				  
			}
		}
		ofs<<"\";\n";
		
		ofs<<"\tif(rownum>0)\n"
			<<"\t{\n";
		ofs<<"\t strSelect"
			<<i
			<<"+=\" ";
			if ( vecWhereFields.size()>0 )
			{
				ofs<<" and rownum<=:f"
				    <<vecWhereFields.size();				  
			}
			else
			{
				ofs<<" where rownum<=:f"
					<<vecWhereFields.size();
			}
			if ( strModField.length()>0 )
			{
				ofs<<" and mod("
					<<strModField
					<<",:f"
					<<vecWhereFields.size()+1
					<<")=:f"
					<<vecWhereFields.size()+2;
			}

		ofs<<"\";\n";
		ofs<<"\t}\n";
		ofs<<"\telse\n"
			<<"\t{\n";
		ofs<<"\t strSelect"
			<<i
			<<"+=\" ";
		if ( strModField.length()>0 )
		{
			if ( vecWhereFields.size()>0 )
			{
				ofs<<" and mod("
					<<strModField
					<<",:f"
					<<vecWhereFields.size()
					<<")=:f"
					<<vecWhereFields.size()+1;			  
			}
			else
			{
				ofs<<" where mod("
					<<strModField
					<<",:f"
					<<vecWhereFields.size()
					<<")=:f"
					<<vecWhereFields.size()+1;
			}
			
		}

		ofs<<"\";\n";
		ofs<<"\t}\n";


		//order by 
		ofs<<"\t strSelect"
			<<i
			<<"+=\" ";
		int nOrderSize=vecOrderFields.size();
		if(nOrderSize>0)
		{
			ofs<<" order by ";
			for ( int x=0;x<vecOrderFields.size();x++ )
			{
				ofs<<vecOrderFields[x];
				if ( x>0 && x!=vecOrderFields.size()-1 )
				{
					ofs<<" ,";
				}
			}
		}
		ofs<<"\";\n";
		
		


		ofs<<"\t s_select"
		   <<i
		   <<"->prepare(strSelect"
		   <<i
		   <<");\n\n";

	}
	ofs<<"\t}\n";
	
	ofs<<"\tif(flag==DELETE)\n"
		<<"\t{\n";
	for(int i=0;i<config.vecDeleteInfo.size();i++)
	{
		ofs<<"\t s_delete"
		   <<i
		   <<"= new Statement;\n"
		   <<"\t *s_delete"
		   <<i
		   <<"=conn_->createStatement();\n";
    
		vector<string>& vecWhereFields=config.vecDeleteInfo[i].vecWhereFields;
		ofs<<"\t string strDelete"
		   <<i
		   <<"=\"delete from \" ;\n"
		   <<"\t strDelete"
		   <<i
		   <<"+=tableName_;\n"
		   <<"\t strDelete"
		   <<i
		   <<"+=\" where ";
		for ( int j=0;j<vecWhereFields.size();j++ )
		{
			if ( j>0 )
			ofs<<" and ";

			ofs<<vecWhereFields[j]
				<<"=:f"
				<<j;
				
			if ( j<0 && j!=vecWhereFields.size()-1 )
			{
				ofs<<",";
			}
		}
		ofs<<"\";\n";
		ofs<<"\t s_delete"
		   <<i
		   <<"->prepare(strDelete"
		   <<i
		   <<");\n\n";
	}
	ofs<<"\t}\n";

	ofs<<"\tif(flag==UPDATE)\n"
	   <<"\t{\n";
	for(int i=0;i<config.vecUpdateInfo.size();i++)
	{
		ofs<<"\t s_update"
			<<i
			<<"= new Statement;\n"
			<<"\t *s_update"
			<<i
			<<"=conn_->createStatement();\n";
		vector<string>& vecUpdateFields=config.vecUpdateInfo[i].vecUpdateFields;
		vector<string>& vecWhereFields=config.vecUpdateInfo[i].vecWhereFields;
		ofs<<"\t string strUpdate"
			<<i
			<<"=\"update \" ;\n"
			<<"\t strUpdate"
			<<i
			<<"+=tableName_;\n"
			<<"\t strUpdate"
			<<i
			<<"+=\" set ";
		for ( int x=0;x<vecUpdateFields.size();x++ )
		{
			if ( x>0 )
				ofs<<" , ";

			ofs<<vecUpdateFields[x]
			<<"=:f"
			   <<x;
		}
		ofs<<" where ";
		for ( int z=0;z<vecWhereFields.size();z++ )
		{
			if ( z>0 )
				ofs<<" and ";

			ofs<<vecWhereFields[z]
			<<"=:f"
				<<(z+vecUpdateFields.size());
		}
		ofs<<" \";\n";
		ofs<<"\t s_update"
		   <<i
		   <<"->prepare(strUpdate"
		   <<i
		   <<");\n\n";
	}

	ofs<<"\t}\n";

	ofs<<"\thandle_ |= flag;\n"
		<<"}\n\n";


	for ( int i=0;i<config.vecSelectInfo.size();i++ )
	{
		CSelectInfo& info=config.vecSelectInfo[i];
		string strModfield=info.modField;
		ofs<<"int "
		   <<strHandleImpName
		   <<"::select"
			<<i
			<<"(";
		for ( int x=0;x<info.vecWhereFields.size();x++ )
		{
			string sField=info.vecWhereFields[x];
			CColumnInfo columnInfo;
			if ( config.GetColumnByName(sField,columnInfo) )
			{
				ofs<<"const ";
				if ( columnInfo.data_type=="NUMBER"  )
				{
					ofs<<"int64 & "
						<<"wh"
						<<sField;
				}
				else if ( columnInfo.data_type=="VARCHAR2" || columnInfo.data_type=="CHAR"  )
				{
					ofs<<"string& "
						<<"wh"
						<<sField;
				}
				else if ( columnInfo.data_type=="DATE"  )
				{
					ofs<<"OSSDateTime& "
						<<"wh"
						<<sField;
				}
				ofs<<",";								
			}
		}//for where end

		vector<string>& vecResultFields=info.vecResultFields;
		if ( vecResultFields.size()==config.vecClounmInfo.size() )
		{
			ofs<<"vector<"
				<<strClassName
				<<">& m_vec,const int& moddividend,const int& modresult,const int &rownum)\n ";
		}
		else
		{
			for ( int z=0;z<vecResultFields.size();z++ )
			{
				string sField=vecResultFields[z];
				CColumnInfo columnInfo;
				if ( config.GetColumnByName(sField,columnInfo) )
				{
					if ( columnInfo.data_type=="NUMBER"  )
					{
						ofs<<" int64& "
							<<"res"
							<<sField;
					}
					else if ( columnInfo.data_type=="VARCHAR2"|| columnInfo.data_type=="CHAR"   )
					{
						ofs<<"string& "
							<<"res"
							<<sField;
					}
					else if ( columnInfo.data_type=="DATE"  )
					{
						ofs<<"OSSDateTime& "
							<<"res"
							<<sField;
					}
					if ( z==vecResultFields.size()-1 )
					{
						ofs<<",const int& moddividend,const int& modresult ,const int &rownum)\n";
					}
					else
						ofs<<",";
				}
			}
		}

		ofs<<"{\n";
		ofs<<"\tthis->init1(SELECT,moddividend,modresult,rownum);\n";
		if ( vecResultFields.size()==config.vecClounmInfo.size() )
		{
			ofs<<"\tm_vec.clear();\n";
		}
		ofs<<"\tthis->s_select"
			<<i
			<<"->clearParams();\n";
		for ( int x=0;x<info.vecWhereFields.size();x++ )
		{
			string sField=info.vecWhereFields[x];
			ofs<<"\tthis->s_select"
				<<i
				<<"->setParam(\"f"
				<<x
				<<"\",";
			CColumnInfo columnInfo;
			if ( config.GetColumnByName(sField,columnInfo) && columnInfo.data_type=="NUMBER" )
			{				
					ofs<<"(double) "
						<<"wh"
						<<sField;				
			}
			else
			{
				ofs<<"wh"
					<<sField;
			}
				ofs<<");\n";
		}
		
		ofs<<"\tif(rownum>0)\n"
			<<"\t{\n"
			<<"\tthis->s_select"
			<<i
			<<"->setParam(\"f"
			<<info.vecWhereFields.size()
			<<"\","
			<<"rownum);\n";
		if ( strModfield.length()>0 )
		{
				ofs<<"\tthis->s_select"
				<<i
				<<"->setParam(\"f"
				<<info.vecWhereFields.size()+1
				<<"\","
				<<"moddividend);\n";

				ofs<<"\tthis->s_select"
					<<i
					<<"->setParam(\"f"
					<<info.vecWhereFields.size()+2
					<<"\","
					<<"modresult);\n";
		}
		ofs<<"\t}\n";

		ofs<<"\telse\n"
			<<"\t{\n";
		if ( strModfield.length()>0 )
		{
			ofs<<"\tthis->s_select"
				<<i
				<<"->setParam(\"f"
				<<info.vecWhereFields.size()
				<<"\","
				<<"moddividend);\n";

			ofs<<"\tthis->s_select"
				<<i
				<<"->setParam(\"f"
				<<info.vecWhereFields.size()+1
				<<"\","
				<<"modresult);\n";
		}
		ofs<<"\t}\n";


		ofs<<"\t this->s_select"
			<<i
			<<"->execute();\n";
		if ( vecResultFields.size()==config.vecClounmInfo.size() )
		{
			ofs<<"\t"
			   <<strClassName
			   <<" v;\n";
			ofs<<"\twhile(this->s_select"
				<<i
				<<"->next())\n"
				<<"\t{\n"
				<<"\t *this->s_select"
				<<i
				<<">>v;\n"
				<<"\t m_vec.push_back(v);\n"
				<<"\t}\n"
				<<"\t return m_vec.size();\n";
		}
		else
		{
			ofs<<"\t int nRes=0;\n\twhile(this->s_select"
			   <<i
			   <<"->next())\n"
			   <<"\t{\n"
			   <<"\t nRes++;\n";
				for ( int y=0;y<vecResultFields.size();y++ )
				{
					string sField=vecResultFields[y];
					CColumnInfo columnInfo;
					
					if ( config.GetColumnByName(sField,columnInfo) && columnInfo.data_type=="NUMBER" )
					{
						ofs<<"\t double tmp"
							<<y
							<<";\n";
						ofs<<"\t *s_select"
							<<i
							<<">>"
							<<"tmp"
							<<y
							<<";\n"
							<<"\t "
							<<"res"
							<<sField
							<<"=tmp"
							<<y
							<<";\n";	
					}
					else
					{
						ofs<<"\t *s_select"
							<<i
							<<">>"
							<<"res"
							<<vecResultFields[y]
						<<";\n";
					}
					
				}
			   ofs<<"\t}\n"
				  <<"\t return nRes;\n";
		}


		ofs<<"}\n";

	}

	
	//gen update body head 
	for ( int i=0;i<config.vecUpdateInfo.size();i++ )
	{
		CUpdateInfo& info=config.vecUpdateInfo[i];
		vector<string>& vecUpdateFields =  info.vecUpdateFields;
		vector<string>& vecWhereFields =  info.vecWhereFields;
		ofs<<"int "
			<<strHandleImpName
			<<"::update"
			<<i
			<<"(";
		if ( vecUpdateFields.size()==config.vecClounmInfo.size() )
		{
			ofs<<"const "
				<<strClassName
				<<" & v,";
		}
		else
		{
			for ( int z=0;z<vecUpdateFields.size();z++ )
			{
				string sField=vecUpdateFields[z];
				CColumnInfo columnInfo;
				if ( config.GetColumnByName(sField,columnInfo) )
				{
					if ( columnInfo.data_type=="NUMBER"  )
					{
						ofs<<"const int64& "
							<<"set"
							<<sField;
					}
					else if ( columnInfo.data_type=="VARCHAR2" || columnInfo.data_type=="CHAR"  )
					{
						ofs<<"const string& "
							<<"set"
							<<sField;
					}
					else if ( columnInfo.data_type=="DATE"  )
					{
						ofs<<"const OSSDateTime& "
							<<"set"
							<<sField;
					}
					ofs<<",";
				}
			}
		}

		if ( vecWhereFields.size()==config.vecClounmInfo.size() )
		{
			ofs<<"const "
				<<strClassName
				<<" & v1)\n";
		}
		else
		{
			for ( int z=0;z<vecWhereFields.size();z++ )
			{
				string sField=vecWhereFields[z];
				CColumnInfo columnInfo;
				if ( config.GetColumnByName(sField,columnInfo) )
				{
					if ( columnInfo.data_type=="NUMBER"  )
					{
						ofs<<"const int64& "
							<<"wh"
							<<sField;
					}
					else if ( columnInfo.data_type=="VARCHAR2"|| columnInfo.data_type=="CHAR"   )
					{
						ofs<<"const string& "
							<<"wh"
							<<sField;
					}
					else if ( columnInfo.data_type=="DATE"  )
					{
						ofs<<"const OSSDateTime& "
							<<"wh"
							<<sField;
					}
					if ( z==vecWhereFields.size()-1 )
					{
						ofs<<")\n";
					}
					else
						ofs<<",";
				}
			}
		}
		//
		ofs<<"{\n";
		ofs<<"\tthis->init1(UPDATE);\n"
		   <<"\tthis->s_update"
		   <<i
		   <<"->clearParams();\n";
		if ( vecUpdateFields.size()==config.vecClounmInfo.size() )
		{
			for ( int z=0;z<vecUpdateFields.size();z++ )
			{
				string sField=vecUpdateFields[z];
				CColumnInfo columnInfo;
				if ( config.GetColumnByName(sField,columnInfo) )
				{
					if ( columnInfo.data_type=="NUMBER"  )
					{
						ofs<<"\ts_update"
							<<i
							<<"->setParam(\"f"
							<<z
							<<"\",(double)"
							<<"v."
							<<columnInfo.member_suffix
							<<"());\n";

					}
					else if ( columnInfo.data_type=="VARCHAR2"|| columnInfo.data_type=="CHAR"   )
					{
						ofs<<"\ts_update"
							<<i
							<<"->setParam(\"f"
							<<z
							<<"\","
							<<"v."
							<<columnInfo.member_suffix
							<<"());\n";
					}
					else if ( columnInfo.data_type=="DATE"  )
					{
						ofs<<"\ts_update"
							<<i
							<<"->setParam(\"f"
							<<z
							<<"\","
							<<"v."
							<<columnInfo.member_suffix
							<<"());\n";
					}

				}
			}
		}
		else
		{
			for ( int z=0;z<vecUpdateFields.size();z++ )
			{
				string sField=vecUpdateFields[z];
				CColumnInfo columnInfo;
				if ( config.GetColumnByName(sField,columnInfo) )
				{
					if ( columnInfo.data_type=="NUMBER"  )
					{
						ofs<<"\ts_update"
							<<i
							<<"->setParam(\"f"
							<<z
							<<"\",(double)"
							<<"set"
							<<sField
							<<");\n";

					}
					else if ( columnInfo.data_type=="VARCHAR2" || columnInfo.data_type=="CHAR"  )
					{
						ofs<<"\ts_update"
							<<i
							<<"->setParam(\"f"
							<<z
							<<"\","
							<<"set"
							<<sField
							<<");\n";

					}
					else if ( columnInfo.data_type=="DATE"  )
					{
						ofs<<"\ts_update"
							<<i
							<<"->setParam(\"f"
							<<z
							<<"\","
							<<"set"
							<<sField
							<<");\n";

					}

				}
			}
		}
		
		//where bind
		if ( vecWhereFields.size()==config.vecClounmInfo.size() )
		{
			for ( int z=0;z<vecWhereFields.size();z++ )
			{
				string sField=vecWhereFields[z];
				CColumnInfo columnInfo;
				if ( config.GetColumnByName(sField,columnInfo) )
				{
					if ( columnInfo.data_type=="NUMBER"  )
					{
						ofs<<"\ts_update"
							<<i
							<<"->setParam(\"f"
							<<z+vecUpdateFields.size()
							<<"\",(double)"
							<<"v1."
							<<columnInfo.member_suffix
							<<"());\n";

					}
					else if ( columnInfo.data_type=="VARCHAR2" || columnInfo.data_type=="CHAR"  )
					{
						ofs<<"\ts_update"
							<<i
							<<"->setParam(\"f"
							<<z+vecUpdateFields.size()
							<<"\","
							<<"v1."
							<<columnInfo.member_suffix
							<<"());\n";
					}
					else if ( columnInfo.data_type=="DATE"  )
					{
						ofs<<"\ts_update"
							<<i
							<<"->setParam(\"f"
							<<z+vecUpdateFields.size()
							<<"\","
							<<"v1."
							<<columnInfo.member_suffix
							<<"());\n";
					}

				}
			}
		}
		else
		{
			for ( int z=0;z<vecWhereFields.size();z++ )
			{
				string sField=vecWhereFields[z];
				CColumnInfo columnInfo;
				if ( config.GetColumnByName(sField,columnInfo) )
				{
					if ( columnInfo.data_type=="NUMBER"  )
					{
						ofs<<"\ts_update"
							<<i
							<<"->setParam(\"f"
							<<z+vecUpdateFields.size()
							<<"\",(double)"
							<<"wh"
							<<sField
							<<");\n";

					}
					else if ( columnInfo.data_type=="VARCHAR2" || columnInfo.data_type=="CHAR"  )
					{
						ofs<<"\ts_update"
							<<i
							<<"->setParam(\"f"
							<<z+vecUpdateFields.size()
							<<"\","
							<<"wh"
							<<sField
							<<");\n";

					}
					else if ( columnInfo.data_type=="DATE"  )
					{
						ofs<<"\ts_update"
							<<i
							<<"->setParam(\"f"
							<<z+vecUpdateFields.size()
							<<"\","
							<<"wh"
							<<sField
							<<");\n";

					}

				}
			}
		}
		
		ofs<<"\treturn this->s_update"
		   <<i
			<<"->execute();\n";

		ofs<<"}\n";

	}//get update function body  end

	
	//gen delete function body begin
	for ( int i=0; i<config.vecDeleteInfo.size();i++ )
	{
		ofs<<"int "
			<<strHandleImpName
			<<"::delete"
			<<i
			<<"(";	
		vector<string>& vecWhereFields =config.vecDeleteInfo[i].vecWhereFields;
		if ( vecWhereFields.size()==config.vecClounmInfo.size() )
		{
			ofs<<" const "
				<<strClassName
				<<" & v)\n";
		}
		else
		{
			for ( int z=0;z<vecWhereFields.size();z++ )
			{
				string sField=vecWhereFields[z];
				CColumnInfo columnInfo;
				if ( config.GetColumnByName(sField,columnInfo) )
				{
					if ( columnInfo.data_type=="NUMBER"  )
					{
						ofs<<"const int64& "
							<<"wh"
							<<sField;
					}
					else if ( columnInfo.data_type=="VARCHAR2" || columnInfo.data_type=="CHAR"  )
					{
						ofs<<"const string& "
							<<"wh"
							<<sField;
					}
					else if ( columnInfo.data_type=="DATE"  )
					{
						ofs<<"const OSSDateTime& "
							<<"wh"
							<<sField;
					}
					if ( z==vecWhereFields.size()-1 )
					{
						ofs<<")\n";
					}
					else
						ofs<<",";
				}
			}

		}
		ofs<<"{\n";
		ofs<<"\tthis->init1(DELETE);\n"
			<<"\tthis->s_delete"
			<<i
			<<"->clearParams();\n";
		
		if ( vecWhereFields.size()==config.vecClounmInfo.size() )
		{
			
				for ( int z=0;z<vecWhereFields.size();z++ )
				{
					ofs<<"\tthis->s_delete"
						<<i
						<<"->setParam(\"f"
						<<z
						<<"\",";
					string sField=vecWhereFields[z];
					CColumnInfo columnInfo;
					if ( config.GetColumnByName(sField,columnInfo) )
					{
						if ( columnInfo.data_type=="NUMBER"  )
						{
							ofs<<"(double) v."
								<<columnInfo.member_suffix
								<<"());\n";
						}
						else 
						{
							ofs<<" v."
								<<columnInfo.member_suffix
								<<"());\n";
						}						
					}
				}

				
		}
		else
		{

			for ( int z=0;z<vecWhereFields.size();z++ )
			{
				ofs<<"\tthis->s_delete"
					<<i
					<<"->setParam(\"f"
					<<z
					<<"\",";

				string sField=vecWhereFields[z];
				CColumnInfo columnInfo;
				if ( config.GetColumnByName(sField,columnInfo) )
				{
					if ( columnInfo.data_type=="NUMBER"  )
					{
						ofs<<"(double) "
							<<"wh"
							<<sField;
					}
					else 
					{
						ofs<<"const OSSDateTime& "
							<<"wh"
							<<sField;
					}
					ofs<<");\n";
				}
			}

		}
		ofs<<"\t return s_delete"
		   <<i
		   <<"->execute();\n";


		ofs<<"}\n";
	}//for 
	
	//get delete func body end

	ofs<<"\n\n";

}
void GenHandleHeadFile(ofstream& ofs,CGlobalConfig& config)
{
	string strClassName=config.str_class_name;
	string strStructName=config.str_struct_h_name;
	string strHandleImpName=config.str_handleimp_class_name;
	string strHandleName=config.str_handle_class_name;
	vector<CColumnInfo>& m_vecColumn = config.vecClounmInfo;
	//头文件开头
	ofs << "#ifndef __"
		<<strClassName
		<<"__HANDLE_H__ \n"
		<< "#define __"
		<<strClassName
		<<"__HANDLE_H__\n\n"
		<<"#include \"Handle.h\" \n"
		<<"#include \""
		<<strStructName
		<<"\" \n\n";
	
	//class define start
	ofs<<"class "
	   <<strHandleImpName
	   <<" \n"
	   <<"{\n"
		<<"public:\n";

	//construct 
	ofs<<"\t"
	   <<strHandleImpName
	   <<"(Connection* conn)\n"
	   <<"\t"
	   <<":conn_"
	   <<"(conn)\n"
	   <<"\t{\n"
		<<"\t handle_=0;\n"
		<<"\t s_insert=NULL;\n";
	for ( int i=0;i<config.vecSelectInfo.size();i++ )
	{
		ofs<<"\t s_select"
			<<i
			<<"=NULL;\n";
	}
	for ( int i=0;i<config.vecDeleteInfo.size();i++ )
	{
		ofs<<"\t s_delete"
			<<i
			<<"=NULL;\n";
	}
	for ( int i=0;i<config.vecUpdateInfo.size();i++ )
	{
		ofs<<"\t s_update"
			<<i
			<<"=NULL;\n";
	}
	 ofs<<"\t}\n\n";

	//destruct
	ofs<<"\t~"
		<<strHandleImpName
		<<"(void);\n\n";
	
	//insert 
	ofs<<"\t int insert(const "
	   <<strClassName
	   <<"& v);\n\n";

	//gen select function
	for ( int i=0;i<config.vecSelectInfo.size();i++ )
	{
		CSelectInfo& info=config.vecSelectInfo[i];
		vector<string>& vecResultFields =  info.vecResultFields;
		string strModField=info.modField;
		ofs<<"\t int select"
		   <<i
		   <<"(";
		for ( int x=0;x<info.vecWhereFields.size();x++ )
		{
			string sField=info.vecWhereFields[x];
			CColumnInfo columnInfo;
			if ( config.GetColumnByName(sField,columnInfo) )
			{
				ofs<<"const ";
				if ( columnInfo.data_type=="NUMBER"  )
				{
					ofs<<"int64 & "
						<<"wh"
					   <<sField;
				}
				else if ( columnInfo.data_type=="VARCHAR2" || columnInfo.data_type=="CHAR"  )
				{
					ofs<<"string& "
						<<"wh"
						<<sField;
				}
				else if ( columnInfo.data_type=="DATE"  )
				{
					ofs<<"OSSDateTime& "
						<<"wh"
						<<sField;
				}
				ofs<<",";								
			}
		}//for where end
		if ( vecResultFields.size()==config.vecClounmInfo.size() )
		{
			ofs<<"vector<"
				<<strClassName
				<<">& m_vec ,const int& moddividend=0,const int& modresult=0 ,const int &rownum=0);\n ";
		}
		else
		{
			for ( int z=0;z<vecResultFields.size();z++ )
			{
				string sField=vecResultFields[z];
				CColumnInfo columnInfo;
				if ( config.GetColumnByName(sField,columnInfo) )
				{
					if ( columnInfo.data_type=="NUMBER"  )
					{
						ofs<<"int64& "
							<<"res"
							<<sField;
					}
					else if ( columnInfo.data_type=="VARCHAR2" || columnInfo.data_type=="CHAR"  )
					{
						ofs<<"string& "
							<<"res"
							<<sField;
					}
					else if ( columnInfo.data_type=="DATE"  )
					{
						ofs<<"OSSDateTime& "
							<<"res"
							<<sField;
					}
					if ( z==vecResultFields.size()-1 )
					{
						ofs<<",const int& moddividend=0,const int& modresult=0 ,const int &rownum=0);\n";
					}
					else
						ofs<<",";
				}
			}
		}
	}

	//gen update function head 
	for ( int i=0;i<config.vecUpdateInfo.size();i++ )
	{
		CUpdateInfo& info=config.vecUpdateInfo[i];
		vector<string>& vecUpdateFields =  info.vecUpdateFields;
		vector<string>& vecWhereFields =  info.vecWhereFields;
		ofs<<"\t int update"
			<<i
			<<"(";
		if ( vecUpdateFields.size()==config.vecClounmInfo.size() )
		{
			ofs<<"const "
			   <<strClassName
			   <<" & v,";
		}
		else
		{
			for ( int z=0;z<vecUpdateFields.size();z++ )
			{
				string sField=vecUpdateFields[z];
				CColumnInfo columnInfo;
				if ( config.GetColumnByName(sField,columnInfo) )
				{
					if ( columnInfo.data_type=="NUMBER"  )
					{
						ofs<<"const int64& "
							<<"set"
							<<sField;
					}
					else if ( columnInfo.data_type=="VARCHAR2"|| columnInfo.data_type=="CHAR"   )
					{
						ofs<<"const string& "
							<<"set"
							<<sField;
					}
					else if ( columnInfo.data_type=="DATE"  )
					{
						ofs<<"const OSSDateTime& "
							<<"set"
							<<sField;
					}
					ofs<<",";
				}
			}
		}

		if ( vecWhereFields.size()==config.vecClounmInfo.size() )
		{
			ofs<<"const "
				<<strClassName
				<<" & v1);\n";
		}
		else
		{
			for ( int z=0;z<vecWhereFields.size();z++ )
			{
				string sField=vecWhereFields[z];
				CColumnInfo columnInfo;
				if ( config.GetColumnByName(sField,columnInfo) )
				{
					if ( columnInfo.data_type=="NUMBER"  )
					{
						ofs<<"const int64& "
							<<"wh"
							<<sField;
					}
					else if ( columnInfo.data_type=="VARCHAR2"|| columnInfo.data_type=="CHAR"   )
					{
						ofs<<"const string& "
							<<"wh"
							<<sField;
					}
					else if ( columnInfo.data_type=="DATE"  )
					{
						ofs<<"const OSSDateTime& "
							<<"wh"
							<<sField;
					}
					if ( z==vecWhereFields.size()-1 )
					{
						ofs<<");\n";
					}
					else
						ofs<<",";
				}
			}
		}
	}//gen update function header  end
	
	//gen delete function header begin
	for ( int i=0; i<config.vecDeleteInfo.size();i++ )
	{
		ofs<<"\t int delete"
		   <<i
		   <<"(";	
		vector<string>& vecWhereFields =config.vecDeleteInfo[i].vecWhereFields;
		if ( vecWhereFields.size()==config.vecClounmInfo.size() )
		{
			ofs<<" const "
			   <<strClassName
			   <<" & v);\n\n";
		}
		else
		{
			for ( int z=0;z<vecWhereFields.size();z++ )
			{
				string sField=vecWhereFields[z];
				printf("sField=%s \n",sField.c_str());
				CColumnInfo columnInfo;
				if ( config.GetColumnByName(sField,columnInfo) )
				{
					if ( columnInfo.data_type=="NUMBER"  )
					{
						printf("sField NUMBER=%s \n",sField.c_str());
						ofs<<"const int64& "
							<<"wh"
							<<sField;
					}
					else if ( columnInfo.data_type=="VARCHAR2" || columnInfo.data_type=="CHAR"  )
					{
						ofs<<"const string& "
							<<"wh"
							<<sField;
					}
					else if ( columnInfo.data_type=="DATE"  )
					{
						ofs<<"const OSSDateTime& "
							<<"wh"
							<<sField;
					}
					if ( z==vecWhereFields.size()-1 )
					{
						printf("sField end=%s \n",sField.c_str());
						ofs<<");\n\n";
					}
					else
						ofs<<",";
				}
			}
		}

	}
	//get delete func header end
	
	

	//gen table name function
	ofs<<"\n\tvoid tableName(const string& tableName)\n"
		<<"\t{\n"
		<<"\t tableName_=tableName;\n"
		<<"\t}\n\n";
	


	//protected function
	ofs<<"protected:\n"
	   <<"\tvoid init1(int flag,int moddividend=0,int modresult=0,int rownum=0);\n\n";
	
	ofs<<"\t int handle_;\n\n";
	ofs<<"\t Connection* conn_;\n\n";
	ofs<<"\t Statement *s_insert;\n\n";
	ofs<<"\t string tableName_;\n\n";

	for ( int i=0;i<config.vecSelectInfo.size();i++ )
	{
		ofs<<"\t Statement *s_select"
		   <<i
		   <<";\n\n";
	}
	for ( int i=0;i<config.vecDeleteInfo.size();i++ )
	{
		ofs<<"\t Statement *s_delete"
		   <<i
		   <<";\n\n";
	}
	for ( int i=0;i<config.vecUpdateInfo.size();i++ )
	{
		ofs<<"\t Statement *s_update"
			<<i
			<<";\n\n";
	}

	/*
	//select 
	ofs<<"\t"
		<<"int select(const BindPara &para,vector<"
		<<strClassName
		<<"> &array);\n\n";
	*/

	//gen virtrual function
	ofs<<"\tint select(const int64 &id,"
		<<strClassName
		<<" &v){return 0;}\n";
	ofs<<"\tint select(const BindPara &para,vector<"
		<<strClassName
		<<"> &v){return 0;}\n";
	ofs<<"\tint update(const "
		<<strClassName
		<<" &v){return 0;}\n";
	ofs<<"\tint Delete(const "
		<<strClassName
		<< " &v){return 0;}\n";
	ofs<<"\tint modify(const "
		<<strClassName
		<<" &v){return 0;}\n";
	ofs<<"\tvoid init(int flag){ return;}\n";


	//class end
	ofs<<"};\n\n";

	//typedef 
	ofs<<"typedef "
		
		<<strHandleImpName
		<<" "
		<<strHandleName
		<<";\n\n";


	//头文件结尾
	ofs<<"#endif \n\n";
}

void GenDataHeadFile(ofstream& ofs,CGlobalConfig& config)
{

	std::string strClassName=config.str_class_name;
	vector<CColumnInfo>& m_vecColumn = config.vecClounmInfo;
	//头文件
	ofs << "#ifndef __"
		<<strClassName
		<<"__H__ \n"
		<< "#define __"
		<<strClassName
		<<"__H__\n\n"
		<< "#include <string> \n"
		<< "#include <datetime.h> \n"
		<< "#include <iostream> \n"
		<< "#include <aidb3.h> \n"
		<< "#include <sstream> \n\n"
		<< "using std::ostream; \n"
		<< "using std::stringstream; \n"
		<< "using std::cout; \n"
		<< "using std::endl; \n"
		<< "using std::string; \n"
		<< "using aidb3::Statement; \n\n";

	//类定义开始
	ofs <<"class "
		<<strClassName
		<<" \n"
		<<"{ \n"

		//construct func
		<<"public: \n"
		<<"\t"
		<<strClassName
		<<"(); \n"

		//copy construct func
		<<"\t"
		<<strClassName
		<<"(const "
		<<strClassName
		<<" & rhs);\n"

		//= operator 
		<<"\t"
		<<strClassName
		<<"& operator = (const "
		<<strClassName
		<<" & rhs);\n"

		//destruct
		<<"\t~"
		<<strClassName
		<<"();\n";

	//member variant define
	ofs<<"private: \n";
	for ( int i=0;i<m_vecColumn.size();i++ )
	{
		if ( m_vecColumn[i].data_type=="NUMBER" )
		{
			ofs<<"\tint64 m_n"
				<<m_vecColumn[i].member_suffix
				<<"; \n";
		}
		else if ( m_vecColumn[i].data_type=="VARCHAR2"|| m_vecColumn[i].data_type=="CHAR"  )
		{
			ofs<<"\tstring m_str"
				<<m_vecColumn[i].member_suffix
				<<"; \n";
		}
		else if ( m_vecColumn[i].data_type=="DATE" )
		{
			ofs<<"\tOSSDateTime m_Date"
				<<m_vecColumn[i].member_suffix
				<<"; \n";
		}

	}

	//member variant get set func def
	ofs<<"public: \n";
	for ( int i=0;i<m_vecColumn.size();i++ )
	{
		if ( m_vecColumn[i].data_type=="NUMBER" )
		{
			ofs<<"\tconst int64& "
				<<m_vecColumn[i].member_suffix
				<<"() const {"
				<<" return m_n" 
				<<m_vecColumn[i].member_suffix
				<<"; "
				<<"} \n"

				<<"\tvoid "
				<<m_vecColumn[i].member_suffix
				<<"( const int64 & "
				<<m_vecColumn[i].member_suffix
				<<"){"
				<<" m_n"
				<<m_vecColumn[i].member_suffix
				<<"="
				<<m_vecColumn[i].member_suffix
				<<"; } \n";

		}
		else if ( m_vecColumn[i].data_type=="VARCHAR2"|| m_vecColumn[i].data_type=="CHAR" )
		{
			ofs<<"\tconst string& "
				<<m_vecColumn[i].member_suffix
				<<"() const {"
				<<" return m_str" 
				<<m_vecColumn[i].member_suffix
				<<"; "
				<<"} \n"

				<<"\tvoid "
				<<m_vecColumn[i].member_suffix
				<<"(const string & "
				<<m_vecColumn[i].member_suffix
				<<"){"
				<<" m_str"
				<<m_vecColumn[i].member_suffix
				<<"="
				<<m_vecColumn[i].member_suffix
				<<"; } \n";
		}
		else if ( m_vecColumn[i].data_type=="DATE" )
		{
			ofs<<"\tconst OSSDateTime& "
				<<m_vecColumn[i].member_suffix
				<<"() const {"
				<<" return m_Date" 
				<<m_vecColumn[i].member_suffix
				<<"; "
				<<"} \n"

				<<"\tvoid "
				<<m_vecColumn[i].member_suffix
				<<"( const OSSDateTime & "
				<<m_vecColumn[i].member_suffix
				<<"){"
				<<" m_Date"
				<<m_vecColumn[i].member_suffix
				<<"="
				<<m_vecColumn[i].member_suffix
				<<"; } \n\n";
		}

	}

	//template print
	ofs<<"\ttemplate<typename t> \n"
		<<"\t t & print(t& os)\n"
		<<"\t{ \n"
		<<"\t\t os<<\"{\" \n";
	for ( int i=0;i<m_vecColumn.size();i++ )
	{
		if ( m_vecColumn[i].data_type=="NUMBER" )
		{

			ofs<<"\t\t  <<\""
				<<"m_n"
				<<m_vecColumn[i].member_suffix
				<<":\""
				<<"<<"
				<<"m_n"
				<<m_vecColumn[i].member_suffix
				<<"<<\",\" \n";

		}
		else if ( m_vecColumn[i].data_type=="VARCHAR2" || m_vecColumn[i].data_type=="CHAR"  )
		{

			ofs<<"\t\t  <<\""
				<<"m_str"
				<<m_vecColumn[i].member_suffix
				<<":\""
				<<"<<"
				<<"m_str"
				<<m_vecColumn[i].member_suffix
				<<"<<\",\" \n";
		}
		else if ( m_vecColumn[i].data_type=="DATE" )
		{
			ofs<<"\t\t  <<\""
				<<"m_Date"
				<<m_vecColumn[i].member_suffix
				<<":\""
				<<"<<"
				<<"m_Date"
				<<m_vecColumn[i].member_suffix
				<<".toString().c_str()"
				<<"<<\",\" \n";
		}
	}
	ofs<<"\t\t<<\"} \\n\"\n"
		<<"\t\t<<endl;\n"
		<<"\t\t return os;\n"
		<<"\t } \n\n";


	//template flush
	ofs<<"\ttemplate<typename T> \n"
		<<"\t T & flush(T& stat)\n"
		<<"\t{ \n"
		<<"\t\tstat\n";
	for ( int i=0;i<m_vecColumn.size();i++ )
	{	
		if ( m_vecColumn[i].data_type=="NUMBER" )
		{

			ofs<<"\t\t<<"
				<<"(double) m_n"
				<<m_vecColumn[i].member_suffix;

		}
		else if ( m_vecColumn[i].data_type=="VARCHAR2"|| m_vecColumn[i].data_type=="CHAR"  )
		{

			ofs<<"\t\t<<"
				<<"m_str"
				<<m_vecColumn[i].member_suffix;
		}
		else if ( m_vecColumn[i].data_type=="DATE" )
		{
			ofs<<"\t\t<<"
				<<"m_Date"
				<<m_vecColumn[i].member_suffix;
		}
		if ( i==m_vecColumn.size()-1 )
		{
			ofs<<";\n";
		}
		else
		{
			ofs<<"\n";
		}

	}	
	ofs<<"\t\t return stat;\n"
		<<"\t } \n\n";


	//fetch function
	ofs<<"\t virtual Statement & fetch(Statement &stat); \n\n";

	//operator << ostream
	ofs<<"\t friend ostream &operator <<(ostream &os,"
		<<strClassName
		<<" &out);\n\n";

	//operator >> Statement
	ofs<<"\t friend Statement &operator >>(Statement &stat,"
		<<strClassName
		<<" &in);\n\n";


	//operator <<(stringstream
	ofs<<"\t friend stringstream &operator <<(stringstream &os,"
		<<strClassName
		<<" &out);\n\n";


	//类定义结尾
	ofs<<"}; \n"
		<<"#endif \n\n";
}


void GenDataBodyFile(ofstream& ofsp,CGlobalConfig& config)
{
	std::string strClassName=config.str_class_name;
	vector<CColumnInfo>& m_vecColumn = config.vecClounmInfo;

	string strHFileName=config.str_struct_h_name;

	ofsp <<"\n\n"
		<<"#include \""
		<<strHFileName
		<<"\" \n\n";

	//default construct
	ofsp<<strClassName
		<<"::"
		<<strClassName
		<<"()\n"
		<<"{\n";
	for ( int i=0;i<m_vecColumn.size();i++ )
	{
		if ( m_vecColumn[i].data_type=="NUMBER" )
		{

			ofsp<<"\tm_n"
				<<m_vecColumn[i].member_suffix
				<<"=0;\n";
		}
		else if ( m_vecColumn[i].data_type=="VARCHAR2" || m_vecColumn[i].data_type=="CHAR")
		{

			ofsp<<"\tm_str"
				<<m_vecColumn[i].member_suffix
				<<"=\"\";\n";
		}
		else if ( m_vecColumn[i].data_type=="DATE" )
		{
			ofsp<<"\tm_Date"
				<<m_vecColumn[i].member_suffix
				<<"=OSSDateTime::currentDateTime() ;\n";

		}
	}
	ofsp<<"}\n\n";

	//copy construct
	ofsp<<strClassName
		<<"::"
		<<strClassName
		<<"(const "
		<<strClassName
		<<"& rhs)\n"
		<<"{\n";
	for ( int i=0;i<m_vecColumn.size();i++ )
	{
		if ( m_vecColumn[i].data_type=="NUMBER" )
		{

			ofsp<<"\tm_n"
				<<m_vecColumn[i].member_suffix
				<<"=rhs.m_n"
				<<m_vecColumn[i].member_suffix
				<<";\n";
		}
		else if ( m_vecColumn[i].data_type=="VARCHAR2" || m_vecColumn[i].data_type=="CHAR")
		{

			ofsp<<"\tm_str"
				<<m_vecColumn[i].member_suffix
				<<"=rhs.m_str"
				<<m_vecColumn[i].member_suffix
				<<";\n";
		}
		else if ( m_vecColumn[i].data_type=="DATE" )
		{
			ofsp<<"\tm_Date"
				<<m_vecColumn[i].member_suffix
				<<"=rhs.m_Date"
				<<m_vecColumn[i].member_suffix
				<<";\n";

		}
	}
	ofsp<<"}\n\n";

	//destruct function
	ofsp<<strClassName
		<<"::~"
		<<strClassName
		<<"()\n{\n}\n\n";


	//operator =
	ofsp<<strClassName
		<<"& "
		<<strClassName
		<<"::operator = (const "
		<<strClassName
		<<"& rhs)\n{\n";
	for ( int i=0;i<m_vecColumn.size();i++ )
	{
		if ( m_vecColumn[i].data_type=="NUMBER" )
		{

			ofsp<<"\tm_n"
				<<m_vecColumn[i].member_suffix
				<<"=rhs.m_n"
				<<m_vecColumn[i].member_suffix
				<<";\n";
		}
		else if ( m_vecColumn[i].data_type=="VARCHAR2" || m_vecColumn[i].data_type=="CHAR")
		{

			ofsp<<"\tm_str"
				<<m_vecColumn[i].member_suffix
				<<"=rhs.m_str"
				<<m_vecColumn[i].member_suffix
				<<";\n";
		}
		else if ( m_vecColumn[i].data_type=="DATE" )
		{
			ofsp<<"\tm_Date"
				<<m_vecColumn[i].member_suffix
				<<"=rhs.m_Date"
				<<m_vecColumn[i].member_suffix
				<<";\n";
		}

	}
	ofsp<<"\treturn *this;\n";
	ofsp<<"}\n\n";

	//fetch function
	ofsp<<"Statement & "
		<<strClassName
		<<"::fetch(Statement &stat)\n{\n";

	for ( int i=0;i<m_vecColumn.size();i++ )
	{

		if ( m_vecColumn[i].data_type=="NUMBER" )
		{

			ofsp<<"\tdouble n"
				<<m_vecColumn[i].member_suffix;
		}
		else if ( m_vecColumn[i].data_type=="VARCHAR2" || m_vecColumn[i].data_type=="CHAR")
		{

			ofsp<<"\tstring str"
				<<m_vecColumn[i].member_suffix;
		}
		else if ( m_vecColumn[i].data_type=="DATE" )
		{
			ofsp<<"\tOSSDateTime Date"
				<<m_vecColumn[i].member_suffix;
		}
		ofsp<<";\n";
	}



	ofsp<<"\tstat";
	for ( int i=0;i<m_vecColumn.size();i++ )
	{
		
		if ( m_vecColumn[i].data_type=="NUMBER" )
		{

		ofsp<<">>n"
		<<m_vecColumn[i].member_suffix;
		}
		else if ( m_vecColumn[i].data_type=="VARCHAR2" || m_vecColumn[i].data_type=="CHAR")
		{

		ofsp<<">>str"
		<<m_vecColumn[i].member_suffix;
		}
		else if ( m_vecColumn[i].data_type=="DATE" )
		{
		ofsp<<">>Date"
		<<m_vecColumn[i].member_suffix;
		}
		if ( i==m_vecColumn.size()-1 )
		{
		ofsp<<";\n";
		}
		else
		{
		ofsp<<"\n\t";
		}
		
	}
	for ( int i=0;i<m_vecColumn.size();i++ )
	{
		if ( m_vecColumn[i].data_type=="NUMBER" )
		{

			ofsp<<"\t"
				<<"m_n"
				<<m_vecColumn[i].member_suffix
				<<"="
				<<"n"
				<<m_vecColumn[i].member_suffix
				<<";\n";
		}
		else if ( m_vecColumn[i].data_type=="VARCHAR2"|| m_vecColumn[i].data_type=="CHAR" )
		{
			ofsp<<"\t"
				<<"m_str"
				<<m_vecColumn[i].member_suffix
				<<"="
				<<"str"
				<<m_vecColumn[i].member_suffix
				<<";\n";
		
		}
		else if ( m_vecColumn[i].data_type=="DATE" )
		{
			ofsp<<"\t"
				<<"m_Date"
				<<m_vecColumn[i].member_suffix
				<<"="
				<<"Date"
				<<m_vecColumn[i].member_suffix
				<<";\n";
		}
	
	}

	ofsp<<"\treturn stat;\n";
	ofsp<<"}\n\n";

	//operator>>
	ofsp<<"Statement & operator>>( Statement &stat,"
		<<strClassName 
		<<"& in)\n"
		<<"{\n"
		<<"\tin.fetch(stat);\n"
		<<"\treturn stat;\n"
		<<"}\n\n";


	//operator<<( ostream &os
	ofsp<<"ostream & operator<<( ostream &os,"
		<<strClassName
		<<"& out)\n"
		<<"{\n"
		<<"\tout.print(os);\n"
		<<"\treturn os;\n"
		<<"}\n\n";

	//operator<<( stringstream &os
	ofsp<<"stringstream & operator<<( stringstream &os,"
		<<strClassName
		<<"& out)\n"
		<<"{\n"
		<<"\tout.print(os);\n"
		<<"\treturn os;\n"
		<<"}\n\n";

	//operator <<(Statement &stat
	ofsp<<"Statement &operator <<(Statement &stat,"
		<<strClassName
		<<"& in)\n"
		<<"{\n"
		<<"\tin.flush(stat);\n"
		<<"\treturn stat;\n"
		<<"}\n\n";
}

int main(int argc, char * argv[])
{
	
	CGlobalConfig gConfig;
	int ch;
	string program_name = OSS::basename(argv[0]);

	Get_Opt get_opt(argc,argv,OSS_LIB_TEXT (":h:c:"),0);
	for(int c;(c = get_opt())!=-1;) {
		switch(c) 
		{
			case 'c':
				gConfig.str_config_name = get_opt.opt_arg();
				break;
			case 'h':
				usage(program_name.c_str());
				return 1;
			default:
				break;
		}
	}

	if ( gConfig.str_config_name.empty()) 
	{
		usage(program_name.c_str());
		return 1;
	}


	char * home =NULL;
	home=getenv("AIOSS_HOME");
	if ( NULL==home )
	{
		cout<<"AIOSS_HOME not set!\n";
		return 1;
	}
	string strHome=home;
	strHome=strHome+"/etc/";
	gConfig.str_config_name=strHome+gConfig.str_config_name;
	cout<<"configure file path="<<gConfig.str_config_name.c_str()<<endl;
	
	//get configure info
	IniFile config;
	if ( !config.open(gConfig.str_config_name) )
	{
		cout<<"open configure file fail :"<<gConfig.str_config_name.c_str()<<endl;
		return 1;
	}
	if(config.valueExists("TABLE","table_name"))
		gConfig.str_table_name= config.readString("TABLE","table_name");
	if(config.valueExists("TABLE","prefix"))
		gConfig.str_prefix_name= config.readString("TABLE","prefix");
	
	if ( gConfig.str_table_name.empty()) 
	{
		cout<<"table_name not set!\n";
		return 1;
	}
	if ( gConfig.str_prefix_name.empty() )//CPreFixData.h CPreFixData.cpp
	{
		gConfig.str_prefix_name=gConfig.str_table_name;
	}

	try
	{
		OSSCONNECTION::instance()->init();
		Connection* oss = OSSCONNECTION::instance()->conn();
		
		//define vector to receive oracle data
		//vector<CColumnInfo> m_vecColumn;
		//query table column infomations
		string str_table_column="SELECT lower(column_name),data_type,data_length FROM all_tab_columns t where table_name=upper('";
		str_table_column+=gConfig.str_table_name;
		str_table_column+="') order by column_id";
		printf("str_table_column=%s \n",str_table_column.c_str());
		
		Statement select_;
		select_ = oss->createStatement();
		select_.prepare(str_table_column);
		select_.execute();
		while (select_.next()) 
		{
			CColumnInfo info;
			select_>>info.column_name;
			select_>>info.data_type;
			select_>>info.data_length;
			info.member_suffix=info.column_name;
			static int static_int=1;
			printf("static_int=%d,column_name= %s\n",static_int,info.column_name.c_str());
			static_int++;
			//开头字母大写，_号后面第一个字符大写
			int nFlagPos=0;
			int nFlag=0;
			for ( int i=0;i<info.member_suffix.size();i++ )
			{
				//printf("suffix= %d\n",info.member_suffix[i]);
				if ( i==0 )
				{
					info.member_suffix[i]=info.member_suffix[i]-32;
				}
				if ( info.member_suffix[i]=='_' )
				{
					nFlagPos=i;
					nFlag=1;

				}
				if ( i==nFlagPos+1 && nFlag==1)
				{
					info.member_suffix[i]=info.member_suffix[i]-32;
					nFlagPos=0;
					nFlag=0;
				}				
			}

			gConfig.vecClounmInfo.push_back(info);
			
		}
		if ( gConfig.vecClounmInfo.size()==0 )
		{
			printf("表无列信息，请确认输入表名是否正确\n");
		}
		OSSCONNECTION::instance()->fini();
		
		//
		//init select info
		char* pConfigSelect="SELECT";
		char strConfigSelect[10];
		for ( int i=0;i<9;i++ )
		{
			sprintf(strConfigSelect,"%s%d",pConfigSelect,i);
			string strFileds;
			string strWheres;
			string strOrders;
			string strModField;
			int nRownum=0;
			if ( config.sectionExists(strConfigSelect) )
			{
				if(config.valueExists(strConfigSelect,"fields"))
					strFileds=config.readString(strConfigSelect,"fields");
				if(config.valueExists(strConfigSelect,"wheres"))
					strWheres=config.readString(strConfigSelect,"wheres");
				if(config.valueExists(strConfigSelect,"orders"))
					strOrders=config.readString(strConfigSelect,"orders");
				if(config.valueExists(strConfigSelect,"rownum"))
					nRownum=config.readInt(strConfigSelect,"rownum");
				if(config.valueExists(strConfigSelect,"modfield"))
					strModField=config.readString(strConfigSelect,"modfield");

				vector<string> vecResultFields; //需要查询出的字段
				vector<string> vecWhereFields;  //查询条件
				vector<string> vecOrderFields;  //查询order by 字段

				//printf("strFileds=%s\n",strFileds.c_str());
				if ( strFileds.length()!=0 )
				{
					char* p = strtok(const_cast<char*>(strFileds.c_str()),";");

					while(p!= NULL)
					{
						string sField=p;
						vecResultFields.push_back(sField);
						p = strtok(NULL,";");
					}
				}
				if ( vecResultFields.size()==0 )
				{
					//printf("gConfig.vecClounmInfo.size()=%d\n",gConfig.vecClounmInfo.size());
					for ( int i=0;i<gConfig.vecClounmInfo.size();i++ )
					{
						string sField;
						sField=gConfig.vecClounmInfo[i].column_name;
						vecResultFields.push_back(sField);
						//printf("sfield=%s\n",sField.c_str());
					}
				}



				if ( strWheres.length()!=0 )
				{
					char* p = strtok(const_cast<char*>(strWheres.c_str()),";");

					while(p!= NULL)
					{
						string sField=p;
						vecWhereFields.push_back(sField);
						p = strtok(NULL,";");
					}
				}
				else //if not set,not where condition select
				{

				}

				if ( strOrders.length()!=0 )
				{
					char* p = strtok(const_cast<char*>(strOrders.c_str()),";");

					while(p!= NULL)
					{
						string sField=p;
						vecOrderFields.push_back(sField);
						p = strtok(NULL,";");
					}
				}
				else //if not set,not order by
				{

				}

				CSelectInfo selectInfo;
				selectInfo.maxSelectCounts=nRownum;
				selectInfo.vecResultFields=vecResultFields;
				selectInfo.vecWhereFields=vecWhereFields;
				selectInfo.vecOrderFields=vecOrderFields;
				selectInfo.modField=strModField;
				//printf("strmodfield=%s\n",strModField.c_str());
				gConfig.vecSelectInfo.push_back(selectInfo);
			}
		}

		//init delete info
		char* pConfigDelete="DELETE";
		char strConfigDelete[10];
		for ( int i=0;i<9;i++ )
		{
			sprintf(strConfigDelete,"%s%d",pConfigDelete,i);
			string strWheres;
			if ( config.sectionExists(strConfigDelete) )
			{
				vector<string> vecWhereFields;  //删除条件
				if(config.valueExists(strConfigDelete,"wheres"))
					strWheres=config.readString(strConfigDelete,"wheres");
				if ( strWheres.length()!=0 )
				{
					char* p = strtok(const_cast<char*>(strWheres.c_str()),";");

					while(p!= NULL)
					{
						string sField=p;
						vecWhereFields.push_back(sField);
						p = strtok(NULL,";");
						//printf("delete1 sfield=%s\n",sField.c_str());
					}
				}
				if ( vecWhereFields.size()==0 )
				{
					for ( int j=0;j<gConfig.vecClounmInfo.size();j++ )
					{
						string sField;
						sField=gConfig.vecClounmInfo[j].column_name;
						vecWhereFields.push_back(sField);
						//printf("delete sfield=%s\n",sField.c_str());
					}
				}
				CDeleteInfo deleteInfo;
				deleteInfo.vecWhereFields=vecWhereFields;

				gConfig.vecDeleteInfo.push_back(deleteInfo);
			}
		}
		

		//init update info
		char* pConfigUpdate="UPDATE";
		char strConfigUpdate[10];
		for ( int i=0;i<9;i++ )
		{
			sprintf(strConfigUpdate,"%s%d",pConfigUpdate,i);
			string strWheres;
			string strFileds;
			if ( config.sectionExists(strConfigUpdate) )
			{
				if(config.valueExists(strConfigUpdate,"fields"))
					strFileds=config.readString(strConfigUpdate,"fields");
				if(config.valueExists(strConfigUpdate,"wheres"))
					strWheres=config.readString(strConfigUpdate,"wheres");

				vector<string> vecUpdateFields; //需要更新的字段
				vector<string> vecWhereFields;  //更新条件
				if ( strFileds.length()!=0 )
				{
					char* p = strtok(const_cast<char*>(strFileds.c_str()),";");

					while(p!= NULL)
					{
						string sField=p;
						vecUpdateFields.push_back(sField);
						p = strtok(NULL,";");
					}
				}
				if ( vecUpdateFields.size()==0 )
				{
					//printf("gConfig.vecClounmInfo.size()=%d\n",gConfig.vecClounmInfo.size());
					for ( int i=0;i<gConfig.vecClounmInfo.size();i++ )
					{
						string sField;
						sField=gConfig.vecClounmInfo[i].column_name;
						vecUpdateFields.push_back(sField);

					}
				}

				if ( strWheres.length()!=0 )
				{
					char* p = strtok(const_cast<char*>(strWheres.c_str()),";");

					while(p!= NULL)
					{
						string sField=p;
						vecWhereFields.push_back(sField);
						p = strtok(NULL,";");
					}
				}
				if ( vecWhereFields.size()==0 )
				{
					for ( int i=0;i<gConfig.vecClounmInfo.size();i++ )
					{
						string sField;
						sField=gConfig.vecClounmInfo[i].column_name;
						vecWhereFields.push_back(sField);

					}
				}
				CUpdateInfo updateinfo;
				updateinfo.vecUpdateFields=vecUpdateFields;
				updateinfo.vecWhereFields=vecWhereFields;
				gConfig.vecUpdateInfo.push_back(updateinfo);
			}
			
		}

		//gen data struct file
		gConfig.str_class_name="C";
		gConfig.str_class_name+=gConfig.str_prefix_name;
		gConfig.str_handleimp_class_name=gConfig.str_class_name+"HandleImp";
		gConfig.str_handle_class_name=gConfig.str_class_name+"Handle";

		gConfig.str_struct_cpp_name=gConfig.str_class_name+"Data.cpp";
		gConfig.str_struct_h_name=gConfig.str_class_name+"Data.h";
		
		printf("data struct header file =%s ,cpp file=%s \n",gConfig.str_struct_h_name.c_str(),gConfig.str_struct_cpp_name.c_str());

		
		//hear to gen cpp file
		ofstream ofs(gConfig.str_struct_h_name.c_str());
		if (!ofs)
		{
			printf("open file % fail \n",gConfig.str_struct_h_name.c_str());
			return -1;
		}

		GenDataHeadFile(ofs,gConfig);

		//hear start to gen cpp file
		ofstream ofsp(gConfig.str_struct_cpp_name.c_str());
		if (!ofsp)
		{
			printf("open file % fail \n",gConfig.str_struct_cpp_name.c_str());
			return -1;
		}
		
		GenDataBodyFile(ofsp,gConfig);

		gConfig.str_handle_cpp_name=gConfig.str_class_name+"Handle.cpp";
		gConfig.str_handle_h_name=gConfig.str_class_name+"Handle.h";

		printf("handle header file =%s ,cpp file=%s \n",gConfig.str_handle_h_name.c_str(),gConfig.str_handle_cpp_name.c_str());
		
		
		ofstream ofhh(gConfig.str_handle_h_name.c_str());
		if (!ofhh)
		{
			printf("open file % fail \n",gConfig.str_handle_h_name.c_str());
			return -1;
		}
		GenHandleHeadFile(ofhh,gConfig);

		ofstream ofhp(gConfig.str_handle_cpp_name.c_str());
		if (!ofhp)
		{
			printf("open file % fail \n",gConfig.str_handle_cpp_name.c_str());
			return -1;
		}
		GenHandleBodyFile(ofhp,gConfig);
		

	}
	catch(OSSException &e) 
	{
		printf(("OSS error(%s)\n",e.what()));
		return 0;
	}

	catch(const otl_exception& p)
	{ 
		//db_new.rollback ();
		printf ("错误：%s (%s,%s) \n",p.msg , p.stm_text , p.var_info);
		throw;
	}
	catch (exception &e)
	{
		printf("error:%s!\n", e.what());
	}
	catch ( ... )
	{
		printf("Unexpected Error!errno=%d,strerr=%s\n",errno,strerror(errno));
		throw;
	}



	printf("over\n");
	return 1;
}
