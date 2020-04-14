// websocketproxy.cpp : 定义控制台应用程序的入口点。
//
#include "stdafx.h"

#include "RtspSource.h"
#include "WSStringProc.h"
#include <boost/unordered_map.hpp>
#include <boost/asio.hpp>
#include <boost/bind.hpp>
#include <atlconv.h>


boost::unordered_map<server::connection_ptr, WSStringProc*> Handle_map;

void OnWSMsg(websocketpp::connection_hdl hdl, std::string str)
{
    WSStringProc*  wsproc;
    server::connection_ptr con  = m_server.get_con_from_hdl(hdl);
    boost::unordered_map<server::connection_ptr, WSStringProc*>::iterator iter = Handle_map.find(con);
    if (iter != Handle_map.end())
        wsproc = iter->second;
    else
	{
        wsproc = new WSStringProc(hdl);
		server::connection_ptr con  = m_server.get_con_from_hdl(hdl);
		Handle_map.insert(boost::unordered_map<server::connection_ptr, WSStringProc*>::value_type(con,wsproc));
	}
    wsproc->ProcWSData(str);
}

void on_message(websocketpp::connection_hdl hdl, server::message_ptr msg)
{
	std::cout << "on_message called with hdl: " << hdl.lock().get()
		<< " and message: " << msg->get_payload()
		<< std::endl;
    if (msg->get_opcode() == websocketpp::frame::opcode::text)
    {
        std::string wsmsg = msg->get_payload();
        boost::asio::io_service io;
        io.dispatch(boost::bind(&OnWSMsg, hdl, wsmsg));
        io.run();
    }
}

void on_close(websocketpp::connection_hdl hdl)
{
	WSStringProc*  wsproc = NULL;
    server::connection_ptr con  = m_server.get_con_from_hdl(hdl);
	boost::unordered_map<server::connection_ptr, WSStringProc*>::iterator iter = Handle_map.find(con);
    if (iter != Handle_map.end())
        wsproc = iter->second;
    Handle_map.erase(con);
	if (wsproc)
		delete wsproc;
}

int _tmain(int argc, _TCHAR* argv[])
{
    if (argc < 2) return -1;
	USES_CONVERSION;
	char *szBuffer = W2A(argv[1]);//这两句也要加入3
	int wsport = std::stoi(std::string(szBuffer));
    m_server.set_message_handler(&on_message);
    m_server.set_close_handler(&on_close);
    m_server.set_access_channels(websocketpp::log::alevel::none);
    //m_server.set_error_channels(websocketpp::log::elevel::all);
	m_server.set_error_channels(websocketpp::log::alevel::all);
	m_server.set_max_message_size(300000);
    m_server.set_max_http_body_size(300000);

    m_server.init_asio();
    m_server.listen(wsport);
    m_server.start_accept();

    m_server.run();
}

