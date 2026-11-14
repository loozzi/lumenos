#!/usr/bin/env python3
"""Run production parser on both existing feeds and the next local edition candidates."""
import json
from pathlib import Path
import subprocess
import tempfile
ROOT=Path(__file__).resolve().parents[1]
with tempfile.TemporaryDirectory() as folder:
    work=Path(folder)
    unit=r'''
#include "ota_online.h"
#include "firmware_edition.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
int main(int argc,char **argv){
 assert(argc==5);
 for(int i=1;i<argc;i++){
  FILE *f=fopen(argv[i],"rb");assert(f);char text[16384];size_t n=fread(text,1,sizeof(text),f);fclose(f);
  pico_release_t r;assert(pico_release_parse(text,n,&r)==ESP_OK);
  assert(pico_release_url_valid(r.url));assert(r.size>0);assert(strlen(r.sha256)==64);
  assert(firmware_love_edition(r.version,sizeof(r.version))==(i%2==0));
  if(i>2)assert(pico_version_compare(r.version,i==3?"1.4.5-vn":"1.4.5-love-vn")>0);
 }
 puts("PASS actual Full/Love manifests and LumenOS candidates: production schema, HTTPS, SHA, edition and upgrade order");
}
'''
    (work/'test.c').write_text(unit)
    files=[]
    for candidate in (False,True):
        for edition,src,version in [('full',ROOT/'metalio-s31-update.json','1.5.0'),('love',ROOT/'releases/love-book/metalio-s31-update.json','1.5.0-love')]:
            data=json.loads(src.read_text())
            if candidate:data['version']=version
            dest=work/f'{edition}-{candidate}.json';dest.write_text(json.dumps(data));files.append(str(dest))
    subprocess.run(['cc','-std=c11','-Wall','-Wextra','-Werror','-fsanitize=undefined','-DDPREADER_BOARD_METALIO47=1','-I'+str(ROOT/'tools/ota_stubs'),'-I'+str(ROOT/'main'),'-I/usr/include/cjson',str(work/'test.c'),str(ROOT/'main/ota_release.c'),'-lcjson','-o',str(work/'test')],check=True)
    subprocess.run([str(work/'test'),*files],check=True)
