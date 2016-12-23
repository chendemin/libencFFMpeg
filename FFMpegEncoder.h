/*
 * libencFFMpegEncoder.h
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
#pragma once
#include "IEncoder.h"
#include "IUtility.h"
#include <list>
extern "C"{
#include <libavutil/opt.h>
#include <libavutil/mathematics.h>
#include <libavformat/avformat.h>
#include <libswscale/swscale.h>
#include <libswresample/swresample.h>
}
#define MAX_420PACKET 5
namespace MoticWeb
{
  namespace Codec
  {
    typedef struct  
    {
      unsigned char* bits;
      int w;
      int h;
      unsigned int ts;
    }I420Data;

    class CFFMpegEncoder:public IEncoder, public MoticWeb::Utility::IThreadCallBack    
    {
    public:
      CFFMpegEncoder(void);
      virtual ~CFFMpegEncoder(void);
    public://IEncoder
      virtual bool Start(const char* filename, int enc);
      virtual void AddFrame(unsigned char* buf, int w, int h, int channel, int bits, unsigned int ts);
      virtual void End();
    public://Thead run
      virtual void ThreadRun();
    private:
      int OpenVideoFile(const char *filename, int w, int h);
      AVStream * AddStream(AVCodec **codec, enum AVCodecID codec_id);
      int OpenVideo(AVCodec *codec, AVStream *st);
      void CloseVideo(AVStream * video_st );
      void RGBToYUV420p( unsigned char* buf, int w, int h, int channel, int bits );      
      void CloseVideoFile();
      I420Data* GetData( int w, int h );
      void SetData( I420Data* data );
      void RGB2I420( unsigned char* buf, I420Data* data );
      void StartRecordingThread();
      void StopRecordingThread();
      int WriteFrame( I420Data* data );
      void FlushData();
    private:
      std::string m_fileName;
      bool m_videoFileOpened;
    private:  
      int bit_rate;
      unsigned int pre_ts;   /* previous timestamp */
      int frame_rate;
      int video_width;
      int video_height;
      AVFrame *frame;
      int frame_count;
      AVOutputFormat *fmt;
      AVFormatContext *oc;
      AVStream *video_st;
      AVCodec *video_codec;
      std::list<I420Data*>m_lstFull;
      std::list<I420Data*>m_lstEmpty;
      I420Data m_databuf[MAX_420PACKET];//maximum buffer 5 frames
      MoticWeb::Utility::ISimpleMutex* m_mutex;
      MoticWeb::Utility::ISimpleThread* m_thread;
      bool m_bRecording;
    };

  }
}
