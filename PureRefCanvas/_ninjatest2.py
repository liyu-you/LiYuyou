import subprocess, os
d = r'C:\Users\LiyuYou\Desktop\Q\PureRefCanvas\build'
env = os.environ.copy()
env['PATH'] = r'D:\software\QT\Tools\CMake_64\bin;D:\software\QT\Tools\Ninja;D:\software\QT\Tools\mingw1310_64\bin;D:\software\QT\6.11.2\mingw_64\bin;' + env['PATH']
r = subprocess.run(['ninja', '-t', 'targets'], cwd=d, env=env, capture_output=True)
out = r.stdout.decode('utf-8', 'replace')
lines = out.splitlines()
print('total targets:', len(lines))
for line in lines[:40]:
    print(repr(line))
