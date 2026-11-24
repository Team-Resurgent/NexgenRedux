"""Records the window while it shows every demo, with the sound, as a video. Windows only.

    python tools/record/record.py <nexgen.exe> <loopback.exe> <video.mp4> [seconds for each demo]

Needs ffmpeg 8 or later. The app knows nothing of being recorded: ffmpeg captures its window, and the
loopback tool whatever the computer is playing, so keep everything else quiet. The window may be
covered, but not minimised.
"""
import os
import subprocess
import sys
import time


def main(app, loopback, video, seconds):
    game = subprocess.Popen([app, '--demo', seconds], stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True)
    time.sleep(1.5)  # its window has to be there
    # The sound starts with the picture, so that they match.
    sound = subprocess.Popen([loopback], stdin=subprocess.PIPE, stdout=subprocess.PIPE)
    capture = 'gfxcapture=window_exe=%s:capture_cursor=false:max_framerate=60,hwdownload,format=bgra' % os.path.basename(app)
    record = subprocess.Popen(
        ['ffmpeg', '-v', 'error', '-y',
         '-thread_queue_size', '1024', '-f', 'lavfi', '-i', capture,
         '-thread_queue_size', '1024', '-f', 's16le', '-ar', '48000', '-ac', '2', '-i', 'pipe:0',
         '-vf', 'setpts=PTS-STARTPTS,scale=1280:720,format=yuv420p', '-r', '60', '-af', 'asetpts=N/SR/TB',
         '-c:v', 'libx264', '-preset', 'veryfast', '-crf', '18', '-c:a', 'aac', '-b:a', '160k', '-movflags', '+faststart', video],
        stdin=sound.stdout)
    # Both ends are ffmpeg's now: when the loopback stops, it finishes the file.
    sound.stdout.close()
    log = game.communicate()[0]
    print(log.strip().splitlines()[-1] if log else 'no output')
    sound.stdin.close()
    sound.wait()
    try:
        record.wait(timeout=20)
    except subprocess.TimeoutExpired:
        record.terminate()
    return game.returncode


if __name__ == '__main__':
    if len(sys.argv) not in (4, 5):
        sys.exit(__doc__)
    sys.exit(main(sys.argv[1], sys.argv[2], sys.argv[3], sys.argv[4] if len(sys.argv) == 5 else '8'))
