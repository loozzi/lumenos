#!/usr/bin/env python3
"""Verify the Full-only release rule after the Love Book freeze at 1.5.18-love.

中文：用真实发布工具函数验证冻结判断、Love Book 校验、Release 附件选择，
以及 releases/love-book/ 与冻结标签逐字节一致的检查。
English: exercise the real release helpers: freeze predicate, Love Book validation,
Release asset selection and the byte-identical releases/love-book/ check.
"""
from pathlib import Path
import subprocess
import sys
import tempfile

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'tools'))
import release_channel as channel  # noqa: E402
import verify_metalio_release as verifier  # noqa: E402
import publish_release  # noqa: E402


def check_predicate():
    assert channel.LOVE_FROZEN_VERSION == '1.5.18-love'
    for version, frozen in (('1.5.17', False), ('1.5.18', False), ('1.5.19', True), ('1.6.0', True),
                            ('2.0.0', True), ('1.5.100', True), ('1.4.5-vn', False)):
        assert channel.love_frozen(version) is frozen, version


def check_verify_all():
    real = verifier.verify
    try:
        def fake(full, love):
            def verify(folder=None, edition='full'):
                return dict(version=full if edition == 'full' else love)
            return verify
        verifier.verify = fake('1.5.19', '1.5.18-love')
        full, love = verifier.verify_all()
        assert full['version'] == '1.5.19' and love['version'] == '1.5.18-love'
        for bad in ('1.5.19-love', '1.5.17-love'):
            verifier.verify = fake('1.5.19', bad)
            try:
                verifier.verify_all()
            except AssertionError:
                pass
            else:
                raise AssertionError('frozen Love Book must stay ' + channel.LOVE_FROZEN_VERSION + ', got ' + bad)
        # Before the freeze both editions still share the base version.
        verifier.verify = fake('1.5.18', '1.5.18-love')
        verifier.verify_all()
        verifier.verify = fake('1.5.18', '1.5.17-love')
        try:
            verifier.verify_all()
        except AssertionError:
            pass
        else:
            raise AssertionError('pre-freeze releases must keep matching editions')
    finally:
        verifier.verify = real


def check_assets():
    parts = {'bootloader.bin': {}, 'partitions.bin': {}, 'storage-empty.bin': {}}
    love = dict(version='1.5.18-love', usb_parts=parts)
    after = publish_release.release_assets(dict(version='1.5.19', usb_parts=parts), love, ROOT)
    names = [n for n, _ in after]
    assert len(names) == 8 and not any(n.startswith('love-book-') or '-love' in n for n in names), names
    assert 'dpReader-metalio47-1.5.19-USB.zip' in names
    assert all('love-book' not in str(path) for _, path in after)
    before = publish_release.release_assets(dict(version='1.5.18', usb_parts=parts),
                                            dict(version='1.5.18-love', usb_parts=parts), ROOT)
    assert len(before) == 16 and sum(n.startswith('love-book-') for n, _ in before) == 7


def check_untouched():
    assert channel.love_frozen_untouched(ROOT), 'releases/love-book must match ' + channel.tag(channel.LOVE_FROZEN_VERSION)
    with tempfile.TemporaryDirectory() as d:
        clone = Path(d) / 'repo'
        subprocess.run(['git', 'clone', '-q', '--shared', '--no-checkout', str(ROOT), str(clone)], check=True)
        subprocess.run(['git', 'checkout', '-q', channel.tag(channel.LOVE_FROZEN_VERSION), '--', 'releases/love-book'],
                       cwd=clone, check=True)
        subprocess.run(['git', 'reset', '-q'], cwd=clone, check=True)
        assert channel.love_frozen_untouched(clone)
        feed = clone / 'releases/love-book/metalio-s31-update.json'
        feed.write_text(feed.read_text() + ' ')
        assert not channel.love_frozen_untouched(clone), 'a modified Love Book feed must be rejected'
        subprocess.run(['git', 'checkout', '-q', '--', 'releases/love-book'], cwd=clone, check=True)
        (clone / 'releases/love-book/extra.bin').write_bytes(b'x')
        assert not channel.love_frozen_untouched(clone), 'a new Love Book file must be rejected'


if __name__ == '__main__':
    check_predicate()
    check_verify_all()
    check_assets()
    check_untouched()
    print('PASS Full-only releases after 1.5.18: freeze predicate, frozen Love Book validation, Full-only assets, '
          'byte-identical releases/love-book check; pre-freeze dual releases unchanged')
