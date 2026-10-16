/*
 * SPDX-FileCopyrightText: 2026 The PixelOS Project
 * SPDX-License-Identifier: Apache-2.0
 */

/*
 * Wrapper around the stock camera HAL (camera.qcom.so), selected through
 * ro.hardware.camera=tundra. It forwards everything to the stock module and
 * adjusts what the HAL reports back for some streams after configure_streams:
 *
 * - 10-bit implementation defined output streams get an explicit TP10 UBWC
 *   (or P010) format. CamX validates their buffers against the TP10 size,
 *   but gralloc would otherwise pick NV12 for them and the provider aborts.
 * - Video encoder streams that are left at BT.2020 with the SDR transfer are
 *   tagged with what CamX actually writes: BT.2020 PQ for 10-bit (HDR10),
 *   BT.709 otherwise. The encoder surface defaults to BT.2020 for 4K when the
 *   recorder sets no color aspects.
 *
 * CamX keeps pointers to the streams it is configured with and may read them
 * again later, so it is given private copies that keep the requested format
 * and dataspace. Stream pointers are translated between the copies and the
 * provider's streams in both directions.
 */

#define LOG_TAG "CameraWrapper"

#include <dlfcn.h>
#include <errno.h>
#include <string.h>

#include <memory>
#include <mutex>
#include <unordered_map>
#include <vector>

#include <hardware/camera3.h>
#include <hardware/camera_common.h>
#include <hardware/gralloc.h>
#include <log/log.h>

#ifdef __LP64__
#define STOCK_HAL_PATH "/vendor/lib64/hw/camera.qcom.so"
#else
#define STOCK_HAL_PATH "/vendor/lib/hw/camera.qcom.so"
#endif

// From QtiGrallocDefs.h
#define GRALLOC_USAGE_PRIVATE_HEIF (1U << 27)
#define GRALLOC_USAGE_PRIVATE_ALLOC_UBWC (1U << 28)
#define GRALLOC_USAGE_PRIVATE_10BIT (1U << 30)
#define HAL_PIXEL_FORMAT_YCbCr_420_P010 0x36
#define HAL_PIXEL_FORMAT_YCbCr_420_TP10_UBWC 0x7FA30C09

namespace {

camera_module_t* gStockModule;

struct CameraDevice;

struct CallbackOps : camera3_callback_ops_t {
    CameraDevice* device;
};

struct CameraDevice {
    camera3_device_t device;
    camera3_device_ops_t ops;
    camera3_device_t* stock;
    const camera3_callback_ops_t* frameworkCallbacks;
    CallbackOps callbacks;

    std::mutex lock;
    // Provider stream -> private copy handed to CamX, and back
    std::unordered_map<camera3_stream_t*, std::unique_ptr<camera3_stream_t>> shadows;
    std::unordered_map<camera3_stream_t*, camera3_stream_t*> frameworkStreams;

    camera3_stream_t* toStock(camera3_stream_t* stream) {
        if (stream == nullptr) return nullptr;
        std::lock_guard<std::mutex> l(lock);
        auto it = shadows.find(stream);
        return it != shadows.end() ? it->second.get() : stream;
    }

