"""Convert approved mesh assets to FBX with baked axes, without rendering."""
import bpy
import json
import math
from pathlib import Path
from mathutils import Matrix, Vector
import re

ROOT=Path(__file__).resolve().parents[1]
manifest=json.loads((ROOT/'SourceData/import-manifest.json').read_text())
bpy.ops.object.select_all(action='SELECT'); bpy.ops.object.delete(use_global=False)
scene=bpy.context.scene
scene.unit_settings.system='METRIC'; scene.unit_settings.scale_length=1.
results=[]


def export_one(source,destination,expected):
    bpy.ops.object.select_all(action='SELECT'); bpy.ops.object.delete(use_global=False)
    # Remove orphaned datablocks so Blender cannot append numeric material suffixes
    # when another source mesh uses the same approved material name.
    for mesh in list(bpy.data.meshes):
        if mesh.users == 0: bpy.data.meshes.remove(mesh)
    for material in list(bpy.data.materials):
        if material.users == 0: bpy.data.materials.remove(material)
    bpy.ops.wm.obj_import(filepath=source,forward_axis='NEGATIVE_Z',up_axis='Y',use_split_objects=False,use_split_groups=False)
    objects=[obj for obj in scene.objects if obj.type=='MESH']
    if len(objects)!=1: raise RuntimeError('Expected one mesh in '+source)
    obj=objects[0]
    # Blender import is (UnityX,UnityZ,UnityY); rotate -90 around Z to (UnityZ,-UnityX,UnityY).
    # Unreal's documented legacy Zup/-Yforward FBX conversion flips Y to its left-handed basis.
    obj.matrix_world=Matrix.Rotation(-math.pi/2,4,'Z')@obj.matrix_world
    bpy.context.view_layer.objects.active=obj
    obj.select_set(True)
    bpy.ops.object.transform_apply(location=True,rotation=True,scale=True)
    for material in obj.data.materials:
        if material:
            name=re.sub(r'\.\d{3}$','',material.name)
            material.name=re.sub(r'[^A-Za-z0-9_]','_',name)
            material.use_nodes=False
    obj.name=Path(destination).stem
    points=[obj.matrix_world@vertex.co for vertex in obj.data.vertices]
    actual={'min':dict(zip('xyz',[min(p.x for p in points)*100,-max(p.y for p in points)*100,min(p.z for p in points)*100])),
            'max':dict(zip('xyz',[max(p.x for p in points)*100,-min(p.y for p in points)*100,max(p.z for p in points)*100]))}
    if any(abs(actual[end][axis]-expected[end][axis])>.03 for end in ('min','max') for axis in 'xyz'):
        raise ValueError('Blender coordinate conversion mismatch '+source+' '+str(actual)+' vs '+str(expected))
    bpy.ops.export_scene.fbx(filepath=destination,use_selection=True,object_types={'MESH'},
        global_scale=1.,apply_unit_scale=True,apply_scale_options='FBX_SCALE_UNITS',axis_forward='-Y',axis_up='Z',
        use_space_transform=True,bake_space_transform=False,use_mesh_modifiers=False,mesh_smooth_type='OFF',
        use_tspace=True,add_leaf_bones=False,bake_anim=False,path_mode='STRIP',embed_textures=False)
    if Path(destination).stat().st_size<100: raise ValueError('FBX export incomplete')
    return {'source':source,'fbx':destination,'vertices':len(points),'triangles':len(obj.data.polygons),
            'expectedUnrealBounds':expected,'blenderConvertedBounds':actual,'materialSlots':[m.name if m else '' for m in obj.data.materials]}


for index,item in enumerate(manifest['meshes']):
    results.append(export_one(item['sourceObj'],item['fbx'],item['bounds']))
    if item['collisionObj']:
        results.append(export_one(item['collisionObj'],item['collisionFbx'],item['collisionBounds']))
    print('FBX_PARITY_EXPORT %d/%d %s'%(index+1,len(manifest['meshes']),item['id']),flush=True)
(ROOT/'Evidence/fbx-export-result.json').write_text(json.dumps({'success':True,'renderer_used':False,'exports':results},indent=2)+'\n')
print('FBX_PARITY_EXPORT_COMPLETE',flush=True)
