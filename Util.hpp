#pragma once
#include <iostream>
#include <string>
#include <stdio.h>
#include <json/json.h>
#include <memory>
#include "mysql.h"
#include "mongoose/mongoose.h"

using namespace std;
class Util
{
public:
  Util()
  {

  }
  static string MgstrToString(mg_str* ms)
  {
    string retstr ="";
    for(size_t i=0;i < ms->len;i++)
    {
      retstr.push_back(ms->p[i]);
    }
    return retstr;
  }
  static bool GetNameAndPasswd(string info,string &name,string &passwd)
  {
    bool result;
    JSONCPP_STRING errs;
    Json::Value root;
    Json::CharReaderBuilder cb;
    std::unique_ptr<Json::CharReader> const cr(cb.newCharReader());
    result = cr->parse(info.data(),info.data()+info.size(),&root,&errs);
    if(!result||!errs.empty())
    {
      cerr<<"parse error"<<endl;
      return false;
    }
    name = root["name"].asString();
    passwd = root["passwd"].asString();
    return true;
  }
  ~Util()
  {

  }

};
