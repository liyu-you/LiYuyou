import subprocess, os, time
d = r'C:\Users\LiyuYou\Desktop\Q\_reader_test'
env = os.environ.copy()
env['PATH'] = r'D:\software\QT\Tools\CMake_64\bin;D:\software\QT\Tools\Ninja;D:\software\QT\Tools\mingw1310_64\bin;D:\software\QT\6.11.2\mingw_64\bin;' + env['PATH']
exe = os.path.join(d, 'readerwidget_test.exe')
try:
    rr = subprocess.run([exe], cwd=d, env=env, capture_output=True, timeout=10)
    print('rc', rr.returncode)
    print('STDOUT:'); print(rr.stdout.decode('utf-8','replace'))
    print('STDERR:'); print(rr.stderr.decode('utf-8','replace')[:800])
except subprocess.TimeoutExpired as e:
    print('TIMEOUT (widget loop). stdout so far:')
    print((e.stdout or b'').decode('utf-8','replace'))