    camera3_stream_t* toFramework(camera3_stream_t* stream) {
        if (stream == nullptr) return nullptr;
        std::lock_guard<std::mutex> l(lock);
        auto it = frameworkStreams.find(stream);
        return it != frameworkStreams.end() ? it->second : stream;
    }
};

CameraDevice* getDevice(const camera3_device_t* device) {
    return static_cast<CameraDevice*>(device->priv);
}

CameraDevice* getDevice(const camera3_callback_ops_t* ops) {
    return static_cast<const CallbackOps*>(ops)->device;
}

/*
 * Stream overrides
 */

// Mirrors the 10-bit cases of gralloc's GetImplDefinedFormat()
void overrideFormat(camera3_stream_t* stream, uint32_t usage) {
    if (stream->format != HAL_PIXEL_FORMAT_IMPLEMENTATION_DEFINED ||
        !(usage & GRALLOC_USAGE_PRIVATE_10BIT) || (usage & GRALLOC_USAGE_HW_CAMERA_READ) ||
        ((usage & GRALLOC_USAGE_HW_VIDEO_ENCODER) && (usage & GRALLOC_USAGE_PRIVATE_HEIF))) {
        return;
    }

    stream->format = (usage & GRALLOC_USAGE_PRIVATE_ALLOC_UBWC)
                             ? HAL_PIXEL_FORMAT_YCbCr_420_TP10_UBWC
                             : HAL_PIXEL_FORMAT_YCbCr_420_P010;
    ALOGI("%ux%u stream: using 10-bit format 0x%x", stream->width, stream->height,
          stream->format);
}

void overrideDataSpace(camera3_stream_t* stream, uint32_t usage) {
    if (!(usage & GRALLOC_USAGE_HW_VIDEO_ENCODER) ||
        (stream->data_space & HAL_DATASPACE_STANDARD_MASK) != HAL_DATASPACE_STANDARD_BT2020 ||
        (stream->data_space & HAL_DATASPACE_TRANSFER_MASK) != HAL_DATASPACE_TRANSFER_SMPTE_170M) {
        return;
    }

    const bool is10Bit = usage & GRALLOC_USAGE_PRIVATE_10BIT;
    stream->data_space = is10Bit ? static_cast<android_dataspace_t>(HAL_DATASPACE_BT2020_ITU_PQ)
                                 : HAL_DATASPACE_V0_BT709;
    ALOGI("%ux%u video stream: tagging %s as %s", stream->width, stream->height,
          is10Bit ? "10-bit" : "8-bit", is10Bit ? "BT2020 PQ" : "BT709");
}

/*
 * Callbacks from CamX
 */

void processCaptureResult(const camera3_callback_ops_t* ops,
                          const camera3_capture_result_t* result) {
    CameraDevice* dev = getDevice(ops);

    camera3_capture_result_t fwResult = *result;
    std::vector<camera3_stream_buffer_t> outputBuffers(
            result->output_buffers, result->output_buffers + result->num_output_buffers);
    for (auto& buffer : outputBuffers) {
        buffer.stream = dev->toFramework(buffer.stream);
    }
    fwResult.output_buffers = outputBuffers.data();

    camera3_stream_buffer_t inputBuffer;
    if (result->input_buffer != nullptr) {
        inputBuffer = *result->input_buffer;
        inputBuffer.stream = dev->toFramework(inputBuffer.stream);
        fwResult.input_buffer = &inputBuffer;
    }

    dev->frameworkCallbacks->process_capture_result(dev->frameworkCallbacks, &fwResult);
}

void notify(const camera3_callback_ops_t* ops, const camera3_notify_msg_t* msg) {
    CameraDevice* dev = getDevice(ops);

    camera3_notify_msg_t fwMsg = *msg;
    if (msg->type == CAMERA3_MSG_ERROR) {
        fwMsg.message.error.error_stream = dev->toFramework(msg->message.error.error_stream);
    }

    dev->frameworkCallbacks->notify(dev->frameworkCallbacks, &fwMsg);
}

camera3_buffer_request_status_t requestStreamBuffers(
        const camera3_callback_ops_t* ops, uint32_t num_buffer_reqs,
        const camera3_buffer_request_t* buffer_reqs, uint32_t* num_returned_buf_reqs,
        camera3_stream_buffer_ret_t* returned_buf_reqs) {
    CameraDevice* dev = getDevice(ops);

    std::vector<camera3_buffer_request_t> requests(buffer_reqs, buffer_reqs + num_buffer_reqs);
    for (auto& request : requests) {
        request.stream = dev->toFramework(request.stream);
    }

    camera3_buffer_request_status_t status = dev->frameworkCallbacks->request_stream_buffers(
            dev->frameworkCallbacks, num_buffer_reqs, requests.data(), num_returned_buf_reqs,
            returned_buf_reqs);

    for (uint32_t i = 0; i < *num_returned_buf_reqs; i++) {
        camera3_stream_buffer_ret_t& ret = returned_buf_reqs[i];
        ret.stream = dev->toStock(ret.stream);
        for (uint32_t j = 0; j < ret.num_output_buffers; j++) {
            ret.output_buffers[j].stream = dev->toStock(ret.output_buffers[j].stream);
        }
    }

    return status;
}

void returnStreamBuffers(const camera3_callback_ops_t* ops, uint32_t num_buffers,
                         const camera3_stream_buffer_t* const* buffers) {
    CameraDevice* dev = getDevice(ops);

    std::vector<camera3_stream_buffer_t> fwBuffers(num_buffers);
    std::vector<const camera3_stream_buffer_t*> fwBufferPtrs(num_buffers);
    for (uint32_t i = 0; i < num_buffers; i++) {
        fwBuffers[i] = *buffers[i];
        fwBuffers[i].stream = dev->toFramework(fwBuffers[i].stream);
        fwBufferPtrs[i] = &fwBuffers[i];
    }

    dev->frameworkCallbacks->return_stream_buffers(dev->frameworkCallbacks, num_buffers,
                                                   fwBufferPtrs.data());
}

/*
 * Device ops
 */

int initialize(const camera3_device_t* device, const camera3_callback_ops_t* callback_ops) {
    CameraDevice* dev = getDevice(device);

    dev->frameworkCallbacks = callback_ops;
    dev->callbacks.process_capture_result = processCaptureResult;
    dev->callbacks.notify = notify;
    dev->callbacks.request_stream_buffers =
            callback_ops->request_stream_buffers ? requestStreamBuffers : nullptr;
    dev->callbacks.return_stream_buffers =
            callback_ops->return_stream_buffers ? returnStreamBuffers : nullptr;
    dev->callbacks.device = dev;

    return dev->stock->ops->initialize(dev->stock, &dev->callbacks);
}

int configureStreams(const camera3_device_t* device, camera3_stream_configuration_t* config) {
    CameraDevice* dev = getDevice(device);

    std::vector<camera3_stream_t*> stockStreams(config->num_streams);
    std::vector<uint32_t> requestedUsages(config->num_streams);
    {
        std::lock_guard<std::mutex> l(dev->lock);

        // Streams left out of the configuration are deleted. Drop their copies, the
        // provider may reuse their addresses for new streams.
        std::unordered_map<camera3_stream_t*, std::unique_ptr<camera3_stream_t>> shadows;
        dev->frameworkStreams.clear();
        for (uint32_t i = 0; i < config->num_streams; i++) {
            camera3_stream_t* stream = config->streams[i];
            auto it = dev->shadows.find(stream);
            std::unique_ptr<camera3_stream_t> shadow;
            if (it != dev->shadows.end()) {
                shadow = std::move(it->second);
            } else {
                shadow = std::make_unique<camera3_stream_t>(*stream);
            }

            // Keep what the HAL owns (max_buffers, priv), take the rest from the provider
            void* priv = shadow->priv;
            uint32_t maxBuffers = shadow->max_buffers;
            *shadow = *stream;
            if (it != dev->shadows.end()) {
                shadow->priv = priv;
                shadow->max_buffers = maxBuffers;
            }

            requestedUsages[i] = stream->usage;
            stockStreams[i] = shadow.get();
            dev->frameworkStreams[shadow.get()] = stream;
            shadows[stream] = std::move(shadow);
        }
        dev->shadows = std::move(shadows);
    }

    camera3_stream_configuration_t stockConfig = *config;
    stockConfig.streams = stockStreams.data();

    int ret = dev->stock->ops->configure_streams(dev->stock, &stockConfig);
    if (ret != 0) {
        return ret;
    }

    for (uint32_t i = 0; i < config->num_streams; i++) {
        camera3_stream_t* stream = config->streams[i];
        const camera3_stream_t* shadow = stockStreams[i];

        stream->format = shadow->format;
        stream->usage = shadow->usage;
        stream->max_buffers = shadow->max_buffers;
        stream->data_space = shadow->data_space;
        stream->priv = shadow->priv;

        if (stream->stream_type == CAMERA3_STREAM_OUTPUT) {
            const uint32_t usage = requestedUsages[i] | stream->usage;
            overrideFormat(stream, usage);
            overrideDataSpace(stream, usage);
        }
    }

    return ret;
}

int registerStreamBuffers(const camera3_device_t* device,
                          const camera3_stream_buffer_set_t* buffer_set) {
    CameraDevice* dev = getDevice(device);

    camera3_stream_buffer_set_t stockSet = *buffer_set;
    stockSet.stream = dev->toStock(buffer_set->stream);

    return dev->stock->ops->register_stream_buffers(dev->stock, &stockSet);
}

const camera_metadata_t* constructDefaultRequestSettings(const camera3_device_t* device,
                                                         int type) {
    CameraDevice* dev = getDevice(device);
    return dev->stock->ops->construct_default_request_settings(dev->stock, type);
}

int processCaptureRequest(const camera3_device_t* device, camera3_capture_request_t* request) {
    CameraDevice* dev = getDevice(device);

    camera3_capture_request_t stockRequest = *request;
    std::vector<camera3_stream_buffer_t> outputBuffers(
            request->output_buffers, request->output_buffers + request->num_output_buffers);
    for (auto& buffer : outputBuffers) {
        buffer.stream = dev->toStock(buffer.stream);
    }
    stockRequest.output_buffers = outputBuffers.data();

    camera3_stream_buffer_t inputBuffer;
    if (request->input_buffer != nullptr) {
        inputBuffer = *request->input_buffer;
        inputBuffer.stream = dev->toStock(inputBuffer.stream);
        stockRequest.input_buffer = &inputBuffer;
    }

    return dev->stock->ops->process_capture_request(dev->stock, &stockRequest);
}

void getMetadataVendorTagOps(const camera3_device_t* device, vendor_tag_query_ops_t* ops) {
    CameraDevice* dev = getDevice(device);
    dev->stock->ops->get_metadata_vendor_tag_ops(dev->stock, ops);
}

void dump(const camera3_device_t* device, int fd) {
    CameraDevice* dev = getDevice(device);
    dev->stock->ops->dump(dev->stock, fd);
}

int flush(const camera3_device_t* device) {
    CameraDevice* dev = getDevice(device);
    return dev->stock->ops->flush(dev->stock);
}

void signalStreamFlush(const camera3_device_t* device, uint32_t num_streams,
                       const camera3_stream_t* const* streams) {
    CameraDevice* dev = getDevice(device);

    std::vector<const camera3_stream_t*> stockStreams(num_streams);
    for (uint32_t i = 0; i < num_streams; i++) {
        stockStreams[i] = dev->toStock(const_cast<camera3_stream_t*>(streams[i]));
    }

    dev->stock->ops->signal_stream_flush(dev->stock, num_streams, stockStreams.data());
}

int isReconfigurationRequired(const camera3_device_t* device,
                              const camera_metadata_t* old_session_params,
                              const camera_metadata_t* new_session_params) {
    CameraDevice* dev = getDevice(device);
    return dev->stock->ops->is_reconfiguration_required(dev->stock, old_session_params,
                                                         new_session_params);
}

int closeDevice(hw_device_t* device) {
    CameraDevice* dev = getDevice(reinterpret_cast<camera3_device_t*>(device));

    int ret = dev->stock->common.close(&dev->stock->common);
    delete dev;

    return ret;
}

/*
 * Module
 */

int openDevice(const hw_module_t* module, const char* id, hw_device_t** device) {
    if (gStockModule == nullptr) {
        return -ENODEV;
    }

    hw_device_t* stockDevice = nullptr;
    int ret = gStockModule->common.methods->open(&gStockModule->common, id, &stockDevice);
    if (ret != 0) {
        return ret;
    }

    auto* stock = reinterpret_cast<camera3_device_t*>(stockDevice);
    auto* dev = new CameraDevice();
    dev->stock = stock;

    // Only offer the ops the stock HAL implements
    const camera3_device_ops_t* stockOps = stock->ops;
#define WRAP(op, fn) dev->ops.op = stockOps->op ? fn : nullptr
    WRAP(initialize, initialize);
    WRAP(configure_streams, configureStreams);
    WRAP(register_stream_buffers, registerStreamBuffers);
    WRAP(construct_default_request_settings, constructDefaultRequestSettings);
    WRAP(process_capture_request, processCaptureRequest);
    WRAP(get_metadata_vendor_tag_ops, getMetadataVendorTagOps);
    WRAP(dump, dump);
    WRAP(flush, flush);
    WRAP(signal_stream_flush, signalStreamFlush);
    WRAP(is_reconfiguration_required, isReconfigurationRequired);
#undef WRAP

    dev->device.common = stock->common;
    dev->device.common.module = const_cast<hw_module_t*>(module);
    dev->device.common.close = closeDevice;
    dev->device.ops = &dev->ops;
    dev->device.priv = dev;

    *device = &dev->device.common;
    return 0;
}

hw_module_methods_t gModuleMethods = {
        .open = openDevice,
};

}  // namespace

