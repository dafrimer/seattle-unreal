"""Snapshot approved Unity exports and authoritative collider data for Unreal migration."""
import hashlib
import json
import math
from pathlib import Path
import re
import shutil
import struct
import yaml

ROOT=Path(__file__).resolve().parents[1]
SOURCE=ROOT.parent/'seattle-rebuilt/AssetsExport'
NATIVE=ROOT.parent/'seattle-rebuilt/Unity/Assets/Generated/Meshes'
DATA=ROOT/'SourceData'


def safe(value): return re.sub(r'[^A-Za-z0-9_]','_',value)
def vec(x,y,z): return {'x':float(x),'y':float(y),'z':float(z)}
def convert(v): return vec(v['z']*100,v['x']*100,v['y']*100)
def sha(path): return hashlib.sha256(path.read_bytes()).hexdigest()


def obj_bounds(path):
    points=[]
    with path.open() as stream:
        for line in stream:
            if line.startswith('v '):
                x,y,z=map(float,line.split()[1:4]); points.append((-z*100,x*100,y*100))
    if not points: raise ValueError('Empty mesh '+str(path))
    return {'min':vec(*[min(p[i] for p in points) for i in range(3)]),
            'max':vec(*[max(p[i] for p in points) for i in range(3)])}


def collider_obj(source,target):
    text=source.read_text()
    mesh=yaml.safe_load('\n'.join(line for line in text.splitlines() if not line.startswith(('%','---'))))['Mesh']
    vertex=mesh['m_VertexData']; count=vertex['m_VertexCount']; channels=vertex['m_Channels']
    active=[c for c in channels if c['dimension']]
    if any(c['stream']!=0 or c['format']!=0 for c in active):
        raise ValueError('Unsupported native collider vertex format: '+str(source))
    stride=max(c['offset']+4*c['dimension'] for c in active)
    payload=bytes.fromhex(vertex['_typelessdata'])
    if len(payload)!=stride*count: raise ValueError('Collider byte stride mismatch')
    positions=[struct.unpack_from('<3f',payload,i*stride+channels[0]['offset']) for i in range(count)]
    # Unity 6000.6 marks serialized byte-array scalars with a trailing 'y'.
    encoded_indices=mesh['m_IndexBuffer']
    if encoded_indices.endswith('y'): encoded_indices=encoded_indices[:-1]
    indices=bytes.fromhex(encoded_indices)
    width=2 if mesh['m_IndexFormat']==0 else 4
    expected_bytes=max(sub['firstByte']+sub['indexCount']*width for sub in mesh['m_SubMeshes'])
    if len(indices)!=expected_bytes: raise ValueError('Collider index buffer length mismatch')
    triangles=[]
    for sub in mesh['m_SubMeshes']:
        if sub['topology']!=0: raise ValueError('Collider is not triangles')
        fmt='<'+('H' if width==2 else 'I')*sub['indexCount']
        ids=struct.unpack_from(fmt,indices,sub['firstByte'])
        ids=[i+sub['baseVertex'] for i in ids]
        triangles.extend([ids[i:i+3] for i in range(0,len(ids),3)])
    with target.open('w') as out:
        out.write('# Read-only extraction of the approved native Unity collider.\n')
        for x,y,z in positions: out.write('v %.9g %.9g %.9g\n'%(x,y,-z))
        for a,b,c in triangles: out.write('f %d %d %d\n'%(c+1,b+1,a+1))
    return {'source':str(source),'sha256':sha(source),'vertices':count,'triangles':len(triangles)}


def prop_key(prop):
    if prop['kind']=='tree':
        def f32(v): return struct.unpack('<f',struct.pack('<f',v))[0]
        x=round(f32(f32(prop['position']['x'])*100)); z=round(f32(f32(prop['position']['z'])*100))
        value=((x&0xffffffff)*73856093)&0xffffffff
        value^=((z&0xffffffff)*19349663)&0xffffffff
        value^=value>>16; value=(value*2246822519)&0xffffffff; value^=value>>13
        return 'tree-'+str(value%3)
    if prop['kind']=='street-sign':
        name=prop['name'].upper()
        if 'UNIVERSITY' in name: name='UNIVERSITY ST'
        elif any(k in name for k in ('FIFTH','5TH','5 AV')): name='5TH AVE'
        return 'street-sign-'+re.sub(r'[^A-Za-z0-9_-]','-',name)
    return prop['kind']


