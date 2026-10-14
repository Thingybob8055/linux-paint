import struct,sys
d=open(sys.argv[1],'rb').read()
pe=struct.unpack_from('<I',d,0x3c)[0]; nsec=struct.unpack_from('<H',d,pe+6)[0]; opt=pe+24; osz=struct.unpack_from('<H',d,pe+20)[0]
dd=opt+112; dbg_rva,dbg_sz=struct.unpack_from('<II',d,dd+6*8)
secs=[struct.unpack_from('<8sIIII',d,opt+osz+i*40) for i in range(nsec)]
def off(r):
    for n,vs,va,rs,rp in secs:
        if va<=r<va+max(vs,rs): return r-va+rp
for i in range(dbg_sz//28):
    e=struct.unpack_from('<IIHHIIII',d,off(dbg_rva)+i*28)
    if e[4]==2:
        p=e[7]; sig,=struct.unpack_from('<4s',d,p)
        g=d[p+4:p+20]; age,=struct.unpack_from('<I',d,p+20); name=d[p+24:d.index(b'\0',p+24)].decode()
        a,b,c=struct.unpack_from('<IHH',g); guid='%08X%04X%04X'%(a,b,c)+g[8:].hex().upper()
        print(name, guid+'%X'%age)
