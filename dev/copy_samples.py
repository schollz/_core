import os
import time
import subprocess
import shutil

from tqdm import tqdm
import pyudev


def mount_drive(device_path, mount_point):
    os.makedirs(mount_point, exist_ok=True)
    subprocess.run(["sudo", "mount", device_path, mount_point])


def unmount_drive(mount_point):
    subprocess.run(["sudo", "umount", mount_point])
    os.rmdir(mount_point)


def write_file(mount_point):
    file_path = os.path.join(mount_point, "file1.txt")
    with open(file_path, "w") as file:
        file.write("Hello, this is file1.txt\n")


def copytree_with_progress(src, dst):
    total_files = sum(len(files) for _, _, files in os.walk(src))
    with tqdm(total=total_files, desc="Copying files") as pbar:
        for root, dirs, files in os.walk(src):
            for file in files:
                src_file = os.path.join(root, file)
                dst_file = os.path.join(dst, os.path.relpath(src_file, src))
                os.makedirs(os.path.dirname(dst_file), exist_ok=True)
                # copy file using subprocess
                subprocess.run(["cp", src_file, dst_file])
                pbar.update(1)


def copy_folder(mount_point, local_folder):
    copytree_with_progress(local_folder, mount_point)


def main():
    context = pyudev.Context()
    monitor = pyudev.Monitor.from_netlink(context)
    monitor.filter_by(subsystem="block")

    for device in iter(monitor.poll, None):
        print(device.action, device.get("ID_BUS"), device.device_node)
        if device.action == "add" and "ID_BUS" in device:
            if device.get("ID_BUS") == "usb" or device.get("ID_BUS") == "mmc":
                device_path = device.device_node
                mount_point = "/media/" + os.path.basename(device_path)
                mount_drive(device_path, mount_point)
                print(f"Drive {device_path} mounted at {mount_point}")
                time.sleep(1)
                try:
                    copy_folder(mount_point, "starting_samples/")
                except:
                    pass
                unmount_drive(mount_point)
                print(f"Drive {device_path} unmounted")
                time.sleep(1)


if __name__ == "__main__":
    main()
