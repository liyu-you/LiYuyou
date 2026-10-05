import subprocess, os
d = r'C:\Users\LiyuYou\Desktop\Q\_reader_test'
env = os.environ.copy()
env['PATH'] = r'D:\software\QT\Tools\CMake_64\bin;D:\software\QT\Tools\Ninja;D:\software\QT\Tools\mingw1310_64\bin;D:\software\QT\6.11.2\mingw_64\bin;' + env['PATH']
env['QT_QPA_PLATFORM'] = 'offscreen'
exe = os.path.join(d, 'readerwidget_test.exe')
rr = subprocess.run([exe], cwd=d, env=env, capture_output=True, timeout=30)
print('rc', rr.returncode, hex(rr.returncode & 0xFFFFFFFF))
print('STDOUT:')
print(rr.stdout.decode('utf-8','replace'))
print('STDERR:')
print(rr.stderr.decode('utf-8','replace'))
