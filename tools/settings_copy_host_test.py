"""Settings descriptions must match the layout constants they describe."""
import re
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
book = (ROOT / "main/apps/app_book.c").read_text()
settings = (ROOT / "main/apps/app_device_settings.c").read_text()

grid = int(re.search(r"#define BOOK_GRID_ROWS (\d+)", book).group(1))
rows = int(re.search(r"#define SHELF_LIST_ROWS (\d+)", book).group(1))
line = re.search(r"descriptions\[\] = \{(.*?)\};", settings).group(1)
dark, minimal, listing = re.findall(r'"([^"]*)"', line)
for text, count in ((dark, grid), (minimal, grid), (listing, rows)):
    assert f"{count} cuốn" in text, (text, count)
assert "dung lượng" not in minimal, minimal  # lưới hiện tiến độ từ 1.5.6 / grid shows progress since 1.5.6
print("settings copy: shelf style descriptions match layout")
