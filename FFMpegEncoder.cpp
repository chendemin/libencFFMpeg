/*
 * libencFFMpegEncoder.cpp
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
#include "OsDef.h"
#include "FFMpegEncoder.h"
#define STREAM_DURATION   10000000000000000000.0
#define STREAM_FRAME_RATE 25 /* 25 images/s */
#define STREAM_NB_FRAMES  ((int)(STREAM_DURATION * STREAM_FRAME_RATE))
#define STREAM_PIX_FMT    AV_PIX_FMT_YUV420P /* default pix_fmt */
extern MoticWeb::Utility::IUtility* g_util;
MoticWeb::Codec::CFFMpegEncoder::CFFMpegEncoder(void)
  :bit_rate(40000000)
  ,pre_ts(0)
  ,frame_rate(25)
  ,video_width(0)
  ,video_height(0)  
  ,frame(0)
  ,frame_count(0)
  ,fmt(0)
  ,oc(0)
  ,video_st(0)
  ,video_codec(0)
  ,m_mutex(0)
  ,m_thread(0)
  ,m_bRecording(false)
{
  m_videoFileOpened = false;
  if(g_util)
  {
    m_mutex = g_util->FindSystemHelper()->CreateMutex();
  }
  memset(m_databuf, 0, MAX_420PACKET*sizeof(I420Data));
}

MoticWeb::Codec::CFFMpegEncoder::~CFFMpegEncoder(void)
{
  StopRecordingThread();
  if(g_util)
  {
    if(m_mutex)g_util->FindSystemHelper()->DestroyMutex(&m_mutex);
  }
  for(int i = 0; i < MAX_420PACKET; ++i)
  {
    if(m_databuf[i].bits)
    {
      delete[]m_databuf[i].bits;
      m_databuf[i].bits = 0;
      m_databuf[i].w = m_databuf[i].h = 0;
    }
  }
  
}

bool MoticWeb::Codec::CFFMpegEncoder::Start( const char* filename, int /*enc*/ )
{
  if(filename == 0 || sizeof(filename) == 0)
  {
    return false;
  }
  {
    FILE* fd = fopen(filename, "wb");
    if(fd)
    {
      fclose(fd);
    }     
    else
    {
      printf("open file %s error!\n", filename);
      return false;
    }
  }
  av_register_all();
  m_fileName = filename;
  m_videoFileOpened = false;  
  m_lstEmpty.clear();
  m_lstFull.clear();
  for(int i = 0; i < MAX_420PACKET; ++i)
  {
    m_lstEmpty.push_back(&m_databuf[i]);
  }
  return true;
}

void MoticWeb::Codec::CFFMpegEncoder::AddFrame( unsigned char* buf, int w, int h, int channel, int bits, unsigned int ts )
{
  if(!m_videoFileOpened)
  {
    m_videoFileOpened = true;
    OpenVideoFile(m_fileName.c_str(), w, h);
    StartRecordingThread();
  }  
  I420Data* data = GetData(w, h);
  if(data)
  {
    RGB2I420(buf, data);
    data->ts = ts;
    SetData(data);
  }
  else
  {
    printf("drop frame");
  }
}

