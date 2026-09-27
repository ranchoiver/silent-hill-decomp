"""Asset-free regression for #138 (Linux/WSL, GCC, pkg-config and SDL2 headers).

Run: python3 pc_port/tests/test_katana_lunge.py [--sanitize]
Use --source-ref <commit> to demonstrate failure on the pre-fix source.

Compiles the real camera shim, lower-body Attack case and displacement/collision
handoff, with the game's headers. The fixture supplies input, animation cursors
and collision responses. It does not run the upper-body FSM, animate a skeleton,
resolve map geometry or verify hit detection; in-game testing remains necessary.
"""
import argparse
import os
from pathlib import Path
import shlex
import subprocess
import tempfile


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--source-ref")
    parser.add_argument("--sanitize", action="store_true")
    args = parser.parse_args()
    repo = Path(__file__).resolve().parents[2]
    path = "src/bodyprog/player_control.c"
    if args.source_ref:
        source = subprocess.check_output(
            ["git", "show", f"{args.source_ref}:{path}"], cwd=repo, text=True)
    else:
        source = (repo / path).read_text()

    def part(start, end, offset=0):
        a = source.index(start, offset)
        b = source.index(end, a)
        return f'\n#line {source[:a].count(chr(10)) + 1} "{path}"\n' + source[a:b]

    # Keep the actual includes, constants, globals and stick helper. Unrelated
    # functions are discarded by the linker; no game assets are linked.
    prefix = source[:source.index("/* Per-weapon steady gun-forward")] + "\n#endif\n"
    prefix += part("s_800C44F0 D_800C44F0", "#define playerProps")
    prefix += "\n#define playerProps g_SysWork.playerWork.player.properties.player\n"
    shim = part("            /* PC movement shim (default path).",
                "\n#else\n            Player_LowerBodyUpdate",
                source.index("void Player_LogicUpdate("))
    native = part("        case PlayerLowerBodyState_Attack:\n            // If weapon is katana.",
                  "\n        case PlayerLowerBodyState_Reload:",
                  source.index("void Player_LowerBodyUpdate("))
    generated = prefix + """
#define SH_AIM_KF_REACHED_P(kf) (player->model.anim.keyframeIdx >= (kf))
void Player_LowerBodyUpdate(s_SubCharacter* player, s_PlayerExtra* extra) {
    switch (extra->lowerBodyState) {
""" + native + """
    default: abort();
    }
    func_8007B924(player, extra);
}
void TestShim(s_SubCharacter* player, s_PlayerExtra* extra) {
#define playerExtra g_SysWork.playerWork.extra
""" + shim + """
#undef playerExtra
}
"""
    generated += part(
        "void func_8007C0D8(s_SubCharacter* player, s_PlayerExtra* extra, GsCOORDINATE2* coords) //",
        "\n    if (g_SysWork.playerWork.extra.upperBodyState == PlayerUpperBodyState_RunForward") + "\n}\n"

    flags = ["-std=gnu11", "-DSH_PC_PORT", "-DVER_USA", "-DSKIP_ASM",
             "-Dstatic_assert=_Static_assert", "-O1", "-g", "-fwrapv",
             "-fno-strict-aliasing", "-ffunction-sections", "-fdata-sections"]
    flags += ["-I" + str(repo / p) for p in (
        "pc_port/include", "pc_port/include/psyq_compat", "pc_port/src", "include",
        "include/decomp", "pc_port/PsyCross/include/psx", "pc_port/PsyCross/include")]
    flags += shlex.split(subprocess.check_output(
        ["pkg-config", "--cflags", "sdl2"], text=True))
    if args.sanitize:
        flags += ["-fsanitize=address,undefined", "-fno-sanitize-recover=all"]
    compiler = shlex.split(os.environ.get("CC", "gcc"))
    with tempfile.TemporaryDirectory(prefix="sh-katana-") as directory:
        out = Path(directory)
        (out / "generated.c").write_text(generated)
        subprocess.run([*compiler, *flags, "-c", str(out / "generated.c"),
                        "-o", str(out / "generated.o")], check=True)
        fixture = Path(__file__).with_name("katana_lunge_fixture.c")
        subprocess.run([*compiler, *flags, str(fixture), str(out / "generated.o"),
                        "-Wl,--gc-sections", "-lm", "-o", str(out / "test")], check=True)
        subprocess.run([str(out / "test")], check=True)


if __name__ == "__main__":
    main()
