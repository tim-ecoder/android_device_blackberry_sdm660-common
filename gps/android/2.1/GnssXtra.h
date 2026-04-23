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

#ifndef ANDROID_HARDWARE_GNSS_V2_1_GNSSXTRA_H
#define ANDROID_HARDWARE_GNSS_V2_1_GNSSXTRA_H

#include <android/hardware/gnss/1.0/IGnssXtra.h>
#include <hidl/Status.h>

namespace android {
namespace hardware {
namespace gnss {
namespace V2_1 {
namespace implementation {

using ::android::hardware::gnss::V1_0::IGnssXtra;
using ::android::hardware::gnss::V1_0::IGnssXtraCallback;
using ::android::hardware::Return;
using ::android::hardware::Void;
using ::android::sp;

/*
 * Stub IGnssXtra implementation.
 * Exists to make the Android framework initialize the PSDS/connectivity pipeline.
 * Actual XTRA download and injection is handled by xtra-daemon.
 */
struct GnssXtra : public IGnssXtra {
    Return<bool> setCallback(const sp<IGnssXtraCallback>& callback) override;
    Return<bool> injectXtraData(const hidl_string& xtraData) override;
};

}  // namespace implementation
}  // namespace V2_1
}  // namespace gnss
}  // namespace hardware
}  // namespace android

#endif  // ANDROID_HARDWARE_GNSS_V2_1_GNSSXTRA_H
