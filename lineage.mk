# Copyright (C) 2016 The CyanogenMod Project
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#      http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

# Inherit from those products. Most specific first.
$(call inherit-product, $(SRC_TARGET_DIR)/product/full_base_telephony.mk)

# Inherit from u3 device
$(call inherit-product, device/iuni/u3/u3.mk)

# Inherit some common CM stuff.
$(call inherit-product, vendor/cm/config/common_full_phone.mk)

PRODUCT_NAME := lineage_u3
PRODUCT_DEVICE := u3
PRODUCT_MANUFACTURER := IUNI
PRODUCT_MODEL := U3

PRODUCT_GMS_CLIENTID_BASE := android-iuni

PRODUCT_BRAND := iuni
TARGET_VENDOR := iuni
TARGET_VENDOR_PRODUCT_NAME := u3
TARGET_VENDOR_DEVICE_NAME := NBL8910A
PRODUCT_BUILD_PROP_OVERRIDES += TARGET_DEVICE=U3 PRODUCT_NAME=u3
