"""Place TRELLIS game-budget props next to the hand-authored props in the Seattle map and capture editor renders.
Nothing is saved: imported assets and spawned actors are discarded when the editor quits."""
import json
import math
import time
import traceback
from pathlib import Path
import unreal

LAB = Path('H:/Projects/asset-gen-lab/work')
OUT = LAB / 'unreal'
OUT.mkdir(parents=True, exist_ok=True)
levels = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
editor = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
if not levels.load_level('/Game/Seattle/Maps/FifthUniversity'):
    raise RuntimeError('FifthUniversity map required')
world = editor.get_editor_world()
for cmd in ('r.ScreenPercentage 100', 'r.MotionBlurQuality 0'):
    unreal.SystemLibrary.execute_console_command(world, cmd)

# Row of props on the roadway ahead of the spawn, hand-authored next to generated.
BASE_X, ROW_Y, GROUND_Z = -2450.0, -1150.0, 2.0
PAIRS = [  # (label, hand asset, generated glb, target height cm, y offset)
    ('hand-lamp', '/Game/Seattle/Meshes/SM_prop_lamp.SM_prop_lamp', None, 588.0, -520.0),
    ('gen-lamp', None, 'lamp-game.glb', 588.0, -300.0),
    ('hand-signal', '/Game/Seattle/Meshes/SM_prop_signal.SM_prop_signal', None, 587.0, 80.0),
    ('gen-signal', None, 'signal-game.glb', 587.0, 520.0),
]
report = {'placed': [], 'shots': [], 'error': None}


def import_glb(name):
    task = unreal.AssetImportTask()
    task.filename = str(LAB / 'trellis-v1' / name)
    task.destination_path = '/Game/AssetGenTest'
    task.automated = True
    task.replace_existing = True
    task.save = False
    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
    meshes = [unreal.load_asset(p) for p in task.imported_object_paths]
    meshes = [m for m in meshes if isinstance(m, unreal.StaticMesh)]
    if not meshes:
        raise RuntimeError('No StaticMesh imported from ' + name + ': ' + str(list(task.imported_object_paths)))
    return meshes[0]


for label, asset, glb, height, dy in PAIRS:
    mesh = unreal.load_asset(asset) if asset else import_glb(glb)
    actor = actors.spawn_actor_from_object(mesh, unreal.Vector(BASE_X, ROW_Y + dy, GROUND_Z))
    actor.set_actor_label('AssetGenTest ' + label)
    origin, extent = actor.get_actor_bounds(False)
    scale = height / max(extent.z * 2, 1e-3)
    actor.set_actor_scale3d(unreal.Vector(scale, scale, scale))
    origin, extent = actor.get_actor_bounds(False)
    loc = actor.get_actor_location()
    actor.set_actor_location(unreal.Vector(loc.x, loc.y, loc.z + (GROUND_Z - (origin.z - extent.z))), False, False)
    origin, extent = actor.get_actor_bounds(False)
    tris = mesh.get_num_triangles(0)
    report['placed'].append({'label': label, 'source': asset or glb, 'scale': round(scale, 3), 'triangles': tris,
                             'bounds_cm': [round(extent.x * 2, 1), round(extent.y * 2, 1), round(extent.z * 2, 1)]})

camera = actors.spawn_actor_from_class(unreal.CameraActor, unreal.Vector(0, 0, 0))
SHOTS = [
    ('row', unreal.Vector(BASE_X - 1500, ROW_Y, 300), unreal.Vector(BASE_X, ROW_Y, 300), 60),
    ('lamps-close', unreal.Vector(BASE_X - 700, ROW_Y - 410, 300), unreal.Vector(BASE_X, ROW_Y - 410, 300), 35),
]
state = {'i': -1, 'task': None, 'ready': time.monotonic() + 45, 'start': time.monotonic()}
unreal.EditorPythonScripting.set_keep_python_script_alive(True)


def aim(i):
    _, pos, target, fov = SHOTS[i]
    rot = unreal.MathLibrary.find_look_at_rotation(pos, target)
    camera.set_actor_location_and_rotation(pos, rot, False, False)
    camera.get_component_by_class(unreal.CameraComponent).set_field_of_view(fov)
    editor.set_level_viewport_camera_info(pos, rot)


def finish(error=None):
    unreal.unregister_slate_post_tick_callback(state['handle'])
    report['error'] = error
    (OUT / 'compare-report.json').write_text(json.dumps(report, indent=2) + '\n')
    unreal.log('ASSETGEN_COMPARE_FINISHED ' + str(error is None))
    unreal.EditorPythonScripting.set_keep_python_script_alive(False)
    unreal.SystemLibrary.quit_editor()


def tick(delta):
    try:
        if time.monotonic() - state['start'] > 600:
            finish('timeout'); return
        if state['task'] is not None:
            if not state['task'].is_task_done(): return
            shot = OUT / (SHOTS[state['i']][0] + '.png')
            # The screenshot is written asynchronously; do not move the camera until the file exists.
            if not shot.is_file() or shot.stat().st_size == 0: return
            report['shots'].append(str(shot))
            state['task'] = None
            if state['i'] + 1 == len(SHOTS): finish(); return
            state['ready'] = time.monotonic() + 8
        elif time.monotonic() >= state['ready']:
            if not state.get('aimed'):
                state['i'] += 1
                aim(state['i'])
                state['aimed'] = True
                state['ready'] = time.monotonic() + 6  # let Lumen and streaming settle on the new view
                return
            state['aimed'] = False
            state['shot_started'] = time.time()
            state['task'] = unreal.AutomationLibrary.take_high_res_screenshot(1600, 900, str(OUT / (SHOTS[state['i']][0] + '.png')), camera=camera, force_game_view=True)
    except Exception:
        finish(traceback.format_exc())


state['handle'] = unreal.register_slate_post_tick_callback(tick)
unreal.log('ASSETGEN_COMPARE_READY')
