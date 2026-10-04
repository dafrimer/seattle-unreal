"""Assemble already verified native assets after an interrupted map step; no re-import."""
import hashlib
import json
from pathlib import Path
import traceback
import unreal
from migrate_scene import StreetImporter, ROOT, safe_name, require

project=Path(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir()))
path=project/'Evidence/import-result.json'
report=json.loads(path.read_text())
manifest_bytes=(project/'SourceData/import-manifest.json').read_bytes()
require(hashlib.sha256(manifest_bytes).hexdigest()==report['sourceManifestSHA256'],'Source manifest changed; run full importer')
manifest=json.loads(manifest_bytes)
layout=json.loads((project/'Content/StreetData/layout.json').read_text())
require(len(report['meshes'])==len(manifest['meshes']),'Native assets have not all been verified')
importer=StreetImporter(manifest,layout,report)
mesh_results={row['id']:row for row in report['meshes']}
for row in manifest['materials']:
    material=importer.existing_owned(ROOT+'/Materials/M_'+safe_name(row['name']),unreal.Material)
    require(material is not None,'Missing saved material '+row['name'])
    importer.materials[row['name']]=material
for row in manifest['meshes']:
    result=mesh_results[row['id']]
    mesh=importer.existing_owned(result['asset'],unreal.StaticMesh)
    require(mesh is not None,'Missing saved mesh '+row['id'])
    importer.measured_bounds(mesh,row['bounds'],row['id'])
    importer.meshes[row['id']]=mesh
    importer.collision_modes[row['id']]=result['collisionMode']
    if result['nanite']:
        for material_name in row['materials']:
            material=importer.materials[material_name]
            unreal.MaterialEditingLibrary.set_base_material_usage(material,unreal.MaterialUsage.MATUSAGE_NANITE,True)
            importer.save(material)
    if result['collisionAsset']:
        collision=importer.existing_owned(result['collisionAsset'],unreal.StaticMesh)
        require(collision is not None,'Missing saved collision '+row['id'])
        importer.collisions[row['id']]=collision
report['errors']=[]
report['success']=False
try:
    importer.map()
    importer.set_runtime_mode()
    report['success']=True
except Exception as error:
    report['errors'].append({'message':str(error),'traceback':traceback.format_exc()})
    unreal.log_error('Map assembly stopped: '+str(error))
    raise
finally:
    report['counts']=dict(report.get('counts',{}),textures=len(report['textures']))
    path.write_text(json.dumps(report,indent=2)+'\n')
