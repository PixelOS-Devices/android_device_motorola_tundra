#!/usr/bin/env -S PYTHONPATH=../../../../tools/extract-utils python3
#
# SPDX-FileCopyrightText: The LineageOS Project
# SPDX-License-Identifier: Apache-2.0
#

import os
import tempfile
import zipfile

from extract_utils.extract import extract_fns_user_type
from extract_utils.extract_star import extract_star_firmware
from extract_utils.fixups_blob import (
    blob_fixup,
    blob_fixups_user_type,
)
from extract_utils.fixups_lib import (
    lib_fixups,
    lib_fixups_user_type,
)
from extract_utils.main import (
    ExtractUtils,
    ExtractUtilsModule,
)
from extract_utils.utils import run_cmd

class TundraExtractUtilsModule(ExtractUtilsModule):
    def proprietary_file_path(self, file_name: str) -> str:
        return os.path.join(
            self.device_path,
            'proprietary-setup',
            file_name,
        )

namespace_imports = [
    'device/motorola/tundra',
    'hardware/motorola',
    'hardware/qcom-caf/sm8350',
    'hardware/qcom-caf/wlan',
    'vendor/qcom/opensource/commonsys-intf/display',
    'vendor/qcom/opensource/commonsys/display',
    'vendor/qcom/opensource/dataservices',
    'vendor/qcom/opensource/display',
]

def lib_fixup_vendor_suffix(lib: str, partition: str, *args, **kwargs):
    return f'{lib}_{partition}' if partition == 'vendor' else None

lib_fixups: lib_fixups_user_type = {
    **lib_fixups,
    (
        'com.qualcomm.qti.dpm.api@1.0',
        'libmmosal',
        'vendor.qti.imsrtpservice@3.0',
    ): lib_fixup_vendor_suffix,
}

# The Dolby codec2 service is built against the Android 11 vendor (VNDK v30),
# so it uses private stock copies of these libraries, renamed to *-stock.
dolby_c2_stock_libs = (
    'libcodec2_hidl@1.0',
    'libcodec2_hidl@1.1',
    'libcodec2_soft_common',
    'libcodec2_vndk',
    'libdapparamstorage',
    'libdeccfg',
    'libsfplugin_ccodec_utils',
    'libstagefright_bufferpool@2.0.1',
    'libstagefright_bufferqueue_helper',
    'vendor.dolby.hardware.dms@2.0',
)

def dolby_c2_stock_fixup() -> blob_fixup:
    fixup = blob_fixup()
    for lib in dolby_c2_stock_libs:
        fixup = fixup.replace_needed(f'{lib}.so', f'{lib}-stock.so')
    return fixup

blob_fixups: blob_fixups_user_type = {
    (
        'vendor/bin/hw/vendor.dolby.media.c2@1.0-service',
        'vendor/lib64/libcodec2_soft_ac4dec.so',
        'vendor/lib64/libcodec2_soft_ddpdec.so',
        'vendor/lib64/libcodec2_store_dolby.so',
    ): dolby_c2_stock_fixup(),
    (
        'vendor/lib64/libcodec2_hidl@1.0-stock.so',
        'vendor/lib64/libcodec2_hidl@1.1-stock.so',
    ): dolby_c2_stock_fixup()
        .fix_soname()
        .add_needed('libbase_shim.so'),
    'vendor/lib64/libcodec2_vndk-stock.so': dolby_c2_stock_fixup()
        .fix_soname()
        .add_needed('libui_shim.so'),
    (
        'vendor/lib64/libcodec2_soft_common-stock.so',
        'vendor/lib64/libdapparamstorage-stock.so',
        'vendor/lib64/libdeccfg-stock.so',
        'vendor/lib64/libsfplugin_ccodec_utils-stock.so',
        'vendor/lib64/libstagefright_bufferpool@2.0.1-stock.so',
        'vendor/lib64/libstagefright_bufferqueue_helper-stock.so',
        'vendor/lib64/vendor.dolby.hardware.dms@2.0-stock.so',
    ): dolby_c2_stock_fixup()
        .fix_soname(),
    'vendor/lib64/libmot_chi_desktop_helper.so': blob_fixup()
        .add_needed('libgui_buffer_shim_vendor.so'),
    'vendor/lib64/sensors.moto.so': blob_fixup()
        .add_needed('libbase_shim.so'),
    'system_ext/etc/permissions/moto-telephony.xml': blob_fixup()
        .regex_replace('/system/', '/system_ext/'),
    'system_ext/priv-app/ims/ims.apk': blob_fixup()
        .apktool_patch('./proprietary-setup/ims-patches'),
    'vendor/etc/sensors/hals.conf': blob_fixup()
        .add_line_if_missing('sensors.moto_ext.so'),
    'vendor/lib64/libwvhidl.so': blob_fixup()
        .add_needed('libcrypto_shim.so'),
    'vendor/lib64/sensors.moto.so': blob_fixup()
        .add_needed('libbase_shim.so'),
    (
        'vendor/lib64/libdpps.so',
        'vendor/lib64/libsnapdragoncolor-manager.so',
    ): blob_fixup()
        .replace_needed('libtinyxml2.so', 'libtinyxml2-v34.so'),
    # Record camcorder audio in stereo so both mics are captured. The stock
    # profiles are mono, so camera apps record from a single (bottom) mic;
    # the camcorder-mic-bf route then has nothing to feed its second channel.
    # Only the AAC video profiles are touched, not the AMR-NB (MMS) ones.
    (
        'vendor/etc/media_profiles_V1_0.xml',
        'vendor/etc/media_profiles_lahaina.xml',
        'vendor/etc/media_profiles_vendor.xml',
    ): blob_fixup()
        .regex_replace(r'(codec="aac"[^>]*?)channels="1"', r'\1channels="2"'),
}  # fmt: skip

# extract-utils only extracts whole APEX files, pull the VNDK v30 libraries
# the Dolby codec2 service needs out of the APEX payload.
vndk_v30_libs = ('libstagefright_bufferqueue_helper.so',)

def extract_vndk_v30_apex(ctx, file_path: str, work_dir: str):
    out_dir = os.path.join(
        os.path.dirname(file_path), 'com.android.vndk.v30', 'lib64'
    )
    os.makedirs(out_dir, exist_ok=True)

    with tempfile.TemporaryDirectory() as tmp_dir:
        with zipfile.ZipFile(file_path) as apex:
            payload_path = apex.extract('apex_payload.img', tmp_dir)

        for lib in vndk_v30_libs:
            lib_path = os.path.join(out_dir, lib)
            run_cmd(['debugfs', '-R', f'dump /lib64/{lib} {lib_path}', payload_path])

    # Keep the APEX itself
    return None

extract_fns: extract_fns_user_type = {
    r'(bootloader|radio)\.img': extract_star_firmware,
    r'com\.android\.vndk\.v30\.apex': extract_vndk_v30_apex,
}

module = TundraExtractUtilsModule(
    'tundra',
    'motorola',
    blob_fixups=blob_fixups,
    lib_fixups=lib_fixups,
    namespace_imports=namespace_imports,
    extract_fns=extract_fns,
    add_firmware_proprietary_file=True,
    add_generated_carriersettings=True,
)

if __name__ == '__main__':
    utils = ExtractUtils.device(module)
    utils.run()
