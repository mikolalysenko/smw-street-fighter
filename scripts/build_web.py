#!/usr/bin/env python3
"""Build the original SMW crossover as a self-contained local web app."""
from pathlib import Path
import os
import json
import shutil
import subprocess
from datetime import datetime, timezone
from prepare_smw import prepare

ROOT=Path(__file__).resolve().parents[1]

def main():
    emcc=shutil.which(os.environ.get('EMCC','emcc'))
    if not emcc:raise SystemExit('Install/activate Emscripten, then run make web.')
    for name in ('smw_assets.dat','sf2/sprites.dat','sf2/audio/boss_ko.wav'):
        if not (ROOT/'local'/name).exists():raise SystemExit('Run make original-assets first: missing '+name)
    prepare()
    tree=ROOT/'build/smw';out=ROOT/'dist/web';out.mkdir(parents=True,exist_ok=True)
    excluded={'main.c','config.c','opengl.c','glsl_shader.c'}
    sources=[p for folder in ('src','src/snes','smb1','smbll') for p in sorted((tree/folder).glob('*.c'))
             if p.name not in excluded and not p.name.startswith('main-')]
    exports=['ww_web_init','ww_web_step','ww_web_pixels','ww_web_audio',
             'ww_web_sram','ww_web_sram_size','ww_web_state','ww_web_set_debug']
    cmd=[emcc,'-O2','-g1','-fno-strict-aliasing','-I'+str(tree),'-I'+str(tree/'src'),
         '-sUSE_SDL=2','-sALLOW_MEMORY_GROWTH=1','-sINITIAL_MEMORY=33554432','-sSTACK_SIZE=1048576',
         '-sMODULARIZE=1','-sEXPORT_ES6=1','-sEXPORT_NAME=createWorldWarrior',
         '-sENVIRONMENT=web,node','-sFORCE_FILESYSTEM=1','-sEXPORTED_RUNTIME_METHODS=HEAPU8,HEAPF32',
         '-sEXPORTED_FUNCTIONS='+','.join('_'+f for f in exports),
         '--no-entry','--preload-file',str(ROOT/'local/smw_assets.dat')+'@/smw_assets.dat',
         '--preload-file',str(ROOT/'local/sf2')+'@/sf2',
         *map(str,sources),str(ROOT/'adapters/web/web_main.c'),'-lm','-o',str(out/'game.mjs')]
    subprocess.run(cmd,cwd=ROOT,check=True)
    for p in (ROOT/'web').iterdir():
        if p.is_file():shutil.copy2(p,out/p.name)
    build=datetime.now(timezone.utc).strftime('%Y%m%d-%H%M%S')
    # Version every module and the asset package together; a reload must
    # never combine an old WASM module with a new sprite/audio bundle.
    for name in ('app.mjs','index.html','touch.mjs'):
        p=out/name;s=p.read_text().replace('__BUILD__',build)
        for module in ('game.mjs','app.mjs','input.mjs','presenter.mjs','touch.mjs'):
            s=s.replace(module, module+'?v='+build)
        p.write_text(s)
    revision=subprocess.check_output(['git','rev-parse','HEAD'],cwd=ROOT,text=True).strip()
    dirty=bool(subprocess.check_output(['git','status','--porcelain'],cwd=ROOT,text=True).strip())
    (out/'build.json').write_text(json.dumps({
        'build':build,'source_revision':revision,'source_dirty':dirty,
    },indent=2)+'\n')
    (out/'.nojekyll').touch()
    shutil.copy2(ROOT/'LICENSE',out/'LICENSE.txt')
    shutil.copy2(ROOT/'references/smw/LICENSE.txt',out/'SMW-LICENSE.txt')
    shutil.copy2(ROOT/'docs/ASSETS.md',out/'ASSETS.md')
    (out/'README.txt').write_text(
        'Super Mario World II - The World Warrior\n\n'
        'Run in this folder: python3 -m http.server 8765 --bind 127.0.0.1\n'
        'Open http://localhost:8765, click Play, then Fullscreen.\n'
        'Keep the local server running while playing. Do not open index.html directly.\n'
        'Keyboard and gamepad controls are listed below the game.\n'
        'Original SMW progress and the high score persist per browser; Export Save backs up both.\n'
        'Build '+build+'\n')
    shutil.make_archive(str(ROOT/'dist/super-mario-world-ii-web'),'zip',root_dir=out.parent,base_dir=out.name)
    print('Built dist/web. Start with: make serve-web')

if __name__=='__main__':main()
