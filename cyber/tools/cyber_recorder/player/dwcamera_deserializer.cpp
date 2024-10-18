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

#include "dwcamera_deserializer.h"
#include "Log.hpp"

namespace apollo {
namespace cyber {
namespace record {


DwCameraDeserializer::DwCameraDeserializer(const std::string& filename)
    : m_h264_file(filename) {}

auto DwCameraDeserializer::convertFrequencyToPeriod(int loopsPerSecond) -> myclock_t::duration
{
    using ns = std::chrono::nanoseconds;

    if (loopsPerSecond <= 0)
        return std::chrono::milliseconds(0);
    else
        return ns(static_cast<ns::rep>(1e9 / loopsPerSecond));
}

void DwCameraDeserializer::setProcessRate(int loopsPerSecond)
{
    m_processPeriod = convertFrequencyToPeriod(loopsPerSecond);
}

void DwCameraDeserializer::tryToSleep(timepoint_t lastRunTime)
{
    // This is the time that the previous iteration took
    auto timeSinceUpdate = myclock_t::now() - lastRunTime;

    // Decide which is the period for the master run loop
    myclock_t::duration runPeriod;
    runPeriod = m_processPeriod;

    // Limit framerate, sleep if necessary
    if (timeSinceUpdate < runPeriod)
    {
        auto sleepDuration = runPeriod - timeSinceUpdate;
        std::this_thread::sleep_for(sleepDuration);
    }
}

// void DwCameraDeserializer::run()
// {
//     // Main program loop
//     m_run = true;
//     if (!onInitialize()){
//         return;
//     }
//     timepoint_t lastRunIterationTime = myclock_t::now() - m_processPeriod;
//     timepoint_t nextOnProcessTime    = myclock_t::now();
//     while (m_run){
//         // Iteration
//         if (myclock_t::now() >= nextOnProcessTime)
//         {
//             nextOnProcessTime = myclock_t::now() + m_processPeriod;
//             onProcess();
//         }

//         // Sleep here in the middle so the rendering
//         // is as regular as possible
//         tryToSleep(lastRunIterationTime);
//         lastRunIterationTime = myclock_t::now();
//     }
//     onRelease();
//     return;
// }

bool DwCameraDeserializer::onInitialize() {

    {
        initializeDriveWorks(m_context);
        dwSAL_initialize(&m_sal, m_context);
    }

    // initialize sensors
    {
        dwSensorParams sensorParams{};
        sensorParams.protocol   = "camera.virtual";
        sensorParams.parameters = m_h264_file.c_str();
        CHECK_DW_ERROR_MSG(dwSAL_createSensor(&m_camera, sensorParams, m_sal),
                            "Cannot create virtual camera sensor, maybe wrong video file?");

        dwSensorCamera_getSensorProperties(&m_cameraProps, m_camera);

        // we would like the application run as fast as the original video
        setProcessRate(m_cameraProps.framerate);
        // std::cout << "m_cameraProps.framerate = " << m_cameraProps.framerate << std::endl;
    }

    // -----------------------------
    // initialize streamer and software isp for raw video playback, if the video input is raw
    // -----------------------------

    dwImageProperties from{};

    CHECK_DW_ERROR(dwSensorCamera_getImageProperties(&from, DW_CAMERA_OUTPUT_CUDA_RGBA_UINT8, m_camera));

    CHECK_DW_ERROR(dwImageStreamer_initialize(&m_streamerToCPUGrab, &from, DW_IMAGE_CPU, m_context));

    // -----------------------------
    // Start Sensors
    // -----------------------------
    // CHECK_DW_ERROR(dwSensor_start(m_camera));

    std::cout << "Initialization complete." << std::endl;
    return true;
}

void DwCameraDeserializer::start() {
    CHECK_DW_ERROR(dwSensor_start(m_camera));
}

void DwCameraDeserializer::initializeDriveWorks(dwContextHandle_t& context) {
    // initialize logger to print verbose message on console in color
    CHECK_DW_ERROR(dwLogger_initialize(getConsoleLoggerCallback(true)));
    CHECK_DW_ERROR(dwLogger_setLogLevel(DW_LOG_VERBOSE));

    // initialize SDK context, using data folder
    dwContextParameters sdkParams = {};
    CHECK_DW_ERROR(dwInitialize(&context, DW_VERSION, &sdkParams));
}

void DwCameraDeserializer::onRelease() {
    if (m_frame)
        dwSensorCamera_returnFrame(&m_frame);

    // stop sensor
    dwSensor_stop(m_camera);

    if (m_streamerToCPUGrab)
    {
        dwImageStreamer_release(m_streamerToCPUGrab);
    }

    // release sensor
    dwSAL_releaseSensor(m_camera);

    // -----------------------------------------
    // Release DriveWorks handles, context and SAL
    // -----------------------------------------
    {
        dwSAL_release(m_sal);
        CHECK_DW_ERROR(dwRelease(m_context));
        CHECK_DW_ERROR(dwLogger_release());
    }
}


void DwCameraDeserializer::onProcess(const std::string &ch, double measurement_time, double header_timestamp)
{
    // apollo::cyber::record::PlayTaskProducer* taskProducer = apollo::cyber::record::PlayTaskProducer::getInstance();
    // if (taskProducer->IsImageBufferFull(m_channel_index)) {
    //     std::this_thread::sleep_for(std::chrono::milliseconds(500));
    // }
    // return the previous frame to camera
    if (m_frame)
    {
        CHECK_DW_ERROR(dwSensorCamera_returnFrame(&m_frame));
    }

    // ---------------------------
    // grab frame from camera
    // ---------------------------
    uint32_t countFailure = 0;
    dwStatus status       = DW_NOT_READY;

    while ((status == DW_NOT_READY) || (status == DW_END_OF_STREAM) || (status == DW_TIME_OUT))
    {
#ifdef _ON_ORIN_
        status = dwSensorCamera_readFrame(&m_frame, 600000, m_camera);
#elif _ON_PEGASUS_
        status = dwSensorCamera_readFrame(&m_frame, 0, 600000, m_camera);
#endif
        countFailure++;
        if (countFailure == 1000000)
        {
            std::cout << "Camera virtual doesn't seem responsive, exit loop and stopping the sample" << std::endl;
            return;
        }

        if (status == DW_END_OF_STREAM)
        {
            std::cout << "Video reached end of stream" << std::endl;
            CHECK_DW_ERROR(dwSensor_reset(m_camera));
        }
        else if ((status != DW_TIME_OUT) && (status != DW_NOT_READY))
        {
            CHECK_DW_ERROR(status);
        }
    }
    CHECK_DW_ERROR(dwSensorCamera_getImage(&m_imageRGBA, DW_CAMERA_OUTPUT_CUDA_RGBA_UINT8, m_frame));

    if (m_imageRGBA)
    {
        // grab frame
        dwTime_t timeout = 33000;

        // stream that image to the CPU domain
        CHECK_DW_ERROR(dwImageStreamer_producerSend(m_imageRGBA, m_streamerToCPUGrab));

        // receive the streamed image as a handle
        dwImageHandle_t frameCPU;
        CHECK_DW_ERROR(dwImageStreamer_consumerReceive(&frameCPU, timeout, m_streamerToCPUGrab));

        // get an image from the frame
        dwImageCPU *imgCPU;
        CHECK_DW_ERROR(dwImage_getCPU(&imgCPU, frameCPU));

        // write the image to cameracomponent
        // apollo::cyber::record::PlayTaskProducer* taskProducer = apollo::cyber::record::PlayTaskProducer::getInstance();
        // if (taskProducer != nullptr) {
        //     taskProducer->PushDecodedDataToBuffer(imgCPU->data[0], m_channel_index,
        //     imgCPU->prop.width, imgCPU->prop.height);
        // }
        apollo::cyber::record::PlayTaskConsumer* taskConsumer = apollo::cyber::record::PlayTaskConsumer::getInstance();
        if (taskConsumer != nullptr) {
            taskConsumer->writeImageToComponent(ch, imgCPU->data[0], m_channel_index,
            imgCPU->prop.width, imgCPU->prop.height, measurement_time, header_timestamp);
        }

        // returned the consumed image
        CHECK_DW_ERROR(dwImageStreamer_consumerReturn(&frameCPU, m_streamerToCPUGrab));

        // notify the producer that the work is done
        CHECK_DW_ERROR(dwImageStreamer_producerReturn(nullptr, timeout, m_streamerToCPUGrab));
    }
}

}  // namespace dwcamera
}  // namespace drivers
}  // namespace apollo

