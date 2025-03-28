import os
import sys
import datetime
import subprocess


def find_old_folders(root_folder, days=30):
    old_folders = []
    cutoff_date = datetime.datetime.now() - datetime.timedelta(days=days)

    for dirpath, dirnames, filenames in os.walk(root_folder):
        all_files_old = True
        for filename in filenames:
            file_path = os.path.join(dirpath, filename)
            file_mtime = datetime.datetime.fromtimestamp(os.path.getmtime(file_path))
            if file_mtime > cutoff_date:
                all_files_old = False
                break
        if all_files_old and filenames:  # Ensure it's not an empty folder
            old_folders.append(dirpath)

    return old_folders


def remove_folders(folders):
    for folder in folders:
        print(f"Removing folder: {folder}")
        try:
            subprocess.run(["rm", "-rf", folder], check=True)
        except subprocess.CalledProcessError as e:
            print(f"Error removing folder {folder}: {e}")


if __name__ == "__main__":
    if len(sys.argv) != 2:
        print("Usage: python script.py <root_folder>")
        sys.exit(1)

    root_folder = sys.argv[1]
    old_folders = find_old_folders(root_folder)

    if old_folders:
        remove_folders(old_folders)
    else:
        print("No folders found with all files older than 30 days.")
