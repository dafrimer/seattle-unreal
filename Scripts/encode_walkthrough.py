"""Encode verified native Unreal walkthrough frames to H.264 MP4 (run on Windows; ffmpeg runs in WSL)."""
import json
import subprocess
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
FFMPEG = '/mnt/h/Projects/seattle-world/.runtime/view/bin/ffmpeg'
FFPROBE = '/mnt/h/Projects/seattle-world/.runtime/view/bin/ffprobe'


def wsl(path):
    resolved = path.resolve()
    return '/mnt/' + resolved.drive[0].lower() + resolved.as_posix()[2:]


def main():
    capture = json.loads((ROOT / 'Evidence/capture.json').read_text())
    frames = sorted((ROOT / 'Evidence/walkthrough-frames').glob('frame-*.png'))
    if not capture['success'] or capture['errors'] or len(frames) != capture['frames']:
        raise ValueError('Native StreetRecord evidence is not a complete successful capture')
    output = ROOT / 'Evidence/seattle-unreal-walkthrough.mp4'
    subprocess.run(['wsl.exe', '-d', 'Ubuntu', '--', FFMPEG, '-hide_banner', '-loglevel', 'error', '-y',
                    '-framerate', str(capture['fps']), '-i', wsl(ROOT / 'Evidence/walkthrough-frames') + '/frame-%06d.png',
                    '-c:v', 'libx264', '-preset', 'slow', '-crf', '18', '-pix_fmt', 'yuv420p', '-movflags', '+faststart',
                    wsl(output)], check=True)
    probe = json.loads(subprocess.run(['wsl.exe', '-d', 'Ubuntu', '--', FFPROBE, '-v', 'error', '-select_streams', 'v:0',
                                       '-count_frames', '-show_entries', 'stream=codec_name,width,height,nb_read_frames,avg_frame_rate',
                                       '-show_entries', 'format=duration', '-of', 'json', wsl(output)],
                                      check=True, capture_output=True, text=True).stdout)
    stream = probe['streams'][0]
    if int(stream['nb_read_frames']) != capture['frames'] or (stream['width'], stream['height']) != (capture['renderWidth'], capture['renderHeight']):
        raise ValueError('Encoded MP4 does not match the verified capture')
    print(json.dumps({'mp4': str(output), 'bytes': output.stat().st_size, 'codec': stream['codec_name'],
                      'frames': int(stream['nb_read_frames']), 'fps': stream['avg_frame_rate'],
                      'durationSeconds': float(probe['format']['duration']), 'size': [stream['width'], stream['height']]}, indent=2))


if __name__ == '__main__':
    main()
