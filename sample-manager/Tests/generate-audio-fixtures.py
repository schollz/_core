"""Regenerate synthetic fixtures; Python and ffmpeg are build-time author tools only."""
import json, math, struct, subprocess, wave, zipfile
from pathlib import Path
folder = Path(__file__).parent / 'fixtures'
samples = [round(15000 * math.sin(2*math.pi*440*n/44100)) for n in range(44100)]
with wave.open(str(folder/'import.wav'),'wb') as out:
    out.setparams((1,2,44100,len(samples),'NONE','not compressed'))
    out.writeframes(struct.pack('<'+'h'*len(samples),*samples))
def chunk(tag,data,endian='>'):
    return tag+struct.pack(endian+'I',len(data))+data+b'\0'*(len(data)%2)
# WAV markers use the standard cue and smpl structures, independent of JUCE.
wav = (folder/'import.wav').read_bytes()
cues = struct.pack('<I',2)+b''.join(struct.pack('<II4sIII',i,0,b'data',0,0,n) for i,n in enumerate([0,22050]))
body = wav[8:] + chunk(b'cue ',cues,'<')
(folder/'cue.wav').write_bytes(b'RIFF'+struct.pack('<I',len(body))+body)
loop = struct.pack('<9I',0,0,22676,60,0,0,0,1,0)+struct.pack('<6I',0,0,11025,33074,0,0)
body = wav[8:] + chunk(b'smpl',loop,'<')
(folder/'loop.wav').write_bytes(b'RIFF'+struct.pack('<I',len(body))+body)
comm = struct.pack('>hIh',1,len(samples),16)+bytes.fromhex('400eac44000000000000')
audio = chunk(b'SSND',struct.pack('>II',0,0)+struct.pack('>'+'h'*len(samples),*samples))
base = b'AIFF'+chunk(b'COMM',comm)+chunk(b'ANNO',b'fixture before audio')+audio
# OP-1 APPL appears beyond the old 8000-byte fixed-offset scan.
op = json.dumps({'start':[441*4058,(22050+441)*4058], 'end':[(22050+800)*4058,(44100+800)*4058]}).encode()
for name,extra in [('op1.aif',chunk(b'APPL',b'op-1'+op)),('custom.aiff',chunk(b'COMT',struct.pack('>H I H H',1,0,0,len(b'{"s":[0,0.25],"e":[0.25,1]}'))+b'{"s":[0,0.25],"e":[0.25,1]}'))]:
    body = base+extra
    (folder/name).write_bytes(b'FORM'+struct.pack('>I',len(body))+body)
with zipfile.ZipFile(folder/'import.xrni','w',zipfile.ZIP_DEFLATED) as z:
    z.writestr('Instrument.xml','<RenoiseInstrument><Samples><Sample><FileName>tone.wav</FileName><SliceMarkers><SliceMarker><SamplePosition>0</SamplePosition></SliceMarker><SliceMarker><SamplePosition>22050</SamplePosition></SliceMarker></SliceMarkers></Sample></Samples></RenoiseInstrument>')
    z.write(folder/'import.wav','SampleData/tone.wav')
for ext in ['flac','mp3','ogg']:
    args=['ffmpeg','-hide_banner','-loglevel','error','-y','-i',str(folder/'import.wav')]
    if ext == 'ogg': args+=['-c:a','vorbis','-strict','experimental','-ac','2']+ ['-metadata','artist=[0,0.5]','-metadata','album=[0.5,1]','-metadata','comment=oneshot']
    subprocess.run(args+[str(folder/('import.'+ext))],check=True)
print('Generated independent synthetic import fixtures')
