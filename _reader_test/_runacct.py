import subprocess, os
d = r'C:\Users\LiyuYou\Desktop\Q\_reader_test'
qt = r'D:/software/QT/6.11.2/mingw_64'
env = os.environ.copy()
env['PATH'] = r'D:\software\QT\Tools\mingw1310_64\bin;D:\software\QT\6.11.2\mingw_64\bin;' + env['PATH']
src = r'C:/Users/LiyuYou/Desktop/Q/PureRefCanvas'
cmd = [r'D:\software\QT\Tools\mingw1310_64\bin\c++.exe',
       '-fPIC',
       f'-I{qt}/include', f'-I{qt}/include/QtCore',
       f'-I{src}',
       '-std=c++17',
       r'C:\Users\LiyuYou\Desktop\Q\_reader_test\accounting_console.cpp',
       src + r'\booklibrary.cpp',
       f'{qt}/lib/libQt6Core.a',
       '-o', r'C:\Users\LiyuYou\Desktop\Q\_reader_test\accounting_console.exe']
r = subprocess.run(cmd, cwd=d, env=env, capture_output=True)
print('compile rc', r.returncode)
print(r.stdout.decode('utf-8','replace')[-800:])
print(r.stderr.decode('utf-8','replace')[-1500:])
if r.returncode == 0:
    exe = r'C:\Users\LiyuYou\Desktop\Q\_reader_test\accounting_console.exe'
    rr = subprocess.run([exe], cwd=d, env=env, capture_output=True)
    print('=== RUN ===')
    print(rr.stdout.decode('utf-8','replace'))
    print(rr.stderr.decode('utf-8','replace')[:500])
    print('rc', rr.returncode)
