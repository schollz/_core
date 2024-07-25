import os
import shutil
import sys
from datetime import datetime, timedelta
import click


def pretty_print_filesize(size):
    for unit in ["B", "KB", "MB", "GB", "TB"]:
        if size < 1024:
            return f"{size:.2f} {unit}"
        size /= 1024
    return f"{size:.2f} PB"


def pretty_print_age(age):
    if age.days > 0:
        return f"{age.days} days"
    elif age.seconds > 3600:
        return f"{age.seconds // 3600} hours"
    elif age.seconds > 60:
        return f"{age.seconds // 60} minutes"
    else:
        return f"{age.seconds} seconds"


def get_folder_size(folder):
    total_size = 0
    for dirpath, _, filenames in os.walk(folder):
        for f in filenames:
            fp = os.path.join(dirpath, f)
            total_size += os.path.getsize(fp)
    return total_size


def get_newest_file_time(directory):
    newest_time = None
    for root, _, files in os.walk(directory):
        for file in files:
            file_path = os.path.join(root, file)
            file_time = os.path.getmtime(file_path)
            if newest_time is None or file_time > newest_time:
                newest_time = file_time
    return newest_time


def folder_ages(base_directory):
    folder_ages_dict = {}
    # find all the directories in the base directory, non-recursive
    folders = [
        f
        for f in os.listdir(base_directory)
        if os.path.isdir(os.path.join(base_directory, f))
    ]
    for folder in folders:
        folder_path = os.path.join(base_directory, folder)
        newest_file_time = get_newest_file_time(folder_path)
        if newest_file_time is not None:
            folder_ages_dict[folder] = datetime.now() - datetime.fromtimestamp(
                newest_file_time
            )
    return folder_ages_dict


@click.command()
@click.argument("directory")
@click.option("--delete", is_flag=True, help="Delete folders if true.")
@click.option(
    "--age",
    type=int,
    default=30,
    required=False,
    help="Age threshold in days to keep folders.",
)
def main(directory, delete, age):
    if not os.path.isdir(directory):
        print(f"The provided path '{directory}' is not a valid directory.")
        sys.exit(1)

    age_threshold = timedelta(days=age)
    folder_ages_dict = folder_ages(directory)

    total_size_deleted = 0
    for folder, folder_age in folder_ages_dict.items():
        if folder_age > age_threshold:
            folder_path = os.path.join(directory, folder)
            try:
                folder_size = get_folder_size(folder_path)
                total_size_deleted += folder_size
                if delete:
                    print(
                        f"Deleting {folder_path} ({pretty_print_filesize(folder_size)}, {pretty_print_age(folder_age)} old)"
                    )
                    shutil.rmtree(folder_path)
                else:
                    print(
                        f"Would delete {folder_path} ({pretty_print_filesize(folder_size)}, {pretty_print_age(folder_age)} old)"
                    )
            except Exception as e:
                print(f"Failed to delete folder {folder_path}: {e}")
    if delete:
        print(f"Total space freed: {pretty_print_filesize(total_size_deleted)}")
    else:
        print(
            f"Total space that would be freed: {pretty_print_filesize(total_size_deleted)}"
        )


if __name__ == "__main__":
    main()
