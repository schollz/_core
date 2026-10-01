"""Small generated card exports; no copyrighted samples are bundled."""
from pathlib import Path
import array, math, struct

def sample(root, bank, slot, channels=2, rate=44100, companion=True, seconds=1):
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
        path=folder/f'{slot}.{variant}.wav';path.write_bytes(wav)
        flags=120 | (int(not companion)<<12) | (int(companion)<<13) | (int(rate==88200)<<14) | ((channels-1)<<15) | (1<<16)
        starts=[size*i//4 for i in range(4)];stops=starts[1:]+[size]
        info=struct.pack('<IIHB',size,flags,96,4)+struct.pack('<8I',*starts,*stops)+bytes(4)+struct.pack('<3H',1,1,0)+struct.pack('<2H',0,rate//32)
        Path(str(path)+'.info').write_bytes(info)

def create(root):
    root=Path(root)
    for bank in range(1,17):sample(root,bank,15)
    sample(root,1,0,channels=1,rate=88200,companion=False)
    # A companion larger than the entire cache, without a huge primary bank.
    sample(root,16,15,seconds=50)
    (root/'bank2/4.0.wav.info').write_bytes(b'truncated')
    settings=root/'settings';settings.mkdir(exist_ok=True)
    for name in ['brightness-75','amen_cv-unipolar','clock_stop_sync-on']:(settings/name).touch()
    (settings/'sample_cv_mapping').write_text('1voct\n')
    rune=settings/'grimoire/rune1';rune.mkdir(parents=True,exist_ok=True)
    (rune/'effect1-on').touch();(rune/'effect2-off').touch()
    return root

if __name__=='__main__':
    import sys
    create(sys.argv[1])
