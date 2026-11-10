/*
 * SPDX-FileCopyrightText: 2026 The PixelOS Project
 * SPDX-License-Identifier: Apache-2.0
 */

/*
 * Wrapper around the stock snapdragoncolor manager (STC), loaded by SDM in
 * the display composer.
 *
 * The panel calibration (qdcm_calib_data_*.xml) has a single HDR mode,
 * hdr-6500, in the DCI-P3 gamut. HWC files every STC mode under the Android
 * color mode that matches its gamut and gamma, so hdr-6500 is only reachable
 * from DISPLAY_P3. In the unmanaged (Saturated) color setting SurfaceFlinger
 * stays in NATIVE, HWC finds no HDR mode there and shows PQ/HLG video through
 * the SDR native mode, which looks orange.
 *
 * For every HDR mode, report a copy tagged with gamma 2.2 instead of sRGB.
 * HWC does not map DCI-P3 + gamma 2.2 to an Android color mode, so the copy
 * becomes the HDR entry of NATIVE, while the blend space SDM derives from it
 * stays DCI-P3. When SDM applies the copy, give the stock library the
 * original mode back.
 */

#define LOG_TAG "SnapdragonColorWrapper"

#include <dlfcn.h>

#include <algorithm>
#include <string>
#include <vector>

#include <log/log.h>
#include <private/snapdragon_color_intf.h>

using namespace snapdragoncolor;

namespace {

constexpr char kStockLibrary[] = "libsnapdragoncolor-manager-stock.so";
constexpr GammaTransfer kNativeHdrGamma = Transfer_Gamma2_2;

using GetInterfaceFn = ScPostBlendInterface *(*)(uint32_t, uint32_t);

bool IsHdrMode(const ColorMode &mode) {
    return std::find(mode.hw_assets.begin(), mode.hw_assets.end(), kPbHdrBlob) !=
           mode.hw_assets.end();
}

bool SameMode(const ColorMode &a, const ColorMode &b) {
    return a.gamut == b.gamut && a.gamma == b.gamma && a.intent == b.intent &&
           a.intent_name == b.intent_name && a.hw_assets == b.hw_assets;
}

class ScPostBlendWrapper : public ScPostBlendInterface {
  public:
    explicit ScPostBlendWrapper(ScPostBlendInterface *stock) : stock_(stock) {}
    ~ScPostBlendWrapper() override { delete stock_; }

    int Init(const std::string &panel_name) override { return stock_->Init(panel_name); }
    int DeInit() override { return stock_->DeInit(); }
    int SetProperty(const ScPayload &payload) override { return stock_->SetProperty(payload); }

    int GetProperty(ScPayload *payload) override {
        int ret = stock_->GetProperty(payload);
        if (ret == 0 && payload && payload->prop == kModeList && payload->payload) {
            AddNativeHdrModes(reinterpret_cast<ColorModeList *>(payload->payload));
        }
        return ret;
    }

    int ProcessOps(ScOps op, const ScPayload &input, ScPayload *output) override {
        if (input.prop != kModeRenderInputParams || !input.payload) {
            return stock_->ProcessOps(op, input, output);
        }

        auto *params = reinterpret_cast<ModeRenderInputParams *>(input.payload);
        const ColorMode *original = FindOriginal(params->color_mode);
        if (!original) {
            return stock_->ProcessOps(op, input, output);
        }

        ModeRenderInputParams stock_params(*params);
        stock_params.color_mode = *original;
        ScPayload stock_input = input;
        stock_input.payload = reinterpret_cast<uint64_t>(&stock_params);
        return stock_->ProcessOps(op, stock_input, output);
    }

  private:
    struct ModePair {
        ColorMode copy;
        ColorMode original;
    };

    void AddNativeHdrModes(ColorModeList *mode_list) {
        auto &modes = mode_list->list;
        std::vector<ColorMode> added;
        pairs_.clear();

        for (const auto &mode : modes) {
            if (!IsHdrMode(mode) || mode.gamma != Transfer_sRGB || mode.intent == kNative) {
                continue;
            }

            ColorMode copy = mode;
            copy.gamma = kNativeHdrGamma;
            bool exists = std::any_of(modes.begin(), modes.end(), [&](const ColorMode &m) {
                return m.gamut == copy.gamut && m.gamma == copy.gamma && m.intent == copy.intent;
            });
            if (exists) {
                continue;
            }

            pairs_.push_back({copy, mode});
            added.push_back(copy);
            ALOGI("Adding HDR mode (gamut %d intent %d %s) for native color mode", mode.gamut,
                  mode.intent, mode.intent_name.c_str());
        }

        modes.insert(modes.end(), added.begin(), added.end());
    }

    const ColorMode *FindOriginal(const ColorMode &mode) const {
        for (const auto &pair : pairs_) {
            if (SameMode(pair.copy, mode)) {
                return &pair.original;
            }
        }
        return nullptr;
    }

    ScPostBlendInterface *stock_;
    std::vector<ModePair> pairs_;
};

}  // namespace

extern "C" ScPostBlendInterface *GetScPostBlendInterface(uint32_t major_version,
                                                         uint32_t minor_version) {
    static GetInterfaceFn get_interface = [] {
        void *handle = dlopen(kStockLibrary, RTLD_NOW);
        if (!handle) {
            ALOGE("Failed to load %s: %s", kStockLibrary, dlerror());
            return static_cast<GetInterfaceFn>(nullptr);
        }
        auto fn = reinterpret_cast<GetInterfaceFn>(dlsym(handle, "GetScPostBlendInterface"));
        if (!fn) {
            ALOGE("Failed to find GetScPostBlendInterface: %s", dlerror());
        }
        return fn;
    }();

    if (!get_interface) {
        return nullptr;
    }

    ScPostBlendInterface *stock = get_interface(major_version, minor_version);
    if (!stock) {
        return nullptr;
    }

    return new ScPostBlendWrapper(stock);
}