void MoticWeb::Codec::CFFMpegEncoder::End()
{
  StopRecordingThread();
  FlushData();
  CloseVideoFile();
  m_videoFileOpened = false;
}
AVStream * MoticWeb::Codec::CFFMpegEncoder::AddStream(AVCodec **codec, enum AVCodecID codec_id)
{
  AVCodecContext *c;
  AVStream *st;  
  /* find the encoder */
  *codec = avcodec_find_encoder(codec_id);
  if (!(*codec)) 
  {
    fprintf(stderr, "Could not find encoder for '%s'\n",  avcodec_get_name(codec_id));
    return NULL;
  }
  
  st = avformat_new_stream(oc, *codec);
  if (!st) 
  {
    fprintf(stderr, "Could not allocate stream\n");
    return NULL;
  }
  st->id = oc->nb_streams-1;
  c = st->codec;
  
  switch ((*codec)->type) 
  {
    case AVMEDIA_TYPE_AUDIO:
      c->sample_fmt  = (*codec)->sample_fmts ?
      (*codec)->sample_fmts[0] : AV_SAMPLE_FMT_FLTP;
      c->bit_rate    = 64000;
      c->sample_rate = 44100;
      c->channels    = 2;
      break;
      
    case AVMEDIA_TYPE_VIDEO:
      c->codec_id = codec_id;
      
      c->bit_rate = bit_rate;
      /* Resolution must be a multiple of two. */
      c->width    = video_width; //352;
      c->height   = video_height; //288;
      /* timebase: This is the fundamental unit of time (in seconds) in terms
       * of which frame timestamps are represented. For fixed-fps content,
       * timebase should be 1/framerate and timestamp increments should be
       * identical to 1. */
      c->time_base.den = frame_rate; //the time base is 1ms
      c->time_base.num = 1;
      c->gop_size      = 12; /* emit one intra frame every twelve frames at most */
      c->pix_fmt       = STREAM_PIX_FMT;
      if (c->codec_id == AV_CODEC_ID_MPEG2VIDEO) 
      {
        /* just for testing, we also add B frames */
        c->max_b_frames = 2;
      }
      if (c->codec_id == AV_CODEC_ID_MPEG1VIDEO) 
      {
        /* Needed to avoid using macroblocks in which some coeffs overflow.
         * This does not happen with normal video, it just happens here as
         * the motion of the chroma plane does not match the luma plane. */
        c->mb_decision = 2;
      }
      av_opt_set(c->priv_data, "preset", "superfast", 0);
      av_opt_set(c->priv_data, "tune", "zerolatency", 0);
      break;      
    default:
      break;
  }
  
  /* Some formats want stream headers to be separate. */
  if (oc->oformat->flags & AVFMT_GLOBALHEADER)
    c->flags |= CODEC_FLAG_GLOBAL_HEADER;
  
  return st;
}


/**************************************************************/
/* video output */



int MoticWeb::Codec::CFFMpegEncoder::OpenVideo(AVCodec *codec, AVStream *st)
{
  int ret;
  AVCodecContext *c = st->codec;
  
  /* open the codec */
  ret = avcodec_open2(c, codec, NULL);
  if (ret < 0) 
  {
    //fprintf(stderr, "Could not open video codec: %s\n", av_err2str(ret));
    return -1;
  }
  
  /* allocate and init a re-usable frame */
  frame = av_frame_alloc();
  if (!frame) 
  {
    fprintf(stderr, "Could not allocate video frame\n");
    return -2;
  }
  frame->format = c->pix_fmt;
  frame->width = c->width;
  frame->height = c->height;
  
  return 0;
}
int MoticWeb::Codec::CFFMpegEncoder::OpenVideoFile(const char *filename, int w, int h)
{   
  int ret = 0;
  video_width = w;
  video_height = h;
  pre_ts = 0;  
  /* allocate the output media context */
  avformat_alloc_output_context2(&oc, NULL, NULL, filename);
  if (!oc) 
  {
    //printf("Could not deduce output format from file extension: using MPEG.\n");
    avformat_alloc_output_context2(&oc, NULL, "mpeg", filename);
  }
  if (!oc) 
  {
    return 1;
  }
  fmt = oc->oformat;
  
  if(fmt->flags & AVFMT_VARIABLE_FPS)
  {
    printf("Variable FPS");
  }
  
  /* Add the audio and video streams using the default format codecs
   * and initialize the codecs. */
  video_st = NULL;
  
  if (fmt->video_codec != AV_CODEC_ID_NONE)
  {
    video_st = AddStream(&video_codec, fmt->video_codec);
  }
  
  /* Now that all the parameters are set, we can open the audio and
   * video codecs and allocate the necessary encode buffers. */
  if (video_st)
  {
    ret = OpenVideo(video_codec, video_st);
    if(ret != 0)return 1;
  }
  
  av_dump_format(oc, 0, filename, 1);
  
  /* open the output file, if needed */
  if (!(fmt->flags & AVFMT_NOFILE)) 
  {
    ret = avio_open(&oc->pb, filename, AVIO_FLAG_WRITE);
    if (ret < 0) 
    {
     // fprintf(stderr, "Could not open '%s': %s\n", filename,
     //        av_err2str(ret));
      return 1;
    }
  }
  
  /* Write the stream header, if any. */
  ret = avformat_write_header(oc, NULL);
  if (ret < 0) 
  {
    //fprintf(stderr, "Error occurred when opening output file: %s\n",
    //        av_err2str(ret));
    return 1;
  }  
  return 0;
}

