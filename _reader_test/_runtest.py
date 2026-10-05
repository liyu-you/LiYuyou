import subprocess, os
d = r'C:\Users\LiyuYou\Desktop\Q\_reader_test'
env = os.environ.copy()
env['PATH'] = r'D:\software\QT\Tools\CMake_64\bin;D:\software\QT\Tools\Ninja;D:\software\QT\Tools\mingw1310_64\bin;D:\software\QT\6.11.2\mingw_64\bin;' + env['PATH']
r = subprocess.run(['ninja', 'readerwidget_test'], cwd=d, env=env, capture_output=True)
print('build rc', r.returncode)
print(r.stdout.decode('utf-8','replace')[-1500:])
print(r.stderr.decode('utf-8','replace')[-1500:])
if r.returncode == 0:
    env2 = dict(env)
    env2['QT_QPA_PLATFORM'] = 'offscreen'
    exe = os.path.join(d, 'readerwidget_test.exe')
    rr = subprocess.run([exe], cwd=d, env=env2, capture_output=True)
    print('=== TEST OUTPUT ===')
    print(rr.stdout.decode('utf-8','replace'))
    print(rr.stderr.decode('utf-8','replace')[-800:])
    print('test rc', rr.returncode)
