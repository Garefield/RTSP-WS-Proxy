#include "WSStringProc.h"
#include <boost/asio.hpp>
#include <boost/bind.hpp>
#include <boost/thread.hpp>

WSStringProc::WSStringProc(websocketpp::connection_hdl hdl)
    : rtspsource(NULL)
    , Con_Hdl(hdl)
{
    //ctor
}

WSStringProc::~WSStringProc()
{
    if (rtspsource)
    {
        RtspSource* delsource = rtspsource;
        rtspsource = NULL;
        delete delsource;
    }
}

void WSStringProc::ProcWSData(std::string data)
{
    WSDataBuf += data;
    while(true)
    {
        std::string::size_type idx;
        idx = WSDataBuf.find("{");
        if (idx != std::string::npos)
        {
            if(idx > 0)
                WSDataBuf.erase(WSDataBuf.begin(),WSDataBuf.begin() + idx);
        }
        else
        {
            return;
        }
        Json::CharReaderBuilder  b;
        Json::CharReader* reader(b.newCharReader());
        Json::Value *root = new Json::Value();
        std::string errs;
        if (reader->parse(WSDataBuf.c_str(), WSDataBuf.c_str() + WSDataBuf.size(), root, &errs))
        {
            int jsonlen = root->getOffsetLimit();
            boost::asio::io_service io;
            io.dispatch(boost::bind(&WSStringProc::OnWSData,this,root));
            io.run();
            WSDataBuf.erase(WSDataBuf.begin(),WSDataBuf.begin() + jsonlen);
        }
        else
        {
            std::cout << errs << std::endl;
            delete reader;
            return;
        }
        delete reader;
    }
}

void WSStringProc::OnWSData(Json::Value *root)
{
    if ((*root)["url"].empty()) return;
    if (rtspsource) return;
    std::string url = (*root)["url"].asString();
    rtspsource = new RtspSource(url, Con_Hdl);
    boost::thread opentread(&WSStringProc::OpenSource, this);
}

void WSStringProc::OpenSource()
{
    if (rtspsource->Open() == false)
    {
        RtspSource* delsource = rtspsource;
        rtspsource = NULL;
        delete delsource;
    }
}
