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

# GitHub rejects pushes containing a file over 100MB. Some blobs (camera,
# modem firmware) exceed that, so split them into < 100MB parts after
# extraction and drop a vendorsetup.sh that reassembles them at build time
# (vendorsetup.sh files are auto-sourced by build/envsetup.sh).
GITHUB_MAX_FILE_SIZE = 100 * 1024 * 1024
SPLIT_CHUNK_SIZE = 90 * 1024 * 1024
SPLIT_SUFFIX = '.split.'

VENDORSETUP_SH_CONTENT = '''#!/bin/bash
# Auto-generated by extract-files.py.
# Reassembles proprietary files that were split to stay under GitHub's
# 100MB file size limit. Sourced automatically by build/envsetup.sh.

_tundra_reassemble_split_files() {
    local vendor_dir
    vendor_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
    local part target
    while IFS= read -r part; do
        target="${part%.split.001}"
        if [ ! -f "$target" ] || [ "$part" -nt "$target" ]; then
            echo "tundra vendorsetup: reassembling $(basename "$target")"
            cat "$target".split.* > "$target"
        fi
    done < <(find "$vendor_dir" -name .git -prune -o -name '*.split.001' -print 2>/dev/null)
}
_tundra_reassemble_split_files
'''


def get_vendor_path() -> str:
    script_dir = os.path.dirname(os.path.abspath(__file__))
    device_path = os.path.dirname(script_dir)
    root_path = os.path.normpath(os.path.join(device_path, '..', '..', '..'))
    return os.path.join(root_path, 'vendor', 'motorola', 'tundra')


def walk_vendor_files(vendor_path: str):
    # The vendor tree is a git checkout; never touch its .git (pack files
    # are well over 100MB).
    for root, dirs, file_names in os.walk(vendor_path):
        dirs[:] = [d for d in dirs if d != '.git']
        yield root, file_names


def split_large_files(vendor_path: str):
    for root, file_names in walk_vendor_files(vendor_path):
        for file_name in file_names:
            if SPLIT_SUFFIX in file_name:
                continue

            file_path = os.path.join(root, file_name)
            if os.path.getsize(file_path) <= GITHUB_MAX_FILE_SIZE:
                continue

            print(f'Splitting {file_path} (exceeds GitHub 100MB limit)')
            with open(file_path, 'rb') as src:
                part_num = 1
                while chunk := src.read(SPLIT_CHUNK_SIZE):
                    part_path = f'{file_path}{SPLIT_SUFFIX}{part_num:03d}'
                    with open(part_path, 'wb') as dst:
                        dst.write(chunk)
                    part_num += 1

            os.remove(file_path)


def join_split_files(vendor_path: str):
    first_suffix = f'{SPLIT_SUFFIX}001'
    for root, file_names in walk_vendor_files(vendor_path):
        for file_name in file_names:
            if not file_name.endswith(first_suffix):
                continue

            file_path = os.path.join(root, file_name[: -len(first_suffix)])
            parts = sorted(
                os.path.join(root, f)
                for f in file_names
                if f.startswith(os.path.basename(file_path) + SPLIT_SUFFIX)
            )

            print(f'Joining {file_path} from {len(parts)} parts')
            with open(file_path, 'wb') as dst:
                for part in parts:
                    with open(part, 'rb') as src:
                        dst.write(src.read())

            for part in parts:
                os.remove(part)


def write_vendorsetup_sh(vendor_path: str):
    vendorsetup_path = os.path.join(vendor_path, 'vendorsetup.sh')
    with open(vendorsetup_path, 'w') as f:
        f.write(VENDORSETUP_SH_CONTENT)
    os.chmod(vendorsetup_path, 0o755)


if __name__ == '__main__':
    # Files split by a previous run must be whole again, since
    # extract-utils hashes them while regenerating the makefiles.
    vendor_path = get_vendor_path()
    if os.path.isdir(vendor_path):
        join_split_files(vendor_path)

    utils = ExtractUtils.device(module)
    utils.run()

    if os.path.isdir(vendor_path):
        split_large_files(vendor_path)
        write_vendorsetup_sh(vendor_path)
    else:
        print(f'Skipping file split: {vendor_path} not found')
