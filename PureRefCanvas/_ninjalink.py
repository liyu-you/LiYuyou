import subprocess, os
d = r'C:\Users\LiyuYou\Desktop\Q\PureRefCanvas\build'
env = os.environ.copy()
env['PATH'] = r'D:\software\QT\Tools\CMake_64\bin;D:\software\QT\Tools\Ninja;D:\software\QT\Tools\mingw1310_64\bin;D:\software\QT\6.11.2\mingw_64\bin;' + env['PATH']
r = subprocess.run(['ninja', '-n'], cwd=d, env=env, capture_output=True)
out = r.stdout.decode('utf-8','replace')
lines = [l for l in out.splitlines() if l.strip()]
print('num steps:', len(lines))
print('=== LAST STEP (link) ===')
print(lines[-1][:3000])
