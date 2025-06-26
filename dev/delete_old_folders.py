#!/usr/bin/env -S uv --quiet run --script
# /// script
# requires-python = ">=3.11"
# dependencies = [
#      "rich",
#      "click",
# ]
# ///
"""
Modern folder cleanup utility with colored output and improved UX.
Analyzes and optionally deletes old folders based on their newest file modification time.
"""

import os
import shutil
import sys
from datetime import datetime, timedelta
from pathlib import Path
from typing import Dict, Optional, Tuple

import click
from rich.console import Console
from rich.progress import Progress, SpinnerColumn, TextColumn
from rich.table import Table
from rich.panel import Panel
from rich.prompt import Confirm
from rich import box

console = Console()


class FolderAnalyzer:
    """Handles folder analysis and cleanup operations."""

    def __init__(self, base_directory: Path):
        self.base_directory = base_directory
        self.console = console

    @staticmethod
    def format_filesize(size: int) -> str:
        """Convert bytes to human-readable format."""
        for unit in ["B", "KB", "MB", "GB", "TB"]:
            if size < 1024:
                return f"{size:.1f} {unit}"
            size /= 1024
        return f"{size:.1f} PB"

    @staticmethod
    def format_age(age: timedelta) -> str:
        """Convert timedelta to human-readable format."""
        if age.days > 0:
            return f"{age.days} day{'s' if age.days != 1 else ''}"
        elif age.seconds > 3600:
            hours = age.seconds // 3600
            return f"{hours} hour{'s' if hours != 1 else ''}"
        elif age.seconds > 60:
            minutes = age.seconds // 60
            return f"{minutes} minute{'s' if minutes != 1 else ''}"
        else:
            return f"{age.seconds} second{'s' if age.seconds != 1 else ''}"

    def get_folder_size(self, folder: Path) -> int:
        """Calculate total size of a folder."""
        total_size = 0
        try:
            for file_path in folder.rglob("*"):
                if file_path.is_file():
                    try:
                        total_size += file_path.stat().st_size
                    except (OSError, FileNotFoundError):
                        # Skip files we can't access
                        continue
        except (OSError, PermissionError):
            # Skip folders we can't access
            pass
        return total_size

    def get_newest_file_time(self, directory: Path) -> Optional[float]:
        """Find the newest file modification time in a directory."""
        newest_time = None
        try:
            for file_path in directory.rglob("*"):
                if file_path.is_file():
                    try:
                        file_time = file_path.stat().st_mtime
                        if newest_time is None or file_time > newest_time:
                            newest_time = file_time
                    except (OSError, FileNotFoundError):
                        continue
        except (OSError, PermissionError):
            pass
        return newest_time

    def analyze_folders(self) -> Dict[str, Tuple[timedelta, int]]:
        """Analyze all folders in the base directory."""
        folder_data = {}

        # Get all subdirectories
        try:
            folders = [
                f
                for f in self.base_directory.iterdir()
                if f.is_dir() and not f.name.startswith(".")
            ]
        except (OSError, PermissionError) as e:
            self.console.print(f"[red]Error accessing directory: {e}[/red]")
            return {}

        if not folders:
            self.console.print(
                "[yellow]No folders found in the specified directory.[/yellow]"
            )
            return {}

        self.console.print(f"[blue]Analyzing {len(folders)} folders...[/blue]")

        with Progress(
            SpinnerColumn(),
            TextColumn("[progress.description]{task.description}"),
            console=self.console,
            transient=True,
        ) as progress:
            task = progress.add_task("Scanning folders...", total=len(folders))

            for folder in folders:
                progress.update(
                    task, description=f"Analyzing [cyan]{folder.name}[/cyan]..."
                )

                newest_time = self.get_newest_file_time(folder)
                if newest_time is not None:
                    age = datetime.now() - datetime.fromtimestamp(newest_time)
                    size = self.get_folder_size(folder)
                    folder_data[folder.name] = (age, size)

                progress.advance(task)

        return folder_data

    def display_analysis(
        self, folder_data: Dict[str, Tuple[timedelta, int]], age_threshold: timedelta
    ) -> Tuple[int, int]:
        """Display folder analysis in a nice table."""
        if not folder_data:
            return 0, 0

        table = Table(
            box=box.ROUNDED,
            show_header=True,
            header_style="bold magenta",
        )

        table.add_column("Folder Name", style="cyan", no_wrap=False)
        table.add_column("Age", justify="right", style="yellow")
        table.add_column("Size", justify="right", style="green")
        table.add_column("Status", justify="center")

        total_old_folders = 0
        total_size_to_delete = 0

        # Sort by age (oldest first)
        sorted_folders = sorted(
            folder_data.items(), key=lambda x: x[1][0], reverse=True
        )

        for folder_name, (age, size) in sorted_folders:
            age_str = self.format_age(age)
            size_str = self.format_filesize(size)

            if age > age_threshold:
                status = "[red]  OLD[/red]"
                total_old_folders += 1
                total_size_to_delete += size
                row_style = "dim"
            else:
                status = "[green]✓ KEEP[/green]"
                row_style = None

            table.add_row(folder_name, age_str, size_str, status, style=row_style)

        self.console.print(table)

        # Summary panel
        if total_old_folders > 0:
            summary_text = (
                f"[red]Folders to delete:[/red] {total_old_folders}\n"
                f"[red]Space to free:[/red] {self.format_filesize(total_size_to_delete)}"
            )
            self.console.print(
                Panel(summary_text, title="📈 Summary", border_style="red")
            )
        else:
            self.console.print(
                Panel(
                    "[green]No folders exceed the age threshold![/green]",
                    title="Summary",
                    border_style="green",
                )
            )

        return total_old_folders, total_size_to_delete

    def delete_old_folders(
        self,
        folder_data: Dict[str, Tuple[timedelta, int]],
        age_threshold: timedelta,
        dry_run: bool = False,
    ) -> int:
        """Delete folders that exceed the age threshold."""
        deleted_size = 0
        deleted_count = 0

        old_folders = [
            (name, age, size)
            for name, (age, size) in folder_data.items()
            if age > age_threshold
        ]

        if not old_folders:
            return 0

        action = "Would delete" if dry_run else "Deleting"

        with Progress(
            SpinnerColumn(),
            TextColumn("[progress.description]{task.description}"),
            console=self.console,
        ) as progress:
            task = progress.add_task(f"{action} folders...", total=len(old_folders))

            for folder_name, age, size in old_folders:
                folder_path = self.base_directory / folder_name

                progress.update(
                    task, description=f"{action} [red]{folder_name}[/red]..."
                )

                try:
                    if not dry_run:
                        shutil.rmtree(folder_path)
                        self.console.print(
                            f"[red]Deleted:[/red] {folder_name} "
                            f"([dim]{self.format_filesize(size)}, {self.format_age(age)} old[/dim])"
                        )
                    else:
                        self.console.print(
                            f"[yellow]Would delete:[/yellow] {folder_name} "
                            f"([dim]{self.format_filesize(size)}, {self.format_age(age)} old[/dim])"
                        )

                    deleted_size += size
                    deleted_count += 1

                except Exception as e:
                    self.console.print(
                        f"[red]Failed to delete {folder_name}: {e}[/red]"
                    )

                progress.advance(task)

        return deleted_size


