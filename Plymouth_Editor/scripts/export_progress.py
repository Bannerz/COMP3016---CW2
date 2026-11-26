"""Report approximate export stages to the parent Blender process."""
import json
import os


def report_progress(percent, message):
    path = os.environ.get("PLYMOUTH_EXPORT_PROGRESS")
    if not path:
        return

    # Replace the file in one operation so the UI only sees complete records.
    temporary_path = path + ".tmp"
    try:
        with open(temporary_path, "w", encoding="utf-8") as output:
            json.dump({"percent": int(percent), "message": message}, output)
        os.replace(temporary_path, path)
    except OSError:
        # Loss of a progress update should not interrupt exporting the map.
        pass
