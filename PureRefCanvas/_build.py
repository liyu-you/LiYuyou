import subprocess, os
d = r'C:\Users\LiyuYou\Desktop\Q\PureRefCanvas\build'
env = os.environ.copy()
env['PATH'] = r'D:\software\QT\Tools\CMake_64\bin;D:\software\QT\Tools\Ninja;D:\software\QT\Tools\mingw1310_64\bin;D:\software\QT\6.11.2\mingw_64\bin;' + env['PATH']
print('manifest exists:', os.path.exists(os.path.join(d, '里与由.exe.manifest')))
r = subprocess.run(['ninja'], cwd=d, env=env, capture_output=True)
print('rc=', r.returncode)
print('=== STDOUT (tail) ===')
print(r.stdout.decode('utf-8','replace')[-3000:])
print('=== STDERR ===')
print(r.stderr.decode('utf-8','replace')[-2000:])
