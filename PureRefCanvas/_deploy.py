import shutil, os, time
src = r'C:\Users\LiyuYou\Desktop\Q\PureRefCanvas\build\里与由.exe'
dst = r'C:\Users\LiyuYou\Desktop\Q\release\里与由\里与由.exe'
# remove old dst if locked by renaming
if os.path.exists(dst):
    try:
        os.remove(dst)
    except Exception:
        try:
            os.rename(dst, dst + '.bak')
        except Exception as e:
            print('could not replace old exe:', e)
shutil.copy2(src, dst)
print('deployed:', os.path.getsize(dst), 'bytes')
print('mtime:', time.ctime(os.path.getmtime(dst)))
