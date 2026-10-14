import struct,sys
from unicorn import *
from unicorn.x86_const import *
exe,symf=sys.argv[1:3]
d=open(exe,'rb').read()
pe=struct.unpack_from('<I',d,0x3c)[0]; nsec=struct.unpack_from('<H',d,pe+6)[0]; osz=struct.unpack_from('<H',d,pe+20)[0]
base=struct.unpack_from('<Q',d,pe+24+24)[0]; imgsz=struct.unpack_from('<I',d,pe+24+56)[0]
syms={}
for l in open(symf):
    a,t,n=l.rstrip('\n').split('\t')
    if t=='Function': syms.setdefault(n,int(a,16))
mu=Uc(UC_ARCH_X86,UC_MODE_64)
mu.mem_map(base,(imgsz+0xfff)&~0xfff)
mu.mem_write(base,d[:0x1000])
for i in range(nsec):
    n,vs,va,rs,rp=struct.unpack_from('<8sIIII',d,pe+24+osz+i*40)
    mu.mem_write(base+va,d[rp:rp+min(rs,vs if vs else rs)])
STK=0x7000000; mu.mem_map(STK,0x100000); HEAP=0x9000000; mu.mem_map(HEAP,0x1000000)
heap=[HEAP+0x100]
vt={0x1000a7f70:'Line',0x1000a8b78:'Arc',0x1000a9840:'Bezier',0x1000a98f8:'Ellipse',0x1000a99b0:'RoundedRect'}
prims=[]; shapes=[]
STOP=0x5000000; mu.mem_map(STOP,0x1000)
def ret(mu,val):
    rsp=mu.reg_read(UC_X86_REG_RSP); ra=struct.unpack('<Q',mu.mem_read(rsp,8))[0]
    mu.reg_write(UC_X86_REG_RSP,rsp+8); mu.reg_write(UC_X86_REG_RIP,ra); mu.reg_write(UC_X86_REG_RAX,val)
lo,hi=syms['CShapeStore::CreateHexagon'],syms['CPaintShape::CPaintShape']
def hook(mu,addr,size,ud):
    rcx=mu.reg_read(UC_X86_REG_RCX); rdx=mu.reg_read(UC_X86_REG_RDX)
    if addr==syms['MFC42U.DLL::operator_new']:
        p=heap[0]; heap[0]+=(rcx+15)&~15; ret(mu,p); return
    if addr==syms['CPaintShape::AddPrimitive']:
        b=bytes(mu.mem_read(rdx,0x28)); v=struct.unpack_from('<Q',b)[0]; t=vt.get(v,hex(v))
        if t=='Line': f=struct.unpack_from('<4i',b,8)
        elif t=='Bezier': f=struct.unpack_from('<8i',b,8)
        elif t=='Arc': f=struct.unpack_from('<4i2f',b,8)
        elif t in('Ellipse','RoundedRect'): f=struct.unpack_from('<4i',b,8)
        else: f=b.hex()
        prims.append((t,f)); ret(mu,0); return
    if addr==syms['CPaintShape::CPaintShape']: ret(mu,rcx); return
    if 'map' in ud and addr==ud['map']:
        key=struct.unpack('<i',mu.mem_read(rdx,4))[0]; shapes.append((key,list(prims))); prims.clear()
        p=heap[0]; heap[0]+=16; ret(mu,p); return
    if not (lo<=addr<hi) and addr!=STOP and not (syms.get('CShapeStore::CreateShapes',0)<=addr<0x100023c18):
        print('  stub call',hex(addr)); ret(mu,0); return
mapop=[a for n,a in syms.items() if n.startswith('std::map<int,CPaintShape') and n.endswith('operator[]')][0]
mu.hook_add(UC_HOOK_CODE,hook,{'map':mapop})
names=[n for n in syms if n.startswith('CShapeStore::Create')]
for n in sorted(names,key=lambda n:syms[n]):
    this=heap[0]; heap[0]+=0x100
    rsp=STK+0x80000; mu.mem_write(rsp,struct.pack('<Q',STOP)); mu.reg_write(UC_X86_REG_RSP,rsp)
    mu.reg_write(UC_X86_REG_RCX,this)
    try: mu.emu_start(syms[n],STOP,count=200000)
    except UcError as e: print('ERR',n,e,hex(mu.reg_read(UC_X86_REG_RIP)))
    print('##',n)
    for key,ps in shapes:
        print(' shape',hex(key))
        for t,f in ps: print('   ',t,f)
    shapes.clear(); prims.clear()
