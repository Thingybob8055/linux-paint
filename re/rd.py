import struct,sys
d=open(sys.argv[1],'rb').read()
pe=struct.unpack_from('<I',d,0x3c)[0]; nsec=struct.unpack_from('<H',d,pe+6)[0]; osz=struct.unpack_from('<H',d,pe+20)[0]
base=struct.unpack_from('<Q',d,pe+24+24)[0]
secs=[struct.unpack_from('<8sIIII',d,pe+24+osz+i*40) for i in range(nsec)]
def off(va):
    r=va-base
    for n,vs,a,rs,rp in secs:
        if a<=r<a+rs: return r-a+rp
syms={}
for l in open(sys.argv[2]):
    a,t,n=l.rstrip('\n').split('\t'); syms.setdefault(n.split('::')[-1] if '::' in n else n,int(a,16)); syms[n]=int(a,16)
name,fmt,count=sys.argv[3],sys.argv[4],int(sys.argv[5])
va=syms[name] if name in syms else int(name,16)
o=off(va); sz=struct.calcsize('<'+fmt)
print(hex(va),[struct.unpack_from('<'+fmt,d,o+i*sz) for i in range(count)])
