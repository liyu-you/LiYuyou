import subprocess, os
d = r'C:\Users\LiyuYou\Desktop\Q\_reader_test'
qt = r'D:/software/QT/6.11.2/mingw_64'
src = r'C:/Users/LiyuYou/Desktop/Q/PureRefCanvas'
env = os.environ.copy()
env['PATH'] = r'D:\software\QT\Tools\mingw1310_64\bin;D:\software\QT\6.11.2\mingw_64\bin;' + env['PATH']
# moc the header
moc = os.path.join(qt, 'bin', 'moc.exe')
moc_out = r'C:\Users\LiyuYou\Desktop\Q\_reader_test\booklibrary.moc.cpp'
r = subprocess.run([moc, src + '/booklibrary.h', '-o', moc_out], cwd=d, env=env, capture_output=True)
print('moc rc', r.returncode, r.stderr.decode('utf-8','replace')[-300:])
cmd = [r'D:\software\QT\Tools\mingw1310_64\bin\c++.exe',
       f'-I{qt}/include', f'-I{qt}/include/QtCore', f'-I{src}',
       '-std=c++17',
       r'C:\Users\LiyuYou\Desktop\Q\_reader_test\accounting_console.cpp',
       src + r'\booklibrary.cpp', moc_out,
       f'{qt}/lib/libQt6Core.a',
       '-o', r'C:\Users\LiyuYou\Desktop\Q\_reader_test\accounting_console.exe']
r = subprocess.run(cmd, cwd=d, env=env, capture_output=True)
print('compile rc', r.returncode)
print(r.stderr.decode('utf-8','replace')[-1000:])
if r.returncode == 0:
    exe = r'C:\Users\LiyuYou\Desktop\Q\_reader_test\accounting_console.exe'
    rr = subprocess.run([exe], cwd=d, env=env, capture_output=True)
    print('=== RUN ===')
    print(rr.stdout.decode('utf-8','replace'))
    print(rr.stderr.decode('utf-8','replace')[:500])
    print('rc', rr.returncode)
