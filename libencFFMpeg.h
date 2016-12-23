/*
 * libencFFMpeg.h
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
#include "OsDef.h"
#include "IDecoder.h"
#include "IEncoder.h"
#include "IUtility.h"
#ifdef __cplusplus
extern "C"{
#endif  
  MoticWeb::Codec::IEncoder* CreateEncoder();
  void DestroyEncoder(MoticWeb::Codec::IEncoder**en);
  void SetUtility(MoticWeb::Utility::IUtility*util);
  MoticWeb::Codec::IDecoder* CreateDecoder();
  void DestroyDecoder(MoticWeb::Codec::IDecoder**dec);
#ifdef __cplusplus
}
#endif