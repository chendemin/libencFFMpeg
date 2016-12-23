/*
 * libencFFMpegDecoder.cpp
 * copyright (c) 2015 Chendemin <chendemin@motic.com>
 *
 * This file is part of libencFFmpeg.
 *
 * libencFFmpeg is free library; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * libencFFmpeg is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with libencFFmpeg; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */
#include "FFMpegDecoder.h"
extern MoticWeb::Utility::IUtility* g_util;
namespace MoticWeb
{
  namespace Codec
  {
    CFFMpegDecoder::CFFMpegDecoder(void)
      :m_bInitialized(false)     
      ,m_avCodec(0)
      ,m_avContext(0)
      ,m_avFmtContext(0)
      ,m_avFrame(0)
      ,m_swsContext(0)
      ,m_picrgb(0)
      ,m_width(0)
      ,m_height(0)
      ,m_iVideoStreamIndex(0)
      ,m_hTread(0)
      ,m_recv(0)
      ,m_hMutex(0)
      ,m_iStatus(0)
      ,m_uStartTimestamp(0)
    {
     
    }
    CFFMpegDecoder::~CFFMpegDecoder(void)
    {
      End();
    }
    bool CFFMpegDecoder::Start(int type)
    {
      if(m_bInitialized)return true;
      /* register all the codecs */
      avcodec_register_all();
      av_init_packet(&m_avPkt);
      /* find the mpeg1 video decoder */
      AVCodecID cid = AV_CODEC_ID_H264;
      if(type == 0)
      {
        cid = AV_CODEC_ID_H264;
      }
      else if(type == 1)
      {

      }
      m_avCodec = avcodec_find_decoder(cid);
      if (!m_avCodec) 
      {      
        return false;
      }
      m_avContext = avcodec_alloc_context3(m_avCodec);
      if (!m_avContext) 
      {       
        return false;
      }

      if(m_avCodec->capabilities&CODEC_CAP_TRUNCATED)
        m_avContext->flags|= CODEC_FLAG_TRUNCATED; /* we do not send complete frames */

      /* For some codecs, such as msmpeg4 and mpeg4, width and height
      MUST be initialized there because this information is not
      available in the bitstream. */
      /* open it */
      if (avcodec_open2(m_avContext, m_avCodec, NULL) < 0) 
      {       
        return false;
      }
      m_avFrame = av_frame_alloc();
      if (!m_avFrame)
      {   
        avcodec_close(m_avContext);
        av_free(m_avContext);          
        m_avContext = 0;
        return false;
      }
      m_bInitialized = true;
      return true;
    }

    bool CFFMpegDecoder::Start( const char* fileName, IImgBuffering* recv)
    {
      End();
      m_recv = recv;
      
      m_avContext = 0;
      av_register_all();

      if(m_hMutex == 0)m_hMutex = g_util->FindSystemHelper()->CreateMutex();
    
      //open rtsp
      if(avformat_open_input(&m_avFmtContext,fileName,NULL,NULL) != 0)
      {
        return false;
      }
      //find steream
      if(avformat_find_stream_info(m_avFmtContext,NULL) < 0)
      {
        avformat_close_input(&m_avFmtContext);
        return false;
      }
      av_dump_format(m_avFmtContext, 0, fileName, 0);
      //search video stream
      for(int i =0;i<(int)m_avFmtContext->nb_streams;i++)
      {
        if(m_avFmtContext->streams[i]->codec->codec_type == AVMEDIA_TYPE_VIDEO)
        {      
          m_avContext = m_avFmtContext->streams[i]->codec;
          m_iVideoStreamIndex = i;
          break;
        }
      }
      if(m_avContext == 0)
      {
        avformat_close_input(&m_avFmtContext);
        return false;
      }     
      if(m_avContext->width < 1 || m_avContext->height < 1)
      {
        avformat_close_input(&m_avFmtContext);
        return false;
      } 
      AllocateConvert();
      m_bInitialized = true;
      m_hTread = g_util->FindSystemHelper()->CreateThread(this);
      return true;
    }