extern "C" {
// Filled in from the stock module when this library is loaded
camera_module_t HAL_MODULE_INFO_SYM __attribute__((visibility("default"))) = {
        .common =
                {
                        .tag = HARDWARE_MODULE_TAG,
                        .id = CAMERA_HARDWARE_MODULE_ID,
                        .name = "Tundra camera wrapper",
                        .author = "The PixelOS Project",
                        .methods = &gModuleMethods,
                },
};
}

__attribute__((constructor)) static void loadStockModule() {
    void* handle = dlopen(STOCK_HAL_PATH, RTLD_NOW);
    if (handle == nullptr) {
        ALOGE("Failed to load %s: %s", STOCK_HAL_PATH, dlerror());
        return;
    }

    gStockModule = static_cast<camera_module_t*>(dlsym(handle, HAL_MODULE_INFO_SYM_AS_STR));
    if (gStockModule == nullptr) {
        ALOGE("Failed to find %s in %s", HAL_MODULE_INFO_SYM_AS_STR, STOCK_HAL_PATH);
        dlclose(handle);
        return;
    }
    gStockModule->common.dso = handle;

    // Everything but open goes straight to the stock module
    hw_module_methods_t* methods = HAL_MODULE_INFO_SYM.common.methods;
    const char* name = HAL_MODULE_INFO_SYM.common.name;
    const char* author = HAL_MODULE_INFO_SYM.common.author;
    HAL_MODULE_INFO_SYM = *gStockModule;
    HAL_MODULE_INFO_SYM.common.methods = methods;
    HAL_MODULE_INFO_SYM.common.name = name;
    HAL_MODULE_INFO_SYM.common.author = author;
    HAL_MODULE_INFO_SYM.common.dso = nullptr;
}