void MoticWeb::Codec::CFFMpegEncoder::CloseVideoFile()
{
  /* Write the trailer, if any. The trailer must be written before you
   * close the CodecContexts open when you wrote the header; otherwise
   * av_write_trailer() may try to use memory that was freed on
   * av_codec_close(). */
  if (oc == 0)return;
  av_write_trailer(oc);
  
  /* Close each codec. */
  if (video_st)
    CloseVideo(video_st);
  
  if (!(fmt->flags & AVFMT_NOFILE))
  /* Close the output file. */
    avio_close(oc->pb);
  

  /* free the stream */
  avformat_free_context(oc);
  oc = 0;  
}

void MoticWeb::Codec::CFFMpegEncoder::CloseVideo(AVStream * st )
{
    avcodec_close(st->codec);
    av_frame_free(&frame); 
}

void bitmap2Yuv420p_calc2(uint8_t *destination, const uint8_t *rgb, size_t width, size_t height)
{
  size_t image_size = width * height;
  size_t upos = image_size;
  size_t vpos = upos + upos / 4;
  size_t i = 0;
  for( size_t line = 0; line < height; line += 2 )
  {
    for( size_t x = 0; x < width; x += 2 )
    {
      uint8_t r = rgb[0];
      uint8_t g = rgb[1];
      uint8_t b = rgb[2];

      destination[i++] = ((66*r + 129*g + 25*b) >> 8) + 16;

      destination[upos++] = ((-38*r + -74*g + 112*b) >> 8) + 128;
      destination[vpos++] = ((112*r + -94*g + -18*b) >> 8) + 128;
      rgb += 3;
      r = rgb[0];
      g = rgb[1];
      b = rgb[2];
      rgb += 3;
      destination[i++] = ((66*r + 129*g + 25*b) >> 8) + 16;
    }
    for( size_t x = 0; x < width; x += 1 )
    {
      uint8_t r = rgb[0];
      uint8_t g = rgb[1];
      uint8_t b = rgb[2];
      destination[i++] = ((66*r + 129*g + 25*b) >> 8) + 16;
      rgb += 3;
    }
  }
}

I420Data* MoticWeb::Codec::CFFMpegEncoder::GetData( int w, int h )
{  
  bool bDrop = false;
  I420Data* data = 0; 
  {
    MoticWeb::Utility::AutoSimpleLock lock(m_mutex);
    if(m_lstEmpty.size() > 0)
    {
      data = m_lstEmpty.front();
      m_lstEmpty.pop_front();    
    }
    else if(m_lstFull.size() > 2)
    {
        data = m_lstFull.front();
        m_lstFull.pop_front();
        printf("drop front.\n");
        bDrop = true;
    }
  }
  if(data)
  {
    if(data->w != w || data->h != h)
    {
      if(data->bits)
      {
        delete[] data->bits;
      }
      data->bits = new unsigned char[(w*h*3 + 1)/2];
      data->w = w;
      data->h = h;
    }
  }
  if(bDrop)
  {
    g_util->Sleep(20);
  }
  return data;
}

void MoticWeb::Codec::CFFMpegEncoder::SetData( I420Data* data )
{
  MoticWeb::Utility::AutoSimpleLock lock(m_mutex);
  m_lstFull.push_back(data);
}

void MoticWeb::Codec::CFFMpegEncoder::RGB2I420( unsigned char* buf, I420Data* data )
{
  bitmap2Yuv420p_calc2(data->bits, buf, data->w, data->h);
}

void MoticWeb::Codec::CFFMpegEncoder::StartRecordingThread()
{
  if(m_thread)return;
  if(g_util)
  {
    m_bRecording = true;
    m_thread = g_util->FindSystemHelper()->CreateThread(this);
  }
}

void MoticWeb::Codec::CFFMpegEncoder::StopRecordingThread()
{
  if(g_util && m_thread)
  {    
    m_bRecording = false;
    g_util->FindSystemHelper()->DestroyThread(&m_thread, 10000L);
  }
}

