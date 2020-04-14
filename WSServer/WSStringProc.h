#ifndef WSSTRINGPROC_H
#define WSSTRINGPROC_H

#include <iostream>
#include <json/reader.h>
#include "RtspSource.h"

class WSStringProc
{
    public:
        WSStringProc(websocketpp::connection_hdl hdl);
        virtual ~WSStringProc();
        void ProcWSData(std::string data);
    protected:
        std::string WSDataBuf;
        void OnWSData(Json::Value *root);
    private:
        RtspSource* rtspsource;
        websocketpp::connection_hdl Con_Hdl;
        void OpenSource();
};

#endif // WSSTRINGPROC_H
