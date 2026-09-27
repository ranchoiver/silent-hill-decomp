"""Compile the real rear-look input function; test aim transitions and rebinding.

Linux/WSL: python3 pc_port/tests/test_rear_look.py [--source-ref <commit>]
Requires GCC, pkg-config and SDL2 headers. Input devices are simulated here;
rendered camera/shot validation requires the game and disc assets.
"""
import argparse
from pathlib import Path
import shlex
import subprocess
import tempfile

repo = Path(__file__).resolve().parents[2]
parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--source-ref')
args = parser.parse_args()
path = 'pc_port/src/pc_combat.c'
source = (subprocess.check_output(['git','show',f'{args.source_ref}:{path}'],cwd=repo,text=True)
          if args.source_ref else (repo/path).read_text())
start = source.index('void Pc_RearLookUpdate(void)')
end = source.index('/* OTS/TPS free-aim aim assist.',start)
fixture = (Path(__file__).with_name('rear_look_fixture.c')).read_text()
generated = fixture.replace('/* FUNCTION_UNDER_TEST */',source[start:end])
flags = ['-std=gnu11','-DSH_PC_PORT','-DVER_USA','-DSKIP_ASM','-Dstatic_assert=_Static_assert',
         '-fsanitize=address,undefined','-fno-sanitize-recover=all','-g']
flags += ['-I'+str(repo/p) for p in ('pc_port/include','pc_port/include/psyq_compat',
          'pc_port/src','include','include/decomp','pc_port/PsyCross/include/psx','pc_port/PsyCross/include')]
flags += shlex.split(subprocess.check_output(['pkg-config','--cflags','sdl2'],text=True))
with tempfile.TemporaryDirectory(prefix='sh-rear-look-') as tmp:
    out = Path(tmp)
    (out/'test.c').write_text(generated)
    subprocess.run(['gcc',*flags,str(out/'test.c'),'-lSDL2','-o',str(out/'test')],check=True)
    subprocess.run([str(out/'test')],check=True)
