#include "RtspSource.h"
#include <boost/thread.hpp>
#include <stdio.h>


#define D_PB_BUF_SIZE 65535

server m_server;

static int Check(void* opa)
{
    RtspSource* p = (RtspSource*)opa;
    if (p->Runing == false) return AVERROR_EOF;
    return av_gettime() -  p->m_tStart >= p->OverTime ? AVERROR_EOF : 0;
}

static std::string GetMIME(uint8_t* data, int len)
{
	int n = 0;
	if (data[0] == 0)
	{
		while (n + 3 < len)
		{
			if ((data[n] == 0) & (data[n + 1] == 0) & (data[n + 2] == 1))
			{
				n += 3;
				break;
			}
			else
				n++;
		}
	}
	n += 1;
	if (n + 3 > len)
		return "";
	char mime[10] = {0};
	sprintf(mime,"%.2x%.2x%.2x",data[n], data[n + 1], data[n + 2]);
	return std::string(mime);
}

static int write_buffer(void *opaque, uint8_t *buf, int buf_size)
{
	RtspSource* source = (RtspSource*)opaque;
	if (source->sendBinaryData(buf, buf_size) < 0)
		return AVERROR_EOF;
	return buf_size;
}

int  RtspSource::sendBinaryData(unsigned char *pData, int nSize)
{
	int nRet = 0;
	try
	{
		m_server.send(Client_Hdl, pData, nSize, websocketpp::frame::opcode::binary);
	}
	catch (websocketpp::exception const & e)
	{
		std::cout << e.what() << std::endl;
		return -1;
	}

	return nRet;
}

void RtspSource::sendWSString(std::string wsstr)
{
	try
	{
		m_server.send(Client_Hdl, wsstr, websocketpp::frame::opcode::text);
	}
	catch (websocketpp::exception const & e)
	{
		std::cout << e.what() << std::endl;
	}
}

RtspSource::RtspSource(std::string url, websocketpp::connection_hdl hdl)
    : RtspUrl(url)
    , Client_Hdl(hdl)
    , In_FormatContext(NULL)
    , Out_FormatContext(NULL)
    , pb_Buf(NULL)
    , Runing(false)
    , in_video_index(-1)
    , in_audio_index(-1)
    , out_video_index(-1)
    , out_audio_index(-1)
{
}

RtspSource::~RtspSource()
{
    Close();
}

