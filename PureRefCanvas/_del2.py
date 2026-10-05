import os, time
d = r'C:\Users\LiyuYou\Desktop\Q\PureRefCanvas\build'
p = os.path.join(d, '里与由.exe')
if not os.path.exists(p):
    print('exe gone already')
else:
    for i in range(3):
        try:
            os.rename(p, p + '.old')
            print('renamed to .old')
            break
        except Exception as e:
            print('rename try', i, 'failed:', e)
            time.sleep(2)
