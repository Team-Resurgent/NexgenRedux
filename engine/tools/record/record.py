"""Records every demo, with the sound, as a video. Windows only; nothing is shown or heard meanwhile.

    python tools/record/record.py <nexgen.exe> <video.mp4> [seconds for each demo] [screen]

Needs ffmpeg. The app itself hands over each frame and what is heard in it, so none is missed whatever the
machine can keep up with. Beside the video goes a .json of when each demo began.
"""
import ctypes
import json
import os
import subprocess
import sys

PIPE = r'\\.\pipe\nexgen-record-%d' % os.getpid()
# RECORD_SIZE=1920x1080 for another size.
WIDTH, HEIGHT = (int(n) for n in os.environ.get('RECORD_SIZE', '1280x720').split('x'))


def main(app, video, seconds, screen):
    kernel = ctypes.WinDLL('kernel32', use_last_error=True)
    kernel.CreateNamedPipeW.restype = ctypes.c_void_p
    pipe = ctypes.c_void_p(kernel.CreateNamedPipeW(PIPE, 1, 0, 1, 0, 1 << 24, 0, None))  # to be read, as bytes
    # Not called PIPE.pcm, which would be a pipe too.
    pcm = os.path.abspath(video) + '.pcm'
    game = subprocess.Popen([app, '--hidden', '--size', str(WIDTH), str(HEIGHT), '--demo', seconds, '--record', PIPE, '--record-sound', pcm] + (['--screen', screen] if screen else []),
                            cwd=os.path.dirname(os.path.abspath(app)), stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True)
    kernel.ConnectNamedPipe(pipe, None)
    picture = video + '.part.mp4'
    encoder = subprocess.Popen(['ffmpeg', '-v', 'error', '-y', '-f', 'rawvideo', '-pix_fmt', 'rgba', '-s', '%dx%d' % (WIDTH, HEIGHT), '-r', '60', '-i', '-',
                                '-pix_fmt', 'yuv420p', '-c:v', 'libx264', '-preset', 'veryfast', '-crf', '17', picture], stdin=subprocess.PIPE)
    size, buffer, got = 1 << 22, ctypes.create_string_buffer(1 << 22), ctypes.c_ulong()
    while kernel.ReadFile(pipe, buffer, size, ctypes.byref(got), None) and got.value:
        encoder.stdin.write(buffer.raw[:got.value] if got.value < size else buffer)
    encoder.stdin.close()
    encoder.wait()
    kernel.CloseHandle(pipe)
    log = game.communicate()[0]
    marks = [(l.split('demo: ')[1].split()[0], int(l.split(' frame ')[1]) / 60) for l in log.splitlines() if 'demo: ' in l]
    json.dump(marks, open(os.path.splitext(video)[0] + '.json', 'w'))
    subprocess.run(['ffmpeg', '-v', 'error', '-y', '-i', picture, '-f', 's16le', '-ar', '48000', '-ac', '2', '-i', pcm, '-c:v', 'copy',
                    '-c:a', 'aac', '-b:a', '160k', '-movflags', '+faststart', video], check=True)
    os.remove(picture)
    os.remove(pcm)
    print(log.strip().splitlines()[-1] if log else 'no output')
    return game.returncode


if __name__ == '__main__':
    if len(sys.argv) < 3:
        sys.exit(__doc__)
    sys.exit(main(sys.argv[1], sys.argv[2], sys.argv[3] if len(sys.argv) > 3 else '8', sys.argv[4] if len(sys.argv) > 4 else None))
