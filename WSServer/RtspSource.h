#ifndef RTSPSOURCE_H
#define RTSPSOURCE_H

#include <iostream>
#include <websocketpp/config/asio_no_tls.hpp>
#include <websocketpp/server.hpp>

typedef websocketpp::server<websocketpp::config::asio> server;

extern server m_server;

extern "C"
{
#include <libavcodec/avcodec.h>
#include <libavutil/common.h>
#include <libavformat/avformat.h>
#include <libavutil/avutil.h>
#include <libavutil/imgutils.h>
#include <libavutil/opt.h>
#include <libavutil/time.h>
#include <libavutil/pixfmt.h>
#include <libavutil/pixdesc.h>
#include <libswscale/swscale.h>
#include <libavutil/fifo.h>
}

class RtspSource
{
    public:
        RtspSource(std::string url, websocketpp::connection_hdl hdl);
        virtual ~RtspSource();
        bool Open();
        void Close();
    public:
        bool Runing;
        uint64_t m_tStart,OverTime;

        int  sendBinaryData(unsigned char *pData, int nSize);
    protected:
        int in_video_index, in_audio_index,  out_video_index, out_audio_index;;

        std::string RtspUrl;
        websocketpp::connection_hdl Client_Hdl;
        uint8_t* pb_Buf;

        AVFormatContext* In_FormatContext;
        AVFormatContext* Out_FormatContext;


    private:
		void sendWSString(std::string wsstr);
        void ReadAndWrite();
};

#endif // RTSPSOURCE_H
