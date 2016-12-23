/*
 * libencFFMpeg.cpp
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
#include "libencFFMpeg.h"
#include "FFMpegEncoder.h"
#include "FFMpegDecoder.h"
MoticWeb::Utility::IUtility* g_util = 0;
MoticWeb::Codec::IEncoder* CreateEncoder()
{
  return new MoticWeb::Codec::CFFMpegEncoder();
}

void DestroyEncoder( MoticWeb::Codec::IEncoder**en )
{
  MoticWeb::Codec::CFFMpegEncoder*enc = dynamic_cast<MoticWeb::Codec::CFFMpegEncoder*>(*en);
  if(enc)
  {
    delete enc;
    *en = 0;
  }
}

void SetUtility( MoticWeb::Utility::IUtility*util )
{
  g_util = util;
}

MoticWeb::Codec::IDecoder* CreateDecoder()
{
  return new MoticWeb::Codec::CFFMpegDecoder();
}
void DestroyDecoder(MoticWeb::Codec::IDecoder**de)
{
  MoticWeb::Codec::CFFMpegDecoder*dec = dynamic_cast<MoticWeb::Codec::CFFMpegDecoder*>(*de);
  if(dec)
  {
    delete dec;
    *de = 0;
  }
}