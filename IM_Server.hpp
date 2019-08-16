#pragma once
#include "Util.hpp"
#include <sstream>

#define IM_DB "im_db"
#define MY_PORT 3306
#define NUM 1024
#define SESSION_ID "im_sid"
#define SESSION_NAME "im_name"                                                                             
#define SESSION_CHECK_INTERVAL 5.0
#define SESSION_TTL 1800.0


struct mg_serve_http_opts s_http_server_opts;

typedef struct session
{
  uint64_t id;
  string name;
  double created;
  double last_used;
}session_t;

class Session
{
private:
  session_t sessions[NUM];
public:
  Session()
  {
    for(auto i = 0; i < NUM; ++i)
    {
      sessions[i].id = 0;
      sessions[i].name="";
      sessions[i].created=0.0;
      sessions[i].last_used=0.0;

    }
  }
  bool GetSession(http_message *hm)
  {
    uint64_t sid;
    char ssid[64];
    char* s_ssid=ssid;
    struct mg_str *cookie_header = mg_get_http_header(hm,"cookie");
    if(cookie_header == nullptr)
    {
      return false;
    }
    if(!mg_http_parse_header2(cookie_header,SESSION_ID,&s_ssid,sizeof(ssid)))
    {
      return false;

    }
    sid = strtoull(ssid,NULL,10);
    for(auto i = 0; i < NUM; ++i)
    {
      if(sessions[i].id == sid)
      {
        sessions[i].last_used = mg_time();
        return true;
      }
    }
    return false;
  }
  bool IsLogin(http_message *hm)
  {
    return GetSession(hm);
  }
  bool CreateSession(string name, uint64_t &id)
  {
    int i=0;
    for(; i < NUM; ++i)
    {
      if(sessions[i].id==0)
      {
        break;
      }
    }
    if(i==NUM)
    {
      return false;
    }
    sessions[i].id=(uint64_t)(mg_time()*1000000L);
    sessions[i].name=name;
    sessions[i].last_used=sessions[i].created=mg_time();
    id=sessions[i].id;
    return true;
  }
  void DestorySession(session_t* s)
  {
    s->id=0;
  }
  void CheckSession()
  {
    double ovtime = mg_time() - SESSION_TTL;
    for(auto i = 0; i < NUM; ++i)
    {
      if(sessions[i].id!=0 && sessions[i].last_used < ovtime)
      {
        DestorySession(sessions+i);
      }
    }
  }
  ~Session()
  {

  }

};

class MysqlClient
{
private:
  MYSQL *my;
  bool ConnectMysql()
  {
    my=mysql_init(NULL);
    mysql_set_character_set(my, "utf8");
    if(!mysql_real_connect(my,"localhost","root","123",IM_DB,MY_PORT,NULL,0))
    {
      cerr<<"connent mysql error"<<endl;
      return false;
    }
    cout<<"connect mysql succes"<<endl;
    mysql_query(my, "set names 'utf8'");
    return true;
  }
public:
  MysqlClient()
  {
  }
  bool InsertUser(string name,string passwd)
  {
    ConnectMysql();
    string sql="insert into user(name,passwd) values(\""; 
    sql += name;
    sql += "\",\"";
    sql += passwd;
    sql += "\")";
    if(0 == mysql_query(my,sql.c_str()))
    {
     
      //cout<<sql<<endl;
      return true;
    }
    mysql_close(my);
    return false;
  }
  bool SelectUser(string name,string passwd)
  {
    ConnectMysql();
    string sql = "SELECT * FROM user WHERE name=\"";
    sql += name;
    sql += "\" AND passwd=\"";
    sql += passwd;
    sql += "\"";
    cout << sql << endl;
    if(0 != mysql_query(my,sql.c_str()))
    {
      return false;

    }
    cout << "select done" << endl;
    MYSQL_RES *result = mysql_store_result(my);
    int num = mysql_num_rows(result);
    free(result);
    mysql_close(my);
    return num > 0?true:false;
  }
  ~MysqlClient()
  {
  }
};
class IM_Server
{
private:
  string port;
  struct mg_mgr mgr;
  struct mg_connection *nc;
  volatile bool quit;
  static MysqlClient mc;
  static Session sn;
public:
  IM_Server(string  _port="8080"):port(_port),quit(false)
  {

  }
  static void Broadcast(struct mg_connection *nc, string ms)
  {
    struct mg_connection *c;
    for (c = mg_next(nc->mgr, NULL); c != NULL; c = mg_next(nc->mgr, c)) 
    {
         mg_send_websocket_frame(c, WEBSOCKET_OP_TEXT, ms.c_str(), ms.size());
    }

  }

  static bool IsWebsocket(const struct mg_connection *nc)
  {
    return (nc->flags & MG_F_IS_WEBSOCKET) ? true : false;
  }

