"""Capture real Unreal editor asset views; explicitly not a playable-runtime verification."""
import json
import math
from pathlib import Path
import time
import traceback
import unreal

root=Path(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir()))
evidence=root/'Evidence'
layout=json.loads((root/'Content/StreetData/layout.json').read_text())
levels=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
editor=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
actors=unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
if not levels.load_level('/Game/Seattle/Maps/FifthUniversity'):
    raise RuntimeError('The imported native map is required')
world=editor.get_editor_world()
unreal.SystemLibrary.execute_console_command(world,'r.ScreenPercentage 100')
unreal.SystemLibrary.execute_console_command(world,'r.MotionBlurQuality 0')
camera=actors.spawn_actor_from_class(unreal.CameraActor,unreal.Vector(0,0,0))
camera.set_actor_label('Temporary asset inspection camera')
views=[next(v for v in layout['views'] if v['id']==name) for name in ('street-entry','hotel-corner','overview')]
results=[]
state={'index':0,'task':None,'readyAt':time.monotonic()+60,'started':time.monotonic()}
unreal.EditorPythonScripting.set_keep_python_script_alive(True)


def place_view(index):
    view=views[index]
    p=view['position']; target=view['target']
    position=unreal.Vector(p['x'],p['y'],p['z']); look=unreal.Vector(target['x'],target['y'],target['z'])
    rotation=unreal.MathLibrary.find_look_at_rotation(position,look)
    camera.set_actor_location_and_rotation(position,rotation,False,False)
    component=camera.get_component_by_class(unreal.CameraComponent)
    horizontal=math.degrees(2*math.atan(math.tan(math.radians(view['fov']/2))*16/9))
    component.set_field_of_view(horizontal)
    editor.set_level_viewport_camera_info(position,rotation)


def finish(error=None):
    unreal.unregister_slate_post_tick_callback(state['handle'])
    actors.destroy_actor(camera)
    result={'success':error is None,'assetPreviewOnly':True,'nativeRuntimeCompiled':False,'gameplayVerified':False,
        'views':results,'error':error,'engine':unreal.SystemLibrary.get_engine_version(),
        'disclosure':'Actual Unreal editor asset renders. Playable migration remains blocked by missing C++ compiler/SDK approval.'}
    (evidence/'unreal-preview-result.json').write_text(json.dumps(result,indent=2)+'\n')
    unreal.log('UNREAL_ASSET_PREVIEW_FINISHED '+str(result['success']))
    unreal.EditorPythonScripting.set_keep_python_script_alive(False)


def tick(delta):
    try:
        if time.monotonic()-state['started']>600:
            finish('Timed out waiting for native screenshots'); return
        if state['task'] is not None:
            if not state['task'].is_task_done(): return
            view=views[state['index']]
            path=evidence/('unreal-assets-'+view['id']+'.png')
            if not path.is_file(): raise RuntimeError('Unreal screenshot file missing: '+str(path))
            results.append({'id':view['id'],'path':str(path),'bytes':path.stat().st_size,'position':view['position'],'target':view['target']})
            state['index']+=1; state['task']=None
            if state['index']==len(views): finish(); return
            place_view(state['index']); state['readyAt']=time.monotonic()+12
        elif time.monotonic()>=state['readyAt']:
            path=evidence/('unreal-assets-'+views[state['index']]['id']+'.png')
            state['task']=unreal.AutomationLibrary.take_high_res_screenshot(1280,720,str(path),camera=camera,force_game_view=True)
    except Exception:
        finish(traceback.format_exc())


place_view(0)
state['handle']=unreal.register_slate_post_tick_callback(tick)
unreal.log('UNREAL_ASSET_PREVIEW_READY')
