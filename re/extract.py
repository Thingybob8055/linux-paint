import json,sys,os,struct
exe,idx,out=sys.argv[1:4]
data=open(exe,'rb').read()
for line in open(idx):
    r=json.loads(line); t=str(r['t']); d=data[r['o']:r['o']+r['b']]
    os.makedirs(f"{out}/{t}",exist_ok=True)
    if t=='2':
        hs=struct.unpack('<I',d[:4])[0]; bpp=struct.unpack('<H',d[14:16])[0]; clr=struct.unpack('<I',d[32:36])[0] if hs>=36 else 0
        ncol=clr if clr else (1<<bpp if bpp<=8 else 0)
        comp=struct.unpack('<I',d[16:20])[0]
        extra=12 if comp==3 and hs==40 else 0
        off=14+hs+ncol*4+extra
        d=b'BM'+struct.pack('<IHHI',14+len(d),0,0,off)+d; ext='bmp'
    elif t=='IMAGE': ext='png' if d[:4]==b'\x89PNG' else 'bin'
    elif t=='24': ext='xml'
    else: ext='bin'
    open(f"{out}/{t}/{r['n']}.{ext}",'wb').write(d)
