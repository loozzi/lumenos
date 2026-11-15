#!/usr/bin/env python3
"""中文：LumenOS 正式发布渠道的唯一配置：仓库、OTA/网页来源与版本格式。
English: Single source of truth for the LumenOS official release channel: repository,
OTA/web origins and version formats.

用户2026-11-15决定把正式发布渠道从 dephia31/dpReader31 移到 loozzi/dpReader31。
新正式版使用 LumenOS 版本号（Full `X.Y.Z`，Love Book `X.Y.Z-love`）；旧 dpReader
`-vn` 版本只作为已归档的历史版本被接受。
On 2026-11-15 the user moved official publication from dephia31/dpReader31 to
loozzi/dpReader31. New releases use LumenOS versions (Full `X.Y.Z`, Love Book
`X.Y.Z-love`); legacy dpReader `-vn` versions are accepted only as archived history.
"""
import os
import re

REPO = os.environ.get('LUMENOS_RELEASE_REPO') or 'loozzi/dpReader31'
# raw.githubusercontent 不重定向，Pages 未启用时也可用；Pages 为网页刷机与备用 OTA。
# raw.githubusercontent does not redirect and works before Pages is enabled; Pages hosts
# the web flasher and the fallback OTA origin.
RAW_ORIGIN = f'https://raw.githubusercontent.com/{REPO}/main/'
PAGES_ORIGIN = 'https://{}.github.io/{}/'.format(*REPO.split('/'))
RELEASE_NAME = 'LumenOS Metalio 4.7 Firmware'

FULL_VERSION = r'\d+\.\d+\.\d+'
LOVE_VERSION = r'\d+\.\d+\.\d+-love'
LEGACY_FULL_VERSION = r'\d+\.\d+\.\d+-vn'
LEGACY_LOVE_VERSION = r'\d+\.\d+\.\d+-love-vn'


def official(version, edition):
    """新正式版版本号。/ A new official LumenOS version."""
    return bool(re.fullmatch(LOVE_VERSION if edition == 'love-book' else FULL_VERSION, version))


def archived(version, edition):
    """可作为历史版本的版本号（含旧 dpReader）。/ A version acceptable as archived history."""
    legacy = LEGACY_LOVE_VERSION if edition == 'love-book' else LEGACY_FULL_VERSION
    return official(version, edition) or bool(re.fullmatch(legacy, version))


def base(version):
    """去掉版本后缀得到 X.Y.Z。/ Strip edition and legacy suffixes to X.Y.Z."""
    return re.match(r'\d+\.\d+\.\d+', version).group(0)


def love_version(full_version):
    return base(full_version) + '-love'


def tag(version):
    """旧版按原 dpReader 标签，新版为 vX.Y.Z。/ Legacy releases keep dpReader tags; new ones use vX.Y.Z."""
    if version.endswith('-vn'):
        return 'v' + version.replace('-love-vn', '-vn')
    return 'v' + base(version)


def image_url(relative):
    """OTA 清单中的应用下载地址。/ Application URL written into OTA manifests."""
    return RAW_ORIGIN + relative.strip('/') + '/dpReader_metalio47_firmware.bin'
