/******************************************************************************
 * Copyright 2024 The HyperLink Authors. All Rights Reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *****************************************************************************/

#pragma once

#include <mutex>
#include <string>
#include "Checks.hpp"
#include <thread>
#include "play_task_consumer.h"

#include <dw/core/VersionCurrent.h>
#include <dw/sensors/Sensors.h>
#include <dw/sensors/SensorSerializer.h>
#include <dw/sensors/camera/Camera.h>
#include <dw/interop/streamer/ImageStreamer.h>

#ifdef _ON_ORIN_
#include <dw/core/context/Context.h>
#include <dw/core/system/NvMedia.h>
#elif _ON_PEGASUS_
#include <dw/core/Context.h>
#include <dw/core/NvMedia.h>
#endif

namespace apollo {
namespace cyber {
namespace record {

class DwCameraDeserializer
{
protected:
    typedef std::chrono::high_resolution_clock myclock_t;
    typedef std::chrono::time_point<myclock_t> timepoint_t;
private:

    // ------------------------------------------------
    // Sample specific variables
    // ------------------------------------------------
    dwContextHandle_t m_context           = DW_NULL_HANDLE;
    dwSALHandle_t m_sal                   = DW_NULL_HANDLE;
    dwSensorHandle_t m_camera             = DW_NULL_HANDLE;
    dwCameraFrameHandle_t m_frame         = DW_NULL_HANDLE;
    dwCameraProperties m_cameraProps      = {};
    dwImageProperties m_cameraImageProps  = {};

    dwImageHandle_t m_imageRGBA = DW_NULL_HANDLE;

    dwImageStreamerHandle_t m_streamerToCPUGrab = DW_NULL_HANDLE; 
    // ------------------------------------------------
    // Time
    // ------------------------------------------------
    /// Defines the minimum time between calls to onProcess()
    myclock_t::duration m_processPeriod;
    timepoint_t m_lastFPSRunTime;

    std::string m_h264_file;
    uint8_t m_channel_index;

    void initializeDriveWorks(dwContextHandle_t& context);

    void setProcessRate(int loopsPerSecond);

    void tryToSleep(timepoint_t lastRunTime);

    myclock_t::duration convertFrequencyToPeriod(int loopsPerSecond);

    bool m_run;
public:
    DwCameraDeserializer(const std::string& filename);

    void run();

    void start();

    void stop(){m_run = false;}

    void onProcess(const std::string &ch, double measurement_time, double header_timestamp);
    
    bool onInitialize();

    void onRelease();
};

}  // namespace record
}  // namespace cyber
}  // namespace apollo

