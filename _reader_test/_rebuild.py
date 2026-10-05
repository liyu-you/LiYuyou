import subprocess, os, time, signal
d = r'C:\Users\LiyuYou\Desktop\Q\_reader_test'
env = os.environ.copy()
env['PATH'] = r'D:\software\QT\Tools\CMake_64\bin;D:\software\QT\Tools\Ninja;D:\software\QT\Tools\mingw1310_64\bin;D:\software\QT\6.11.2\mingw_64\bin;' + env['PATH']
env['QT_QPA_PLATFORM'] = 'offscreen'
# kill stray
subprocess.run(['taskkill','/F','/IM','readerwidget_test.exe'], capture_output=True)
time.sleep(1)
exe = os.path.join(d, 'readerwidget_test.exe')
if os.path.exists(exe):
    try: os.remove(exe)
    except Exception:
        try: os.rename(exe, exe+'.old')
        except Exception as e: print('rename fail', e)
r = subprocess.run(['ninja', 'readerwidget_test'], cwd=d, env=env, capture_output=True)
print('build rc', r.returncode)
if r.returncode != 0:
    print(r.stdout.decode('utf-8','replace')[-1200:]); print(r.stderr.decode('utf-8','replace')[-1200:])
else:
    try:
        rr = subprocess.run([exe], cwd=d, env=env, capture_output=True, timeout=20)
        print('rc', rr.returncode, hex(rr.returncode & 0xFFFFFFFF))
        print('STDOUT:'); print(rr.stdout.decode('utf-8','replace'))
        print('STDERR:'); print(rr.stderr.decode('utf-8','replace')[:800])
    except subprocess.TimeoutExpired as e:
        print('TIMEOUT. stdout:'); print((e.stdout or b'').decode('utf-8','replace'))
