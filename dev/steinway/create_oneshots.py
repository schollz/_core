import os
import shutil

# List of MIDI note values
midi_notes = [60, 62, 63, 65, 67, 69, 70, 72, 74, 75, 77, 79, 81, 82, 84, 86]

# Source file and destination folder
destination_folder = "cdorian"

# Create destination folder if it does not exist
if not os.path.exists(destination_folder):
    os.makedirs(destination_folder)

# Copy the file for each MIDI note
for midi_val in midi_notes:
    source_file = f"{midi_val}.3.3.1.0.wav"
    destination_path = os.path.join(destination_folder, source_file)
    # replace .wav with .ogg
    destination_path = destination_path.replace(".wav", ".ogg")
    # now convert the wave file to ogg using sox
    os.system(f"sox {source_file} {destination_path}")
    # add metadata comment that says "oneshot"
    os.system(f"vorbiscomment -w -t 'comment=oneshot,singleslice' {destination_path}")
print("All files copied successfully.")
