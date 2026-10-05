import subprocess, os
d = r'C:\Users\LiyuYou\Desktop\Q\PureRefCanvas\build'
env = os.environ.copy()
env['PATH'] = r'D:\software\QT\Tools\CMake_64\bin;D:\software\QT\Tools\Ninja;D:\software\QT\Tools\mingw1310_64\bin;D:\software\QT\6.11.2\mingw_64\bin;' + env['PATH']
# list targets
r = subprocess.run(['ninja', '-t', 'targets'], cwd=d, env=env, capture_output=True)
out = r.stdout.decode('utf-8', 'replace')
for line in out.splitlines():
    if 'manifest' in line.lower() or '.exe' in line.lower():
        print(repr(line))
print('--- stderr ---')
print(r.stderr.decode('utf-8','replace')[:500])