bool RtspSource::Open()
{
    if (avformat_alloc_output_context2(&Out_FormatContext, NULL, "mp4", NULL) < 0)
        return false;
    pb_Buf = (uint8_t*)av_malloc(sizeof(uint8_t)*(D_PB_BUF_SIZE));
    Out_FormatContext->pb = avio_alloc_context(pb_Buf, D_PB_BUF_SIZE,1,(void*)this,NULL,write_buffer,NULL);
    if (Out_FormatContext->pb == NULL)
    {
        avformat_free_context(Out_FormatContext);
        Out_FormatContext = NULL;
		sendWSString("fail");
        return false;
    }
	Out_FormatContext->pb->write_flag = 1;
	Out_FormatContext->pb->seekable = 1;
	Out_FormatContext->flags=AVFMT_FLAG_CUSTOM_IO;
	Out_FormatContext->flags |= AVFMT_FLAG_FLUSH_PACKETS;
	Out_FormatContext->flags |= AVFMT_NOFILE;
	Out_FormatContext->flags |= AVFMT_FLAG_AUTO_BSF;
	Out_FormatContext->flags |= AVFMT_FLAG_NOBUFFER;
    AVDictionary* dco = NULL;
    av_dict_set(&dco, "rtsp_transport", "tcp", 0);
    In_FormatContext = avformat_alloc_context();
    if (!In_FormatContext)
    {
        avformat_free_context(Out_FormatContext);
        Out_FormatContext = NULL;
		sendWSString("fail");
        return false;
    }
    In_FormatContext->interrupt_callback.callback = Check;
    In_FormatContext->interrupt_callback.opaque = this;
	Runing = true;
    m_tStart = av_gettime();
    OverTime = 5000000;
    int ret = avformat_open_input(&In_FormatContext, RtspUrl.c_str(), NULL, &dco);
    av_dict_free(&dco);
    if (ret < 0)
    {
        avformat_free_context(Out_FormatContext);
        Out_FormatContext = NULL;
		Runing = false;
		sendWSString("fail");
        return false;
    }
    m_tStart = av_gettime();
    OverTime = 5000000;
	In_FormatContext->flags |= AVFMT_FLAG_NOBUFFER;
	av_format_inject_global_side_data(In_FormatContext);
    if (avformat_find_stream_info(In_FormatContext, NULL) < 0)
    {
        avformat_close_input(&In_FormatContext);
        avformat_free_context(Out_FormatContext);
        Out_FormatContext = NULL;
		Runing = false;
		sendWSString("fail");
        return false;
    }
    for (int i = 0; i < In_FormatContext->nb_streams; i++)
    {
		if ((In_FormatContext->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO)
            || (In_FormatContext->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_AUDIO))
        {
            AVStream* in_stream = In_FormatContext->streams[i];
            AVStream* out_stream = avformat_new_stream(Out_FormatContext, NULL);
            if (!out_stream)
            {
                avformat_close_input(&In_FormatContext);
                avformat_free_context(Out_FormatContext);
                Out_FormatContext = NULL;
				Runing = false;
				sendWSString("fail");
                return false;
            }
            if (in_stream->codecpar->codec_type == AVMEDIA_TYPE_AUDIO)
            {
                if ((in_stream->codecpar->codec_id == AV_CODEC_ID_AAC) || (in_stream->codecpar->codec_id == AV_CODEC_ID_AAC_LATM))
                {
                    in_audio_index = in_stream->index;
                    out_audio_index = out_stream->index;
                }
            }
            if (in_stream->codecpar->codec_type == AVMEDIA_TYPE_VIDEO)
            {
                if (in_stream->codecpar->codec_id == AV_CODEC_ID_H264)
                {
                    in_video_index = in_stream->index;
                    out_video_index = out_stream->index;
                }
            }
			AVCodec* in_codec = avcodec_find_encoder(in_stream->codecpar->codec_id);
			AVCodecContext *codec_ctx = avcodec_alloc_context3(in_codec);
			ret = avcodec_parameters_to_context(codec_ctx, in_stream->codecpar);
			if (ret < 0)
			{
				avformat_close_input(&In_FormatContext);
                avformat_free_context(Out_FormatContext);
                Out_FormatContext = NULL;
				Runing = false;
				sendWSString("fail");
                return false;
			}

			codec_ctx->codec_tag = 0;
			if (Out_FormatContext->oformat->flags & AVFMT_GLOBALHEADER)
				codec_ctx->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;

			ret = avcodec_parameters_from_context(out_stream->codecpar, codec_ctx);
			if (ret < 0)
			{
				avformat_close_input(&In_FormatContext);
                avformat_free_context(Out_FormatContext);
                Out_FormatContext = NULL;
				Runing = false;
				sendWSString("fail");
                return false;
			}
			out_stream->time_base = in_stream->time_base;
			avcodec_free_context(&codec_ctx);
			if(out_stream->codecpar->bit_rate == 0)
				out_stream->codecpar->bit_rate = 400000;
        }
    }
    if (out_video_index == -1)
    {
        avformat_close_input(&In_FormatContext);
        avformat_free_context(Out_FormatContext);
        Out_FormatContext = NULL;
        Runing = false;
        sendWSString("fail");
        return false;
    }

    AVDictionary *opts = NULL;
	av_dict_set(&opts, "movflags", "frag_keyframe+empty_moov+omit_tfhd_offset+faststart+separate_moof+disable_chpl+default_base_moof+dash", 0);

    std::string openstr = "open:avc1." + GetMIME(Out_FormatContext->streams[out_video_index]->codecpar->extradata, Out_FormatContext->streams[out_video_index]->codecpar->extradata_size);
    if(out_audio_index != -1)
    {
        switch (Out_FormatContext->streams[out_audio_index]->codecpar->profile)
        {
            case FF_PROFILE_AAC_MAIN :
                openstr += ",mp4a.40.1";
                break;
            case FF_PROFILE_AAC_LOW:
                openstr += ",mp4a.40.2";
                break;
            case FF_PROFILE_AAC_SSR:
                openstr += ",mp4a.40.3";
                break;
            case FF_PROFILE_AAC_LTP:
                openstr += ",mp4a.40.4";
                break;
            case FF_PROFILE_AAC_HE:
                openstr += ",mp4a.40.5";
                break;
            case FF_PROFILE_AAC_HE_V2:
                openstr += ",mp4a.40.1D";
                break;
            case FF_PROFILE_AAC_LD:
                openstr += ",mp4a.40.17";
                break;
            case FF_PROFILE_AAC_ELD:
                openstr += ",mp4a.40.27";
                break;
            default:
                out_audio_index = -1;
                break;
        }
    }
    sendWSString(openstr);

    ret = avformat_write_header(Out_FormatContext, &opts);
	Out_FormatContext->oformat->flags |= AVFMT_TS_NONSTRICT;
	av_dict_free(&opts);
	if (ret < 0)
	{
        avformat_close_input(&In_FormatContext);
        avformat_free_context(Out_FormatContext);
        Out_FormatContext = NULL;
		Runing = false;
		sendWSString("close");
        return false;
	}
    boost::thread ReadThread(&RtspSource::ReadAndWrite,this);
    return true;
}

