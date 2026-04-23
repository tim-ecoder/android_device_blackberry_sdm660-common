/*
 * Copyright (c) 2020, The Linux Foundation. All rights reserved.
 * Not a Contribution
 */
/*
 * Copyright (C) 2016 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#define LOG_TAG "GnssHal_GnssXtra"

#include "GnssXtra.h"
#include <log_util.h>

namespace android {
namespace hardware {
namespace gnss {
namespace V2_1 {
namespace implementation {

Return<bool> GnssXtra::setCallback(const sp<IGnssXtraCallback>& /*callback*/) {
    // Stub: accept callback to satisfy framework initialization.
    // Actual XTRA download is handled by xtra-daemon.
    LOC_LOGD("%s]: GnssXtra callback set (stub)", __func__);
    return true;
}

Return<bool> GnssXtra::injectXtraData(const hidl_string& /*xtraData*/) {
    // Stub: framework may call this after downloading PSDS data.
    // We accept it but actual XTRA injection is handled by xtra-daemon
    // which downloads and injects directly via QMI.
    LOC_LOGD("%s]: injectXtraData called (stub, %s)",
             __func__, "xtra-daemon handles actual injection");
    return true;
}

}  // namespace implementation
}  // namespace V2_1
}  // namespace gnss
}  // namespace hardware
}  // namespace android
