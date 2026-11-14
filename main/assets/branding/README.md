# LumenOS logo assets

`lumen-logo-source.png` is a byte-identical copy of the user's
`Claude_AI_symbol.black.png`: black symbol on a transparent PNG canvas.
The supplied shape is retained without redrawing or image generation.

`../startup_logo_4bpp.bin` combines this mark with the **LumenOS** wordmark,
rasterized using the existing embedded font. The generator composites transparency
onto white and encodes a centered 4bpp image for the firmware. Runtime drawing
reads packed pixels directly from flash without a logo framebuffer.

```sh
python3 tools/gen_startup_logo.py
```

The generated header records dimensions and the source/font SHA-256 hashes.
Do not hand-edit the bitmap or its header. `--wordmark ''` encodes only the mark.

`web/assets/lumen-logo.png` retains the same supplied PNG bytes. Web headers place
**LumenOS** beside it; CSS inverts the black mark on the existing dark background.
The local transfer portal embeds the PNG so it needs no Internet request:

```sh
python3 tools/sync_brand_logo.py
```

The older `dpreader-logo-*` assets remain for historical references. Existing
release binaries and download URLs retain their original contents and identities;
changing website branding does not create or publish a LumenOS firmware release.
