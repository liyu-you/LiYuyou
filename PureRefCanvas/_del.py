import os
p = r'C:\Users\LiyuYou\Desktop\Q\PureRefCanvas\build\里与由.exe'
try:
    os.remove(p)
    print('removed')
except FileNotFoundError:
    print('already gone')
except Exception as e:
    print('failed:', e)
# also remove implib
for f in os.listdir(r'C:\Users\LiyuYou\Desktop\Q\PureRefCanvas\build'):
    if f.endswith('.dll.a'):
        try:
            os.remove(os.path.join(r'C:\Users\LiyuYou\Desktop\Q\PureRefCanvas\build', f))
            print('removed', f)
        except Exception as e:
            print('skip', f, e)
