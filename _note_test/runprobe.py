import os, subprocess, sys
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

d = r"C:\Users\LiyuYou\Desktop\Q\_reader_test"
env = os.environ.copy()
env["PATH"] = (r"D:\software\QT\Tools\CMake_64\bin;D:\software\QT\Tools\Ninja;"
               r"D:\software\QT\Tools\mingw1310_64\bin;D:\software\QT\6.11.2\mingw_64\bin;"
               + env["PATH"])
env["QT_QPA_PLATFORM"] = "offscreen"
env["QT_QPA_PLATFORM_PLUGIN_PATH"] = r"D:\software\QT\6.11.2\mingw_64\plugins\platforms"
env["LIYIYOU_TEST_DIR"] = os.path.join(d, "build", "scratch")

r = subprocess.run(["ninja", "mw_probe"], cwd=d, env=env, capture_output=True)
print("build rc", r.returncode)
if r.returncode != 0:
    print((r.stdout or b"").decode("utf-8", "replace")[-2000:])
    print((r.stderr or b"").decode("utf-8", "replace")[-2000:])
    sys.exit(1)

try:
    rr = subprocess.run([os.path.join(d, "mw_probe.exe")], cwd=d, env=env,
                        capture_output=True, timeout=90)
    print("run rc", rr.returncode, hex(rr.returncode & 0xFFFFFFFF))
    out = (rr.stdout or b"").decode("utf-8", "replace")
    print("STDOUT (tail 3000):")
    print(out[-3000:])
    err = (rr.stderr or b"").decode("utf-8", "replace")
    if err.strip():
        print("STDERR (tail 500):")
        print(err[-500:])
except subprocess.TimeoutExpired as e:
    print("TIMEOUT")
    print((e.stdout or b"").decode("utf-8", "replace")[-2000:])