    void CFFMpegDecoder::End()
    {
      if(m_iStatus == 2)
      {
        Resum();
      }
      if(m_bInitialized)
      {
        m_bInitialized = false;
        if(m_hTread)
        {
          g_util->FindSystemHelper()->DestroyThread(&m_hTread);
        }
        FreeConvert();      
        if(m_avCodec)
        {
          avcodec_close(m_avContext);         
          if(m_avFmtContext)
          {
            avformat_close_input(&m_avFmtContext);
          }
          else
          {
           av_free(m_avContext);    
          }
          m_avContext = 0;
          av_frame_free(&m_avFrame);
          m_avFrame = 0;
        }
       
      }
    }

    void CFFMpegDecoder::AddBuffer( unsigned char*buf, int size, IImgBuffering*img )
    {
      m_avPkt.size = size;
      m_avPkt.data = buf;

      int len, got_frame;  
      while (m_avPkt.size > 0)
      {
        len = avcodec_decode_video2(m_avContext, m_avFrame, &got_frame, &m_avPkt);
        if (len < 0) 
        {         
          break;
        }
        if (got_frame) 
        {    
          if(img)
          {
            AllocateConvert();
            unsigned char* rgb = img->GetWrite(m_width, m_height, 3);
            if(rgb)
            {
              avpicture_fill((AVPicture *) m_picrgb, rgb, AV_PIX_FMT_RGB24, m_width, m_height);
              sws_scale(m_swsContext, m_avFrame->data, m_avFrame->linesize, 0, m_height, m_picrgb->data, m_picrgb->linesize);
              img->SetWrite(rgb, m_width, m_height, 24, 0);
            }
          }         
        }
        if (m_avPkt.data) 
        {
          m_avPkt.size -= len;
          m_avPkt.data += len;
        }
      }
    }
    
    //unsigned char* CFFMpegDecoder::ConverToRGB24( AVFrame * frame )
    //{
    //  AllocateConvert(); 
    //  if(m_picrgb)
    //  {
    //    //m_rgbBuf = (uint8_t*)(av_malloc(size2));
    //    
    //    avpicture_fill((AVPicture *) m_picrgb, m_rgbBuf, PIX_FMT_BGR24, m_avContext->width, m_avContext->height);

    //    sws_scale(m_swsContext, frame->data, frame->linesize, 0, m_avContext->height, m_picrgb->data, m_picrgb->linesize);

    //    return m_picrgb->data[0]; 
    //  }
    //  return 0;
    //}

    void CFFMpegDecoder::AllocateConvert()
    {
      if(m_avContext->width != m_width || m_avContext->height != m_height || m_swsContext == 0)
      {
        m_width = m_avContext->width;
        m_height = m_avContext->height;
        FreeConvert();
        m_swsContext = sws_getContext(m_width, m_height, m_avContext->pix_fmt, m_width, m_height,
          AV_PIX_FMT_RGB24, SWS_BICUBIC, NULL, NULL, NULL);
        m_picrgb = av_frame_alloc();
        //m_rgbSize = avpicture_get_size(AV_PIX_FMT_RGB24, m_width, m_height);
       }      
    }

    void CFFMpegDecoder::FreeConvert()
    {
      if(m_picrgb)
      {
        av_frame_free(&m_picrgb);
      }    
      if(m_swsContext)
      {
        sws_freeContext(m_swsContext);
        m_swsContext = 0;
      }      
    }

