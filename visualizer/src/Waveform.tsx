import { useEffect, useRef } from 'react';
import type { Playback, Waveform as WaveformData } from './types';
import { Playhead } from './playhead';
import { decodeSpectrum, spectrumAt } from './spectrum';
import { isFresh } from './protocol';
import type { ButtonPress } from './midi';

const INACTIVE_WAVEFORM_ALPHA = 0.55;

// Trace a measured portion of a polyline, so the outline draws and undraws.
function trace(ctx: CanvasRenderingContext2D, points: number[][], progress: number) {
  const lengths = points.slice(1).map((point, i) => Math.hypot(point[0] - points[i][0], point[1] - points[i][1]));
  let remaining = lengths.reduce((sum, length) => sum + length, 0) * progress;
  ctx.beginPath(); ctx.moveTo(points[0][0], points[0][1]);
  for (let i = 0; i < lengths.length && remaining > 0; i++) {
    const fraction = Math.min(1, remaining / lengths[i]);
    ctx.lineTo(points[i][0] + (points[i + 1][0] - points[i][0]) * fraction,
      points[i][1] + (points[i + 1][1] - points[i][1]) * fraction);
    remaining -= lengths[i];
  }
  ctx.stroke();
}

export function Waveform({ wave, playback, buttonPress, receivedAt, live }: {
  wave: WaveformData; playback?: Playback; buttonPress?: ButtonPress; receivedAt?: number; live: boolean;
}) {
  const canvas = useRef<HTMLCanvasElement>(null);
  const spectrumCanvas = useRef<HTMLCanvasElement>(null);
  const clock = useRef(new Playhead());
  const current = useRef({ playback, buttonPress, receivedAt, live });
  current.current = { playback, buttonPress, receivedAt, live };
  useEffect(() => {
    if (playback && receivedAt !== undefined) clock.current.update(playback, wave, receivedAt);
  }, [playback, receivedAt, wave]);

  useEffect(() => {
    const node = canvas.current!;
    const ctx = node.getContext('2d')!;
    const spectrumNode = spectrumCanvas.current!;
    const spectrumCtx = spectrumNode.getContext('2d')!;
    const levels = decodeSpectrum(wave.spectrum);
    let spectrumWidth = 0, spectrumHeight = 0;
    let displayed = Array(32).fill(0), lastFrame = performance.now(), lastTrigger = '';
    const reduceMotion = window.matchMedia('(prefers-reduced-motion: reduce)');
    const backing = document.createElement('canvas');
    const base = backing.getContext('2d')!;
    let width = 0, height = 0, raf = 0;
    let uiScale = 1, labelFont = 'monospace';
    let placementKey = '';
    let placement = { side: 1, horizontal: 0, vertical: 0 };
    const top = 12, bottom = 12;
    function resize() {
      const box = node.getBoundingClientRect(), dpr = window.devicePixelRatio || 1;
      width = box.width; height = box.height;
      const spectrumBox = spectrumNode.getBoundingClientRect();
      spectrumWidth = spectrumBox.width; spectrumHeight = spectrumBox.height;
      spectrumNode.width = Math.round(spectrumWidth * dpr);
      spectrumNode.height = Math.round(spectrumHeight * dpr);
      const style = getComputedStyle(node);
      uiScale = parseFloat(style.fontSize) / 16;
      labelFont = style.fontFamily;
      node.width = backing.width = Math.round(width * dpr);
      node.height = backing.height = Math.round(height * dpr);
      base.setTransform(dpr, 0, 0, dpr, 0, 0);
      base.clearRect(0, 0, width, height);
      base.fillStyle = '#fff';
      // One mirrored envelope with finer columns and continuous amplitude.
      // Take the largest absolute peak across channels so stereo cannot cancel.
      const columns = Math.max(1, Math.min(640, Math.floor(width)));
      const amplitude = Math.max(1, height - top - bottom) / 2;
      const middle = top + amplitude;
      const bins = wave.peaks[0].length / 2;
      for (let column = 0; column < columns; column++) {
        const first = Math.floor(column / columns * bins);
        const last = Math.min(bins, Math.max(first + 1, Math.ceil((column + 1) / columns * bins)));
        let peak = 0;
        for (const channel of wave.peaks) {
          for (let b = first; b < last; b++) {
            peak = Math.max(peak, Math.abs(channel[b * 2]), Math.abs(channel[b * 2 + 1]));
          }
        }
        const h = peak / 32768 * amplitude;
        const x = Math.floor(column / columns * width);
        const end = Math.floor((column + 1) / columns * width);
        base.fillRect(x, middle - h, end - x, h * 2);
      }
    }
    const observer = new ResizeObserver(resize); observer.observe(node); observer.observe(spectrumNode); resize();
    function frame() {
      const dpr = window.devicePixelRatio || 1;
      ctx.setTransform(1, 0, 0, 1, 0, 0);
      ctx.clearRect(0, 0, node.width, node.height);
      ctx.globalAlpha = INACTIVE_WAVEFORM_ALPHA; ctx.drawImage(backing, 0, 0); ctx.globalAlpha = 1;
      ctx.setTransform(dpr, 0, 0, dpr, 0, 0);
      // Quiet reference marks, like the geography beneath a radar track.
      ctx.strokeStyle = '#404040'; ctx.lineWidth = 1;
      for (let tick = 0; tick <= 8; tick++) {
        const x = Math.min(width - 1, Math.floor(tick / 8 * width)) + 0.5;
        ctx.beginPath(); ctx.moveTo(x, height - 4); ctx.lineTo(x, height - (tick % 2 ? 7 : 10)); ctx.stroke();
      }
      const { playback: state, buttonPress: press, receivedAt: at, live: connected } = current.current;
      const now = performance.now();
      const valid = connected && isFresh(at, now) && state?.valid && state.bank === wave.bank && state.sample === wave.sample;
      if (valid) {
        const slice = wave.slices[state.slice];
        if (slice) {
          const x = slice.start / wave.duration * width, w = (slice.stop - slice.start) / wave.duration * width;
          if (!state.stopped && !state.muted) {
            // Brighten only the envelope inside the slice; keep its background black.
            ctx.save();
            ctx.beginPath(); ctx.rect(x, 0, w, height); ctx.clip();
            ctx.drawImage(backing, 0, 0, width, height);
            ctx.restore();
          }
          const position = clock.current.value(now);
          if (!state.stopped && !state.muted && position !== null) {
            const px = position / wave.duration * width;
            ctx.strokeStyle = '#fff'; ctx.lineWidth = 3;
            ctx.beginPath();
            ctx.moveTo(Math.floor(px) + 0.5, 0);
            ctx.lineTo(Math.floor(px) + 0.5, height);
            ctx.stroke();
            ctx.fillStyle = '#fff';
            ctx.beginPath();
            ctx.moveTo(px, height - bottom + 1);
            ctx.lineTo(px + 3, height - bottom + 5);
            ctx.lineTo(px, height - bottom + 9);
            ctx.lineTo(px - 3, height - bottom + 5);
            ctx.closePath(); ctx.fill();
          }
          const age = press ? now - press.at : Infinity;
          const pressedSlice = press && press.bank === wave.bank && press.sample === wave.sample ? wave.slices[press.slice] : undefined;
          if (pressedSlice && !state.stopped && !state.muted && position !== null && age >= 0 && age < 480) {
            const key = `${press!.at}:${press!.button}`;
            if (key !== placementKey) {
              placementKey = key;
              // Choose once per press, never once per animation frame.
              placement = { side: Math.random() < 0.5 ? -1 : 1,
                horizontal: Math.random(), vertical: Math.random() };
            }
            // Draw in (140 ms), hold (120 ms), then retract (220 ms).
            const progress = age < 140 ? age / 140 : age < 260 ? 1 : 1 - (age - 260) / 220;
            // Use the same pixel as the playhead so the leader stays attached
            // while the box follows playback, including jumps and reversals.
            const target = Math.floor(position / wave.duration * width) + 0.5;
            const sliceWidth = (pressedSlice.stop - pressedSlice.start) / wave.duration * width;
            const inset = 3 * uiScale;
            const boxWidth = Math.min(width - inset * 2,
              Math.max(40 * uiScale, Math.min(60 * uiScale, Math.round(sliceWidth * 1.8))));
            const boxHeight = Math.min(height - inset * 2, 28 * uiScale);
            const gap = 10 * uiScale + placement.horizontal * Math.min(60 * uiScale, width * 0.15);
            let side = placement.side;
            // Keep the full box visible when the chosen side reaches an edge.
            if (side === -1 && target < boxWidth + gap + inset) side = 1;
            else if (side === 1 && target + boxWidth + gap > width - inset) side = -1;
            const left = Math.max(inset, Math.min(width - boxWidth - inset,
              target + (side === 1 ? gap : -boxWidth - gap)));
            const right = left + boxWidth;
            const y = inset + placement.vertical * Math.max(0, height - boxHeight - inset * 2);
            const join = side === 1 ? left : right;
            const far = join === left ? right : left;
            const tipY = height / 2;
            const joinY = y + boxHeight / 2;
            ctx.strokeStyle = '#fff'; ctx.lineWidth = Math.max(1, uiScale);
            trace(ctx, [[target, tipY], [join, joinY], [join, y],
              [far, y], [far, y + boxHeight], [join, y + boxHeight], [join, joinY]], progress);
            if (progress >= 1) {
              ctx.font = `${14 * uiScale}px ${labelFont}`; ctx.textAlign = 'center'; ctx.textBaseline = 'middle';
              ctx.fillStyle = '#fff';
              ctx.fillText(String(press!.button).padStart(2, '0'), left + boxWidth / 2, y + boxHeight / 2);
            }
          }
        }
      }
      const spectrumPosition = valid && !state.stopped && !state.muted ? clock.current.value(now) : null;
      const target = spectrumAt(wave.spectrum, levels, spectrumPosition);
      const trigger = state ? `${state.bank}:${state.sample}:${state.trigger}:${state.estimated}` : '';
      const reset = trigger !== lastTrigger || spectrumPosition === null || reduceMotion.matches;
      const dt = Math.min(100, Math.max(0, now - lastFrame));
      displayed = target.map((value, band) => reset ? value : displayed[band] + (value - displayed[band]) *
        (1 - Math.exp(-dt / (value > displayed[band] ? 35 : 140))));
      lastFrame = now; lastTrigger = trigger;
      spectrumCtx.setTransform(dpr, 0, 0, dpr, 0, 0);
      spectrumCtx.clearRect(0, 0, spectrumWidth, spectrumHeight);
      const gap = Math.max(2, Math.min(5, spectrumWidth / 160));
      const step = spectrumWidth / 32;
      for (let band = 0; band < 32; band++) {
        const barHeight = Math.max(1, displayed[band] * Math.max(0, spectrumHeight - 2));
        spectrumCtx.fillStyle = '#fff';
        spectrumCtx.globalAlpha = INACTIVE_WAVEFORM_ALPHA * Math.max(0, Math.min(1, displayed[band]));
        spectrumCtx.fillRect(Math.floor(band * step), spectrumHeight - barHeight,
          Math.max(1, Math.floor(step - gap)), barHeight);
      }
      spectrumCtx.globalAlpha = 1;
      raf = requestAnimationFrame(frame);
    }
    frame();
    return () => { observer.disconnect(); cancelAnimationFrame(raf); };
  }, [wave]);
  const active = live && isFresh(receivedAt, performance.now()) && playback?.valid &&
    playback.bank === wave.bank && playback.sample === wave.sample && wave.slices[playback.slice];
  return <>
    <canvas ref={canvas} className="waveform" role="img"
      aria-label={`Waveform for bank ${wave.bank + 1}, sample ${wave.sample + 1}, ${wave.slices.length} slices. Active slice ${active ? playback!.slice + 1 : 'unavailable'}.`} />
    <div className="spectrum-panel">
      <canvas ref={spectrumCanvas} className="spectrum" role="img"
        aria-label="Source audio frequency spectrum, 32 bands from 50 Hz to 16 kHz; device effects are not included" />
      <div className="spectrum-axis"><span>50 Hz</span><span>1 kHz</span><span>16 kHz</span></div>
    </div>
  </>;
}
