import os, re
d = r'C:\Users\LiyuYou\Desktop\Q\PureRefCanvas\build'
# bytes on disk
disk = [f for f in os.listdir(d) if f.endswith('.manifest')]
print('DISK:', [(f, f.encode('utf-8').hex()) for f in disk])
# bytes in build.ninja (utf-8)
with open(os.path.join(d, 'build.ninja'), 'rb') as fh:
    data = fh.read()
# find MANIFESTS line
for line in data.split(b'\n'):
    if line.startswith(b'MANIFESTS'):
        print('NINJA BYTES:', line.strip().decode('utf-8').encode('utf-8').hex(), line.decode('utf-8'))
# find any token ending .manifest
for m in re.finditer(rb'[\x20-\x7e]\.manifest', data):
    s = m.group(0)
    print('TOKEN near manifest:', s)