void RtspSource::Close()
{
    Runing = false;
    while (Out_FormatContext)
		av_usleep(1000);
}

void RtspSource::ReadAndWrite()
{
    int read_error_num = 0;
	int write_error_num = 0;
	bool FirstKeyFrame = false;
    AVPacket pkt;
    while (Runing)
    {
        int out_index = -1;
        av_init_packet(&pkt);
        pkt.data = NULL;
        pkt.size = 0;
        m_tStart = av_gettime();
        OverTime = 3000000;
		int ret = av_read_frame(In_FormatContext, &pkt);
        if (ret < 0)
		{
            read_error_num++;
			if (read_error_num > 3)
			{
				av_packet_unref(&pkt);
				break;
			}
			else
			{
				av_packet_unref(&pkt);
				continue;
			}
		}
        else
            read_error_num = 0;
        if (pkt.stream_index == in_video_index)
            out_index = out_video_index;
        if (pkt.stream_index == in_audio_index)
            out_index = out_audio_index;
        if (out_index == -1)
        {
            av_packet_unref(&pkt);
            continue;
        }
        if (!FirstKeyFrame && (pkt.stream_index == in_video_index))
        {
            if (pkt.flags & AV_PKT_FLAG_KEY)
                FirstKeyFrame = true;
            else
            {
                av_packet_unref(&pkt);
                continue;
            }
        }
        AVStream* in_stream = In_FormatContext->streams[pkt.stream_index];
		pkt.stream_index = out_index;
        AVStream* out_stream = Out_FormatContext->streams[out_index];
		pkt.flags |= AV_PKT_FLAG_KEY;
        pkt.pos = -1;
		ret = av_interleaved_write_frame(Out_FormatContext, &pkt);
        av_packet_unref(&pkt);
    }
	std::cout << "read end " << std::endl;
	if (In_FormatContext)
        avformat_close_input(&In_FormatContext);
    if (Out_FormatContext)
    {
        av_write_trailer(Out_FormatContext);
        avformat_free_context(Out_FormatContext);
        Out_FormatContext = NULL;
    }
}
