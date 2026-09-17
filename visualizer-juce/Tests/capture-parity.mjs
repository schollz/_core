// Optional maintainer tool. Captures expectations from the unchanged browser
// implementation; neither ordinary builds nor native tests require Node.
import { readFileSync, writeFileSync } from 'node:fs';
import { resolve } from 'node:path';
import { stripTypeScriptTypes } from 'node:module';
import { createHash } from 'node:crypto';
const source = resolve(process.argv[2] ?? '../visualizer');
const output = process.argv[3] ?? 'Tests/parity.json';
const strip = path => stripTypeScriptTypes(readFileSync(resolve(source,path),'utf8')).replace(/^import .*$/gm,'').replace(/export /g,'');
const makeSpectrum = new Function('Buffer', strip('build/spectrum.ts')+';return makeSpectrum;')(Buffer);
const makeWaveform = new Function('Buffer','makeSpectrum',strip('build/waveform.ts')+';return makeWaveform;')(Buffer,makeSpectrum);
const results=[];
for(const rate of [44100,88200])for(const channels of [1,2]){
  const stride=channels*2,frames=rate*2,wav=Buffer.alloc(44+frames*stride);
  wav.write('RIFF');wav.writeUInt32LE(wav.length-8,4);wav.write('WAVEfmt ',8);wav.writeUInt32LE(16,16);wav.writeUInt16LE(1,20);wav.writeUInt16LE(channels,22);wav.writeUInt32LE(rate,24);wav.writeUInt32LE(rate*stride,28);wav.writeUInt16LE(stride,32);wav.writeUInt16LE(16,34);wav.write('data',36);wav.writeUInt32LE(frames*stride,40);
  for(let i=0;i<frames;i++)for(let ch=0;ch<channels;ch++){
    let value=Math.round(16000*Math.sin(2*Math.PI*1000*(i-rate/2)/rate));if(ch)value=-value;wav.writeInt16LE(value,44+i*stride+ch*2);
  }
  const info=Buffer.alloc(29),size=rate*stride;info.writeUInt32LE(size);info.writeUInt32LE(120|(1<<13)|((rate/44100-1)<<14)|((channels-1)<<15)|(1<<16),4);info[10]=2;info.writeInt32LE(0,11);info.writeInt32LE(size/4,15);info.writeInt32LE(size/4,19);info.writeInt32LE(size,23);
  const w=makeWaveform(wav,info,0,0);
  const peaks=Buffer.alloc(w.peaks.reduce((n,c)=>n+c.length*2,0));let offset=0;for(const channel of w.peaks)for(const x of channel){peaks.writeInt16LE(x,offset);offset+=2;}
  results.push({rate,channels,duration:w.duration,slices:w.slices,peaksSHA256:createHash('sha256').update(peaks).digest('hex'),spectrum:w.spectrum});
}
writeFileSync(output,JSON.stringify({source:'visualizer/build/waveform.ts and spectrum.ts',cases:results},null,2)+'\n');
console.log(`Captured ${results.length} browser parity cases in ${output}`);