@click.command()
@click.argument(
    "directory", type=click.Path(exists=True, file_okay=False, dir_okay=True)
)
@click.option(
    "--age",
    "-a",
    type=int,
    default=30,
    help="Age threshold in days for folder deletion.",
    show_default=True,
)
@click.option(
    "--delete",
    "-d",
    is_flag=True,
    help="Actually delete folders (without this flag, it's a dry run).",
)
@click.option(
    "--yes", "-y", is_flag=True, help="Skip confirmation prompt when deleting."
)
@click.version_option(version="2.0.0", prog_name="folder-cleanup")
def main(directory: str, age: int, delete: bool, yes: bool):
    """
    Modern folder cleanup utility

    Analyzes folders in DIRECTORY and optionally deletes those older than AGE days.
    Age is determined by the newest file modification time in each folder.

    Examples:

        folder-cleanup /path/to/cleanup --age 30        # Dry run, 30 days threshold

        folder-cleanup /path/to/cleanup --delete --age 7   # Delete folders older than 7 days

        folder-cleanup /path/to/cleanup -d -y -a 14    # Delete without confirmation
    """
    console.print(
        Panel.fit(
            "[bold blue]Folder Cleanup Utility v2.0[/bold blue]\n"
            "[dim]Modern folder analysis and cleanup tool[/dim]",
            border_style="blue",
        )
    )

    try:
        base_path = Path(directory).resolve()
        analyzer = FolderAnalyzer(base_path)
        age_threshold = timedelta(days=age)

        console.print(f"[blue]Target directory:[/blue] {base_path}")
        console.print(
            f"[blue]Age threshold:[/blue] {analyzer.format_age(age_threshold)}"
        )
        console.print(f"[blue]Mode:[/blue] {'DELETE' if delete else 'DRY RUN'}")
        console.print()

        # Analyze folders
        folder_data = analyzer.analyze_folders()

        if not folder_data:
            console.print("[yellow]No folders to analyze. Exiting.[/yellow]")
            return

        # Display analysis
        old_count, total_size = analyzer.display_analysis(folder_data, age_threshold)

        if old_count == 0:
            return

        # Handle deletion
        if delete:
            if not yes:
                console.print()
                if not Confirm.ask(
                    f"[red]⚠️  Delete {old_count} folders "
                    f"({analyzer.format_filesize(total_size)})?[/red]"
                ):
                    console.print("[yellow]Operation cancelled.[/yellow]")
                    return

            console.print()
            deleted_size = analyzer.delete_old_folders(
                folder_data, age_threshold, dry_run=False
            )

            console.print()
            console.print(
                Panel(
                    f"[green]Cleanup completed![/green]\n"
                    f"[green]Space freed:[/green] {analyzer.format_filesize(deleted_size)}",
                    title="Success",
                    border_style="green",
                )
            )
        else:
            console.print()
            console.print(
                Panel(
                    "[yellow]This was a dry run. Use --delete to actually remove folders.[/yellow]",
                    title="ℹInfo",
                    border_style="yellow",
                )
            )

    except KeyboardInterrupt:
        console.print("\n[yellow]Operation cancelled by user.[/yellow]")
        sys.exit(1)
    except Exception as e:
        console.print(f"[red]Unexpected error: {e}[/red]")
        sys.exit(1)


if __name__ == "__main__":
    main()
