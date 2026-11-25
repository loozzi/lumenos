#!/usr/bin/env python3
"""中文：LumenOS 正式发布渠道的唯一配置：仓库、OTA/网页来源与版本格式。
English: Single source of truth for the LumenOS official release channel: repository,
OTA/web origins and version formats.

用户2026-11-15决定把正式发布渠道从 dephia31/dpReader31 移到 loozzi/lumenos。
新正式版使用 LumenOS 版本号（Full `X.Y.Z`，Love Book `X.Y.Z-love`）；旧 dpReader
`-vn` 版本只作为已归档的历史版本被接受。
On 2026-11-15 the user moved official publication from dephia31/dpReader31 to
loozzi/lumenos. New releases use LumenOS versions (Full `X.Y.Z`, Love Book
`X.Y.Z-love`); legacy dpReader `-vn` versions are accepted only as archived history.
"""
import os
import re

REPO = os.environ.get('LUMENOS_RELEASE_REPO') or 'loozzi/lumenos'
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


# 用户2026-11-25决定：Love Book 停在 1.5.18-love，之后只更新 Full；
# 其 releases/love-book/ 字节保持不变，继续供已安装设备 OTA 与网页刷机。
# User decision 2026-11-25: Love Book stops at 1.5.18-love and only Full is updated
# afterwards; releases/love-book/ stays byte-identical for existing OTA and Web Flash users.
LOVE_FROZEN_VERSION = '1.5.18-love'


def _parts(version):
    return tuple(int(x) for x in base(version).split('.'))


def love_frozen(full_version):
    """该 Full 正式版是否在 Love Book 冻结之后（只发布 Full）。/ Whether this Full release is after the Love Book freeze (Full only)."""
    return (not full_version.endswith('-vn')) and _parts(full_version) > _parts(LOVE_FROZEN_VERSION)


def love_frozen_untouched(root):
    """releases/love-book/ 与冻结标签逐字节一致。/ releases/love-book/ matches the freeze tag byte for byte."""
    import subprocess
    freeze_tag = tag(LOVE_FROZEN_VERSION)
    tracked = subprocess.run(['git', 'diff', '--quiet', freeze_tag, '--', 'releases/love-book'], cwd=root)
    untracked = subprocess.run(['git', 'ls-files', '--others', '--exclude-standard', '--', 'releases/love-book'],
                               cwd=root, capture_output=True, text=True, check=True).stdout.strip()
    if tracked.returncode not in (0, 1):
        raise RuntimeError('Cannot compare releases/love-book with ' + freeze_tag)
    return tracked.returncode == 0 and not untracked


def tag(version):
    """旧版按原 dpReader 标签，新版为 vX.Y.Z。/ Legacy releases keep dpReader tags; new ones use vX.Y.Z."""
    if version.endswith('-vn'):
        return 'v' + version.replace('-love-vn', '-vn')
    return 'v' + base(version)


def image_url(relative):
    """OTA 清单中的应用下载地址。/ Application URL written into OTA manifests."""
    return RAW_ORIGIN + relative.strip('/') + '/dpReader_metalio47_firmware.bin'
