import os, subprocess, sys
sys.stdout.reconfigure(encoding="utf-8", errors="backslashreplace")

d = r"C:\Users\LiyuYou\Desktop\Q\PureRefCanvas\build"
env = os.environ.copy()
env["PATH"] = (r"D:\software\QT\Tools\CMake_64\bin;D:\software\QT\Tools\Ninja;"
               r"D:\software\QT\Tools\mingw1310_64\bin;D:\software\QT\6.11.2\mingw_64\bin;"
               + env["PATH"])
r = subprocess.run(["ninja"], cwd=d, env=env, capture_output=True)
out = (r.stdout or b"").decode("utf-8", "replace")
err = (r.stderr or b"").decode("utf-8", "replace")
print("rc =", r.returncode)
lines = out.splitlines()
print("=== last 25 stdout lines ===")
for ln in lines[-25:]:
    print(ln)
if err.strip():
    print("=== stderr (tail) ===")
    for ln in err.splitlines()[-30:]:
        print(ln)
