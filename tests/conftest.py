import os
import sys
from pathlib import Path

import pytest

REPO_ROOT = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(REPO_ROOT / "tools"))
sys.path.insert(0, str(REPO_ROOT))


@pytest.fixture(scope="session")
def repo_root() -> Path:
    return REPO_ROOT


@pytest.fixture(scope="session")
def build_dir() -> Path:
    return Path(os.environ.get("MM_BUILD_DIR", REPO_ROOT / "build"))


@pytest.fixture
def scratch_dir(tmp_path: Path) -> Path:
    return tmp_path
