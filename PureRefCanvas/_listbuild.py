import os
d = r'C:\Users\LiyuYou\Desktop\Q\PureRefCanvas\build'
for f in os.listdir(d):
    if 'manifest' in f.lower() or f.endswith('.exe'):
        print(repr(f))
