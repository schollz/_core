"""Small generated card exports; no copyrighted samples are bundled."""
from pathlib import Path
import array, math, struct

def sample(root, bank, slot, channels=2, rate=44100, companion=False, seconds=1, legacy_server=False):
    folder=Path(root)/f'bank{bank}';folder.mkdir(parents=True,exist_ok=True)
    for variant in range(2 if companion else 1):
        frames=rate*seconds*(8 if variant else 1)
        size=frames*channels*2
        # Circular half-second head/tail padding used by the sample manager.
        period=array.array('h',(int(12000*math.sin(2*math.pi*220*f/(rate*(8 if variant else 1)))) for f in range(rate*(8 if variant else 1)) for _ in range(channels)))
        raw=period.tobytes()*seconds
        pad=rate*channels
        pcm=raw[-pad:]+raw+raw[:pad]
        fmt=struct.pack('<HHIIHH',1,channels,rate,rate*channels*2,channels*2,16)
        wav=b'RIFF'+struct.pack('<I',36+len(pcm))+b'WAVEfmt '+struct.pack('<I',16)+fmt+b'data'+struct.pack('<I',len(pcm))+pcm
        if legacy_server:
            # The old Go exporter read its temporary WAV as raw PCM, retaining
            # a complete 44-byte header in both variants' audio and metadata.
            pcm=wav;size+=44
            wav=b'RIFF'+struct.pack('<I',36+len(pcm))+b'WAVEfmt '+struct.pack('<I',16)+fmt+b'data'+struct.pack('<I',len(pcm))+pcm
        path=folder/f'{slot}.{variant}.wav';path.write_bytes(wav)
        flags=120 | (0<<12) | (1<<13) | (int(rate==88200)<<14) | ((channels-1)<<15) | (1<<16)
        starts=[size*i//(4*channels*2)*(channels*2) for i in range(4)];stops=starts[1:]+[size]
        info=struct.pack('<IIHB',size,flags,96,4)+struct.pack('<8I',*starts,*stops)+bytes(4)+struct.pack('<3H',1,1,0)+struct.pack('<2H',0,rate//32)
        Path(str(path)+'.info').write_bytes(info)

def create(root):
    root=Path(root)
    for bank in range(1,17):sample(root,bank,15)
    sample(root,1,0,channels=1,rate=88200,companion=False)
    # A longer resident primary exercises random reads without any companion cache.
    sample(root,16,15,seconds=50)
    (root/'bank2/4.0.wav.info').write_bytes(b'truncated')
    for slot,(channels,rate) in enumerate([(1,44100),(2,44100),(1,88200),(2,88200)]):
        sample(root,3,slot,channels=channels,rate=rate,legacy_server=True)
    # Corrupt companions are ignored; a corrupt primary still rejects its entry.
    sample(root,4,0,legacy_server=True,companion=True)
    path=root/'bank4/0.1.wav';wav=bytearray(path.read_bytes());wav[44:48]=b'NOPE';path.write_bytes(wav)
    sample(root,4,1,legacy_server=True,companion=True)
    path=root/'bank4/1.0.wav';wav=bytearray(path.read_bytes());wav[0:4]=b'NOPE';path.write_bytes(wav)
    # Stale companion metadata cannot reject an otherwise valid primary.
    sample(root,4,2,legacy_server=True,companion=True)
    path=root/'bank4/2.1.wav.info';info=bytearray(path.read_bytes());info[5]^=0x40;path.write_bytes(info)
    sample(root,4,3,legacy_server=True,companion=True)
    path=root/'bank4/3.1.wav.info';info=bytearray(path.read_bytes());struct.pack_into('<I',info,0,struct.unpack_from('<I',info)[0]+4);path.write_bytes(info)
    settings=root/'settings';settings.mkdir(exist_ok=True)
    for name in ['brightness-75','amen_cv-unipolar','clock_stop_sync-on']:(settings/name).touch()
    (settings/'sample_cv_mapping').write_text('1voct\n')
    rune=settings/'grimoire/rune1';rune.mkdir(parents=True,exist_ok=True)
    (rune/'effect1-on').touch();(rune/'effect2-off').touch()
    return root

if __name__=='__main__':
    import sys
    create(sys.argv[1])
