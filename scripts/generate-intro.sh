#!/usr/bin/env bash
set -euo pipefail

# Regenerate the bundled intro from the wallpaper; optionally choose an output.
script_dir=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)
project_dir=$(dirname -- "$script_dir")
output=${1:-"$project_dir/backgrounds/intros/1-WALLPAPER00.mp4"}
temporary_video=$(mktemp --suffix=.mp4)
trap 'rm -f -- "$temporary_video"' EXIT

# Select bright neon colors, leaving the illustration and dark outlines alone.
# Blur only the mask edges so the glow follows the lamps when they blink.
neon="if(gt(g(X,Y),170)*gt(g(X,Y),1.35*r(X,Y))*gt(g(X,Y),1.15*b(X,Y))+gt(r(X,Y),210)*gt(b(X,Y),100)*gt(r(X,Y),1.5*g(X,Y))+gt(g(X,Y),210)*gt(b(X,Y),210)*lt(r(X,Y),150),255,0)"
blink="between(t,3.4,3.55)+between(t,3.8,4.03)+between(t,4.42,4.55)+between(t,4.8,5.1)"

filters="
[0:v]format=gbrp,split=2[image][mask_source];
[image]zoompan=z='1+0.04*pow(max(0,1-on/96),3)':x='iw/2-iw/zoom/2':y='ih/2-ih/zoom/2':d=180:s=3840x2160:fps=30,
  fade=t=in:st=0:d=0.6,split=2[normal][dim_source];
[dim_source]lutrgb=r='val*0.35':g='val*0.35':b='val*0.35'[dim];
[mask_source]scale=1920:1080,
  geq=r='$neon':g='$neon':b='$neon',gblur=sigma=2,
  scale=3840:2160,loop=loop=179:size=1:start=0,setpts=N/(30*TB)[mask];
[normal][dim][mask]maskedmerge=enable='$blink',
  scale=out_color_matrix=bt709:out_range=tv,format=yuv420p[video]
"

ffmpeg -hide_banner -loglevel warning -nostdin -y \
  -i "$project_dir/backgrounds/1-WALLPAPER00.jpg" \
  -filter_complex_threads 2 -filter_complex "$filters" -map '[video]' \
  -frames:v 180 -an -c:v libx264 -preset medium -crf 18 -threads 4 \
  -colorspace bt709 -color_primaries bt709 -color_trc bt709 -color_range tv \
  -movflags +faststart "$temporary_video"

mkdir -p -- "$(dirname -- "$output")"
chmod 644 -- "$temporary_video"
mv -- "$temporary_video" "$output"