    void CFFMpegDecoder::ThreadRun()
    {
      AVDictionary    *optionsDict = NULL;    
      av_init_packet(&m_avPkt);

      //AVStream* stream=NULL;

      m_avCodec = avcodec_find_decoder(m_avContext->codec_id);
      if(m_avCodec == 0)return;
      if (avcodec_open2(m_avContext, m_avCodec,&optionsDict) < 0)
      {
        return;
      }
      m_avFrame = av_frame_alloc();
      if (!m_avFrame)
      {   
        avcodec_close(m_avContext);
        av_free(m_avContext);          
        m_avContext = 0;
        return;
      }    
      m_uStartTimestamp = 0;
      int64_t basetime = 0;  
      int ct = m_avFmtContext->streams[m_iVideoStreamIndex]->time_base.den/m_avFmtContext->streams[m_iVideoStreamIndex]->time_base.num;
      m_iStatus = 1;
      int iFrame = 0;
      int iDropFrame = 0;
      while(m_bInitialized)
      {  
        {
          MoticWeb::Utility::AutoSimpleLock lock(m_hMutex);       
          int ret = av_read_frame(m_avFmtContext,&m_avPkt);        
          if(ret < 0)
          {    
            while(m_bInitialized && iDropFrame-- > 0)
            {
              DecodeOneFrame(ct);
            }
            iFrame = 0;
            m_uStartTimestamp = 0;          
            av_seek_frame(m_avFmtContext, m_iVideoStreamIndex, 0, AVSEEK_FLAG_FRAME);
           
            iDropFrame = 0;
            continue;        
          }       
          int64_t showtime = m_avPkt.dts*1000/ct;
          if(m_uStartTimestamp == 0)
          {
            m_uStartTimestamp = g_util->TimeStamp();
            basetime = showtime;
          }
          else
          {
            unsigned int curtime = g_util->TimeStamp();
            unsigned int rtm = curtime - m_uStartTimestamp;
            unsigned int ptm = (unsigned int)(showtime - basetime);
            if(rtm < ptm)
            {
              g_util->Sleep(ptm - rtm); 
            }
          }
        }
        if(!m_bInitialized)break;
        
        if(m_avPkt.stream_index == m_iVideoStreamIndex)
        {
          if(DecodeOneFrame(ct) == false)
          {
            iDropFrame++;
          }
          else
          {
            iFrame++;
          }
        }
        av_free_packet(&m_avPkt);
      }
      av_free_packet(&m_avPkt);
    }
    bool CFFMpegDecoder::DecodeOneFrame(int ct)
    {
      int frameReady = 0;
      avcodec_decode_video2(m_avContext, m_avFrame, &frameReady, &m_avPkt);
      if(frameReady)
      { 
        if(!m_bInitialized)return false;
        if(m_recv)
        {
          AllocateConvert();
          unsigned char* rgb = m_recv->GetWrite(m_width, m_height, 3);
          if(rgb)
          {
            avpicture_fill((AVPicture *) m_picrgb, rgb, AV_PIX_FMT_RGB24, m_width, m_height);
            sws_scale(m_swsContext, m_avFrame->data, m_avFrame->linesize, 0, m_height, m_picrgb->data, m_picrgb->linesize);
            unsigned int uPts = (unsigned int)(m_avFrame->pkt_pts*1000/ct - m_avFmtContext->start_time/1000);                
            m_recv->SetWrite(rgb, m_width, m_height, 24, uPts);
          }
        }
        return true;
      }
      return false;
    }

    void CFFMpegDecoder::Pause()
    { 
      if(m_iStatus == 2)return;     
      if(m_hMutex != 0)
      {
        m_hMutex->Lock();
        m_iStatus = 2;
      }
    }

    void CFFMpegDecoder::Resum()
    {
      if(m_iStatus != 2)return;
      if(m_hMutex != 0)
      {
        m_uStartTimestamp = 0;
        m_hMutex->Unlock();
        m_iStatus = 1;
      }
    }

