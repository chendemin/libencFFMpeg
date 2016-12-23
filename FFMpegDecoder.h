/*
 * libencFFMpegDecoder.h
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
extern "C"{
#include <libavutil/opt.h>
#include <libavutil/mathematics.h>
#include <libavformat/avformat.h>
#include <libswscale/swscale.h>
#include <libswresample/swresample.h>
}
#include "IDecoder.h"
#include "IUtility.h"
namespace MoticWeb
{
  namespace Codec
  {
    class CFFMpegDecoder:public IDecoder,public MoticWeb::Utility::IThreadCallBack
    {
    public:
      CFFMpegDecoder(void);
      virtual ~CFFMpegDecoder(void);
    public:
      bool Start(const char* fileName, IImgBuffering* recv);
      bool Start(int type);
      void AddBuffer(unsigned char*buf, int size, IImgBuffering*img);
      void End();
      bool GetBrief(const char* fileName, ILiveReceiver* recv);

      virtual void Pause();
      virtual void Resum();
      virtual void Seek(unsigned int ms);

     /* unsigned char* ConverToRGB24( AVFrame * frame );*/
      void AllocateConvert();
      void FreeConvert();
    public:
      void ThreadRun();
    private:
      bool DecodeOneFrame(int ct);
    private:
      bool m_bInitialized;
      AVCodec *m_avCodec;
      AVCodecContext *m_avContext; 
      AVFormatContext* m_avFmtContext;
      AVFrame *m_avFrame;
      AVPacket m_avPkt;
      SwsContext *m_swsContext;
      AVFrame *m_picrgb;           
      int m_width;
      int m_height;
     // int m_rgbSize;
      int m_iVideoStreamIndex;
      MoticWeb::Utility::ISimpleThread* m_hTread;
      IImgBuffering* m_recv;
      MoticWeb::Utility::ISimpleMutex* m_hMutex;
      int m_iStatus;
      unsigned int m_uStartTimestamp;
    };
  }
}