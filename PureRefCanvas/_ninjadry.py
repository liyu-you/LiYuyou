import subprocess, os
d = r'C:\Users\LiyuYou\Desktop\Q\PureRefCanvas\build'
env = os.environ.copy()
env['PATH'] = r'D:\software\QT\Tools\CMake_64\bin;D:\software\QT\Tools\Ninja;D:\software\QT\Tools\mingw1310_64\bin;D:\software\QT\6.11.2\mingw_64\bin;' + env['PATH']
r = subprocess.run(['ninja', '-n', '-v'], cwd=d, env=env, capture_output=True)
print('rc=', r.returncode)
print('STDOUT:', r.stdout.decode('utf-8','replace')[:2000])
print('STDERR:', r.stderr.decode('utf-8','replace')[:2000])