def main():
    DATA.mkdir(exist_ok=True)
    (DATA/'Colliders').mkdir(exist_ok=True); (DATA/'FBX').mkdir(exist_ok=True)
    index=json.loads((SOURCE/'index.json').read_text()); layout=json.loads((SOURCE/'scene-layout.json').read_text())
    materials={}
    for path in sorted(SOURCE.glob('*.materials.json')):
        for mat in json.loads(path.read_text())['materials']:
            if mat['name'] in materials and materials[mat['name']]!=mat: raise ValueError('Conflicting material '+mat['name'])
            materials[mat['name']]=mat
    (DATA/'Textures').mkdir(exist_ok=True)
    for mat in materials.values():
        for key in ('albedo','normalOpenGL','roughness','metallicSmoothness','occlusion'):
            if not mat[key]: continue
            source=SOURCE/mat[key]; target=DATA/'Textures'/source.name
            if not target.exists(): shutil.copy2(source,target)
            if sha(source)!=sha(target): raise ValueError('Texture copy mismatch')
            mat[key]=str(target)
        mat['twoSidedFoliage']=mat['name'].startswith('leaf')
    meshes=[]; instances=[]; extraction=[]
    def add_mesh(identifier,path,kind,collider=None,nanite=False):
        spec={'id':identifier,'sourceObj':str(path),'fbx':str(DATA/'FBX'/(safe(identifier)+'.fbx')),
            'materials':[m['name'] for m in json.loads(path.with_suffix('.materials.json').read_text())['materials']],
            'collisionFbx':str(DATA/'FBX'/(safe(identifier)+'_collision.fbx')) if collider else '',
            'collisionObj':str(collider) if collider else '', 'bounds':obj_bounds(path),'kind':kind,'nanite':nanite}
        if collider: spec['collisionBounds']=obj_bounds(collider)
        meshes.append(spec)
    for building in index['buildings']:
        add_mesh(building['id'],SOURCE/building['obj'],'building',SOURCE/(building['id']+'-collision.obj'),True)
        instances.append({'id':building['id'],'label':'building:'+building['id'],'mesh':building['id'],
            'position':convert(building['pivot']),'yaw':0.,'scale':1.,'collision':True})
    for prop in index['props']:
        kind=prop['kind']; collider=DATA/'Colliders'/('prop-'+kind+'.obj')
        extraction.append(collider_obj(NATIVE/('Prop-'+kind+'-Collision.asset'),collider))
        add_mesh('prop-'+kind,SOURCE/('prop-'+kind+'.obj'),'prop',collider)
    for i,prop in enumerate(layout['props']):
        instances.append({'id':'prop-'+str(i),'label':'prop:%d:%s'%(i,prop['kind']),'mesh':'prop-'+prop_key(prop),
            'position':convert(prop['position']),'yaw':prop['yaw'],'scale':prop['scale'],'collision':True})
    for surface in layout['surfaces']:
        identifier='surface-'+surface['id']; add_mesh(identifier,SOURCE/(identifier+'.obj'),'surface')
        instances.append({'id':identifier,'label':'surface:'+surface['id'],'mesh':identifier,
            'position':vec(0,0,0),'yaw':0.,'scale':1.,'collision':surface['collider']})
    output={'schemaVersion':1,'sourceRoot':str(SOURCE),'materials':list(materials.values()),'meshes':meshes,'instances':instances,
        'expected':{'buildings':len(index['buildings']),'props':len(layout['props']),'surfaces':len(layout['surfaces'])},
        'coordinatePolicy':'UE(cm)=(UnityZ,UnityX,UnityY)*100; FBX export uses Blender Zup/-Yforward and baked -90degZ object rotation.',
        'colliderExtraction':extraction,'sourceLayoutSha256':sha(SOURCE/'scene-layout.json')}
    (DATA/'import-manifest.json').write_text(json.dumps(output,indent=2)+'\n')
    for name in ('source-attribution.json','material-sources.json','CC0-1.0.txt'):
        shutil.copy2(SOURCE/name,DATA/name)
    print(json.dumps({'meshes':len(meshes),'materials':len(materials),'instances':len(instances),'collider_meshes':len(extraction)},indent=2))


if __name__=='__main__': main()