  static void LogHandler(struct mg_connection *nc, int ev, void *data)
  {
    if(ev != MG_EV_HTTP_REQUEST)
    { 
      //mongoose 样例的bug，链接关闭的事件也会触发...
      return;
    }
    string code = "0";
    string shead = "";
    string echo_json = "{\"result\":";
    struct http_message *hm = (struct http_message*)data;
    
    mg_printf(nc, "HTTP/1.1 200 OK\r\n");
    
    string method=Util::MgstrToString(& hm->method);
    if(method == "POST"||method == "post")
    {
      string body=Util::MgstrToString(& hm->body);
      string name,passwd;
      if(Util::GetNameAndPasswd(body,name,passwd) && !name.empty() && !passwd.empty())
      {
        if(mc.SelectUser(name, passwd))
        {
          uint64_t id = 0;
          if(sn.CreateSession(name,id))
          {
            stringstream ss;
            ss << "Set-Cookie: "<<SESSION_ID<<"="<<id<<";path=/\r\n";
            ss << "Set-Cookie: "<<SESSION_NAME<<"="<<name<<";path=/\r\n";
            shead = ss.str();
            mg_printf(nc, shead.data());
            code = "0";
          }
          else 
          {
            code = "3";
          }
        }
        else 
        {
          code = "1";
        }
      }
      else 
      {
        code = "2";
      }
      echo_json += code;
      echo_json += "}";
      mg_printf(nc, "Content-Length: %lu\r\n\r\n", echo_json.size());
      mg_printf(nc,echo_json.data());
    }
    else 
    {
      mg_serve_http(nc, hm, s_http_server_opts);
    }
    nc->flags |= MG_F_SEND_AND_CLOSE; //相应完毕，完毕链接
  }
  static void RegisterHandler(struct mg_connection *nc, int ev, void *data)
  {
    if(ev == MG_EV_CLOSE) {
      return;
    }
    struct http_message *hm = (struct http_message*)data;
    string method=Util::MgstrToString(& hm->method);
    if(method=="POST"||method=="post")
    {
      string echo_json="{\"result\": ";
      string code = "0";
      string body=Util::MgstrToString(& hm->body);
      string name,passwd;
      if(Util::GetNameAndPasswd(body,name,passwd) && !name.empty() && !passwd.empty())
      {
        if(mc.InsertUser(name,passwd))
        {
          code = "0";
        }
        else 
        {
          code = "1";
        }
        
      }
      else 
      {
        code="2";
      }
      echo_json += code;
      echo_json += "}";
      mg_printf(nc, "HTTP/1.1 200 OK\r\n");
      mg_printf(nc, "Content-Length: %lu\r\n\r\n", echo_json.size());
      mg_printf(nc, echo_json.data());
    }
    else 
    {
      mg_serve_http(nc, hm, s_http_server_opts);
    }
    nc->flags |= MG_F_SEND_AND_CLOSE; //相应完毕，完毕链接
    
  }
  static void EvHandler(struct mg_connection *nc, int ev, void *data)
  {
    switch (ev) 
    {
      case MG_EV_WEBSOCKET_HANDSHAKE_DONE: 
        {
          string tip="-欢迎新人加入...";
          Broadcast(nc,tip);
          /* websocket握手事件完成 */
          break;
        }
      case MG_EV_WEBSOCKET_FRAME: 
        {
          struct websocket_message *wm = (struct websocket_message *)data;
          struct mg_str d = {(char *) wm->data, wm->size};
          string ms = Util::MgstrToString(&d);
          Broadcast(nc,ms);
          /* 正常的websocket数据 */
          break;
        }
      case MG_EV_HTTP_REQUEST: 
        {
          /* 正常的http请求 */
          struct http_message *hm = (struct http_message*)data;
          string uri = Util::MgstrToString(&hm->uri);
          if(uri.empty()||uri=="/"||uri=="/index.html")
          {
            if(sn.IsLogin(hm))
            {
              mg_serve_http(nc,hm,s_http_server_opts);
            }
            else
            {
              mg_http_send_redirect(nc,302,mg_mk_str("/login.html"),mg_mk_str(NULL));
            }
          }
          else
            mg_serve_http(nc,hm,s_http_server_opts);
          nc->flags |= MG_F_SEND_AND_CLOSE;
          break;
        }
      case MG_EV_CLOSE: 
        {
          /* 连接关闭 */
          if(IsWebsocket(nc))
          {
            string message = "-有用户退出...";
            Broadcast(nc, message);
          }
          break;              
        }     
    }
  }
  void Init()
  {
    signal(SIGPIPE, SIG_IGN);
    mg_mgr_init(&mgr, NULL);
    nc=mg_bind(&mgr,port.c_str(), EvHandler);
    
    mg_register_http_endpoint(nc,"/LH",LogHandler);
    mg_register_http_endpoint(nc,"/RH",RegisterHandler);
    
    mg_set_protocol_http_websocket(nc);
    s_http_server_opts.document_root = "web"; //设置服务器当前工作目录

    mg_set_timer(nc, mg_time()+SESSION_CHECK_INTERVAL);

  }
  void Start()
  {
    int timeout=10000;
    while(!quit)
    {
      mg_mgr_poll(&mgr,timeout);
      //cout<<"timeout....."<<endl;
    }
  }
  ~IM_Server()
  {
    mg_mgr_free(&mgr);
  }

};
MysqlClient IM_Server::mc;
Session   IM_Server::sn;