    void CFFMpegDecoder::Seek( unsigned int ms )
    {
     
      bool bPause = (m_iStatus == 2);
      if(bPause)
      {
        Resum();
      }
      {
        MoticWeb::Utility::AutoSimpleLock lock(m_hMutex);
        if(m_bInitialized && m_avContext && m_avFmtContext)
        { 
          int ct = m_avFmtContext->streams[m_iVideoStreamIndex]->time_base.den/m_avFmtContext->streams[m_iVideoStreamIndex]->time_base.num;
          int64_t tms =(ms+m_avFmtContext->start_time/1000)*ct/1000 ;
          av_seek_frame(m_avFmtContext, m_iVideoStreamIndex, tms, 0);
          m_uStartTimestamp = 0;
        }
      }
      if(bPause)
      {
        Pause();
      }
    }

    bool CFFMpegDecoder::GetBrief( const char* fileName, ILiveReceiver* recv )
    {  
      av_register_all();
      AVFormatContext * fmtContext = 0;
      if(avformat_open_input(&fmtContext,fileName,NULL,NULL) != 0)
      {
        return false;
      }
      //find steream
      if(avformat_find_stream_info(fmtContext,NULL) < 0)
      {
        avformat_close_input(&fmtContext);
        return false;
      }
      av_dump_format(fmtContext, 0, fileName, 0);
      AVCodecContext *context =0;
      int idxStream;
      //search video stream
      for(int i =0;i<(int)fmtContext->nb_streams;i++)
      {
        if(fmtContext->streams[i]->codec->codec_type == AVMEDIA_TYPE_VIDEO)
        {      
          context = fmtContext->streams[i]->codec;  
          idxStream = i;
          break;
        }
      }
      if(context == 0)
      {
        avformat_close_input(&fmtContext);
        return false;
      }     
      if(context->width < 1 || context->height < 1)
      {
        avformat_close_input(&fmtContext);
        return false;
      } 
      bool ret = false;
      AVFrame* frame = av_frame_alloc();
      AVDictionary    *optionsDict = NULL;   
      AVPacket pkt;
      av_init_packet(&pkt);
      //AVStream* stream=NULL;
      AVCodec* codec = avcodec_find_decoder(context->codec_id);
      if(codec == 0)
      {
        goto ERRRETURN;
      }
      if (avcodec_open2(context, codec,&optionsDict) < 0)
      {
        goto ERRRETURN;
      }
      while(1)
      {  
        int ret = av_read_frame(fmtContext,&pkt);        
        if(ret < 0)
        {  
          break;     
        }
        if(pkt.stream_index == idxStream)
        {
          int frameReady = 0;
          avcodec_decode_video2(context, frame, &frameReady, &pkt);
          if(frameReady)
          { 
            SwsContext *swsContext = sws_getContext(context->width, context->height, context->pix_fmt, context->width, context->height,
                      AV_PIX_FMT_RGB24, SWS_BICUBIC, NULL, NULL, NULL);
            if(swsContext)
            {
              int size = avpicture_get_size(AV_PIX_FMT_RGB24, context->width, context->height);
              AVFrame* picrgb = av_frame_alloc();
              if(picrgb)
              {
                unsigned char* rgb = new unsigned char[size];
                avpicture_fill((AVPicture *) picrgb, rgb, AV_PIX_FMT_RGB24, context->width, context->height);
                sws_scale(swsContext, frame->data, frame->linesize, 0, context->height, picrgb->data, picrgb->linesize);
                av_frame_free(&picrgb);  
                if(recv)
                {                         
                  recv->Recv(rgb, context->width, context->height, 24, (unsigned int)(fmtContext->duration/1000));
                }
                delete[] rgb;
                av_frame_free(&picrgb);
                ret = true;
              }
              sws_freeContext(swsContext);
              swsContext = 0;
            }
            break;
          }
        }
      }
ERRRETURN: 
      if(frame)
      {
        av_frame_free(&frame);
      }
      if(context)
      {
        avcodec_close(context);
      } 
      if(fmtContext)
      {
        avformat_close_input(&fmtContext);
      }
      return ret;
    }
  }
}