void MoticWeb::Codec::CFFMpegEncoder::ThreadRun()
{
  frame_count = 0;
  while(m_bRecording)
  {
    I420Data* data = 0;
    {
      MoticWeb::Utility::AutoSimpleLock lock(m_mutex);
      if(m_lstFull.size() > 0)
      {
        data = m_lstFull.front();
        m_lstFull.pop_front();
      }
    }
    if(data == 0)
    {
      g_util->Sleep(20);
      continue;
    }
    WriteFrame(data);
    {
      MoticWeb::Utility::AutoSimpleLock lock(m_mutex);
      m_lstEmpty.push_back(data);
    }
  }
}

int MoticWeb::Codec::CFFMpegEncoder::WriteFrame( I420Data* data )
{
  int ret;
  AVCodecContext *c = video_st->codec;
  
  //if (frame_count >= STREAM_NB_FRAMES) 
  //{
  //  /* No more frames to compress. The codec has a latency of a few
  //   * frames if using B-frames, so we get the last frames by
  //   * passing the same picture again. */
  //}   
  if (oc->oformat->flags & AVFMT_RAWPICTURE) 
  {
    /* Raw video case - directly store the picture in the packet */
    AVPacket pkt;
    av_init_packet(&pkt);
    
    pkt.flags        |= AV_PKT_FLAG_KEY;
    pkt.stream_index  = video_st->index;
    pkt.data          = data->bits;
    pkt.size          = sizeof(AVPicture);
    
    //printf("pts:%ld \n",pkt.pts);
    //printf("dts:%ld \n",pkt.dts);
    //printf("duration:%d \n",pkt.duration);
    //printf("frame count:%d \n\n",frame_count);
    
    ret = av_interleaved_write_frame(oc, &pkt);
  } 
  else 
  {
    //double ss = av_q2d(st->time_base);
    AVPacket pkt = { 0 };
    int got_packet;
    av_init_packet(&pkt);
    
    int64_t t = (int64_t)data->ts;
    t *= video_st->time_base.den;
    t /= 1000;

    int sz = data->w*data->h;
    /* encode the image */
    frame->data[0] = data->bits;
    frame->data[1] = data->bits + sz;
    frame->data[2] = frame->data[1] + (sz>>2);
    frame->linesize[0] = data->w;
    frame->linesize[1] = frame->linesize[2] = (data->w >> 1);
    frame->pts = t;
    if(frame_count == 0)
    {
      frame->flags |= AV_PKT_FLAG_KEY;
    }
    else
    {
      frame->flags = 0;
    }
    ret = avcodec_encode_video2(c, &pkt, frame, &got_packet);
    if (ret < 0) 
    {
      //fprintf(stderr, "Error encoding video frame: %s\n", av_err2str(ret));
      return -1;
    }
    /* If size is zero, it means the image was buffered. */
    
    if (!ret && got_packet && pkt.size) 
    {     
      /* rescale output packet timestamp values from codec to stream timebase */
      pkt.pts = t;//av_rescale_q_rnd(t, c->time_base, st->time_base, AV_ROUND_NEAR_INF|AV_ROUND_PASS_MINMAX);
      pkt.dts = t;//av_rescale_q_rnd(t, c->time_base, st->time_base, AV_ROUND_NEAR_INF|AV_ROUND_PASS_MINMAX);
      pkt.duration = (data->ts - pre_ts)*video_st->time_base.den/1000; //av_rescale_q(pkt.duration, c->time_base, st->time_base);
      pkt.stream_index = video_st->index;
      pre_ts = data->ts;
      /*printf("pts:%ld \n",pkt.pts);
      printf("dts:%ld \n",pkt.dts);
      printf("duration:%d \n",pkt.duration);
      printf("frame count:%d \n\n",frame_count);*/
      
      /* Write the compressed frame to the media file. */
      ret = av_interleaved_write_frame(oc, &pkt);
    }
    else
    {
      ret = 0;
    }
  }
  if (ret != 0) 
  {
    //fprintf(stderr, "Error while writing video frame: %s\n", av_err2str(ret));
  }
  frame_count++;
  return ret;
}

void MoticWeb::Codec::CFFMpegEncoder::FlushData()
{
  MoticWeb::Utility::AutoSimpleLock lock(m_mutex);
  while(m_lstFull.size() > 0)
  {
    I420Data* data = m_lstFull.front();
    m_lstFull.pop_front();
    WriteFrame(data);
    m_lstEmpty.push_back(data);
  }
}
