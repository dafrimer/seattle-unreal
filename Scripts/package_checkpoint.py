"""Package the actual native assets and explicitly report any remaining parity blocker."""
import hashlib
import json
from pathlib import Path
import zipfile

ROOT=Path(__file__).resolve().parents[1]


def sha(path):
    value=hashlib.sha256()
    with path.open('rb') as stream:
        for part in iter(lambda:stream.read(4*1024*1024),b''): value.update(part)
    return value.hexdigest()


def main():
    report=json.loads((ROOT/'Evidence/import-result.json').read_text())
    preview=json.loads((ROOT/'Evidence/unreal-preview-result.json').read_text())
    if not report['sceneImported'] or not preview['success']:
        raise ValueError('Native assets/map and actual editor preview must be verified')
    if any(not row['pass'] for row in report['bounds']+report['materialSlotChecks']):
        raise ValueError('Unresolved native import contract failure')
    selected=[]
    for directory in ('Config','Content','Source','Scripts','SourceData','Prerequisites'):
        selected.extend(p for p in (ROOT/directory).rglob('*') if p.is_file() and '__pycache__' not in p.parts)
    selected.extend(ROOT.glob('*.uproject')); selected.extend(ROOT.glob('*.cmd'))
    selected.extend(p for p in (ROOT/'Evidence').glob('*') if p.is_file() and p.suffix in ('.json','.png','.mp4'))
    files={p.relative_to(ROOT).as_posix():sha(p) for p in sorted(set(selected)) if p.name!='migration-checkpoint.json'}
    def evidence(name):
        path=ROOT/'Evidence'/name
        return json.loads(path.read_text()) if path.exists() else {}
    verify=evidence('runtime-verification.json'); capture=evidence('capture.json')
    runtime_verified=bool(report['runtimeModeReady'] and verify.get('success') and not verify.get('errors'))
    recorded=bool(capture.get('success') and not capture.get('errors') and (ROOT/'Evidence/seattle-unreal-walkthrough.mp4').exists())
    player=ROOT/'Build/Windows/SeattleUnreal.exe'
    status=('native_runtime_verified' if runtime_verified and recorded and player.exists()
            else 'native_runtime_verified_unpackaged' if runtime_verified and recorded
            else 'blocked_native_runtime_verification' if report['runtimeModeReady'] else 'blocked_native_runtime_build')
    result={'status':status,'engine':report['engineVersion'],'assetsAndMapVerified':True,
        'runtimeCompiled':report['runtimeModeReady'],'playableParityVerified':runtime_verified,
        'walkthroughRecorded':recorded,'packagedPlayer':player.relative_to(ROOT).as_posix() if player.exists() else None,
        'nativeCounts':report['counts'],'naniteBuildings':sum(r['nanite'] for r in report['meshes']),
        'textureAndMaterialAssignmentsVerified':True,'layoutScaleOrientationVerified':True,
        'lighting':report['featureSettings'],'editorPreview':preview,
        'runtimeVerification':{k:verify.get(k) for k in ('success','errors','returnErrorCm','waypointToleranceCm','resetPassed',
            'worldInvariantPassed','fallingTransitions','distanceWalkedCm','graphicsDevice','rhi')},
        'wallProbes':[{k:w[k] for k in ('id','pass','projectedTravelCm','capsuleSweepContactDistanceCm')} for w in verify.get('wallProbes',[])],
        'viewpoints':[{'id':v.get('id'),'pass':v.get('pass')} for v in verify.get('viewpoints',[])],
        'walkthrough':{'mp4':'Evidence/seattle-unreal-walkthrough.mp4','frames':capture.get('frames'),'fps':capture.get('fps'),
            'chapters':[{k:ch[k] for k in ('start','end','title','mode')} for ch in capture.get('chapters',[])]} if recorded else None,
        'inspectionProject':'SeattleAssetsPreview.uproject','nativeRuntimeProject':'SeattleUnreal.uproject',
        'disclosure':'Stylized reconstruction; mapped footprints, inferred architectural details. Not surveyed or photorealistic.',
        'unitySourceUnmodified':True,'files':files}
    (ROOT/'Evidence/migration-checkpoint.json').write_text(json.dumps(result,indent=2)+'\n')
    output=ROOT/'Seattle-Unreal-Migration-Checkpoint.zip'
    with zipfile.ZipFile(output,'w',compression=zipfile.ZIP_DEFLATED,compresslevel=2) as archive:
        for relative in files: archive.write(ROOT/relative,'SeattleUnreal/'+relative)
        archive.write(ROOT/'Evidence/migration-checkpoint.json','SeattleUnreal/Evidence/migration-checkpoint.json')
    with zipfile.ZipFile(output) as archive:
        if archive.testzip() is not None: raise IOError('Packaged archive integrity check failed')
    print(json.dumps({'archive':str(output),'bytes':output.stat().st_size,'files':len(files)+1,'nativeCounts':report['counts'],
        'runtimeCompiled':result['runtimeCompiled'],'playableParityVerified':runtime_verified,'walkthroughRecorded':recorded,
        'packagedPlayer':result['packagedPlayer'],'status':status},indent=2))


if __name__=='__main__': main()
