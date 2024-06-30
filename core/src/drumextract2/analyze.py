import wave
import numpy as np
import matplotlib.pyplot as plt
from scipy.signal import find_peaks
from scipy.signal import savgol_filter


def read_wav(file_path):
    with wave.open(file_path, "rb") as wav_file:
        n_channels = wav_file.getnchannels()
        sampwidth = wav_file.getsampwidth()
        framerate = wav_file.getframerate()
        n_frames = wav_file.getnframes()

        audio_data = wav_file.readframes(n_frames)
        audio_data = np.frombuffer(audio_data, dtype=np.int16)

        if n_channels > 1:
            audio_data = np.reshape(audio_data, (-1, n_channels))

    return audio_data, framerate


def envelope_follower(audio_data, frame_rate, window_size=512, hop_size=128):
    envelope = []
    for i in range(0, len(audio_data) - window_size, hop_size):
        window = audio_data[i : i + window_size]
        envelope.append(np.max(np.abs(window)))
    return np.array(envelope)


def get_peaks(envelope, time_axis):
    # determine the peak height
    peak_height = np.mean(envelope) + np.std(envelope)
    peaks = []
    in_peak = False
    last_peak = -1
    for i, v in enumerate(envelope):
        if v > peak_height and not in_peak and time_axis[i] - last_peak > 0.2:
            peaks.append(i)
            in_peak = True
            last_peak = time_axis[i]
        elif v < peak_height and in_peak:
            in_peak = False
    return np.array(peaks)


def plot_envelope_with_peaks(envelope, frame_rate, hop_size):
    time_axis = np.arange(0, len(envelope) * hop_size, hop_size) / frame_rate
    peaks = get_peaks(envelope, time_axis)
    peaks_time = time_axis[peaks]
    print(peaks_time)
    plt.figure(figsize=(10, 4))
    plt.plot(time_axis, envelope, label="Envelope")
    plt.plot(time_axis[peaks], envelope[peaks], "x", label="Peaks")
    plt.xlabel("Time (s)")
    plt.ylabel("Amplitude")
    plt.title("Envelope Follower with Peaks")
    plt.legend()
    plt.grid()
    plt.show()


# Example usage
file_path = "bombo.wav"  # Replace with your .wav file path
audio_data, frame_rate = read_wav(file_path)
hop_size = 32
window_size = 64
envelope = envelope_follower(
    audio_data, frame_rate, window_size=window_size, hop_size=hop_size
)
plot_envelope_with_peaks(envelope, frame_rate, hop_size)
