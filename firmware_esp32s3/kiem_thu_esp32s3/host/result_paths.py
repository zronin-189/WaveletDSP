"""Keep listenable WAV in results and test sidecars in its data subdirectory."""
from pathlib import Path

RESULTS = Path(__file__).resolve().parents[3] / "results"
DATA = RESULTS / "du_lieu_kiem_thu"


def log_path(path):
    path = Path(path)
    if path.parent.resolve() == RESULTS.resolve():
        path = DATA / path.name
    path.parent.mkdir(parents=True, exist_ok=True)
    return path


def capture_paths(prefix):
    prefix = Path(prefix)
    prefix.parent.mkdir(parents=True, exist_ok=True)
    result = {suffix: log_path(Path(str(prefix) + suffix)) for suffix in (".log", ".raw", ".json")}
    for suffix in ("_slot0_pcm.wav", "_slot1_pcm.wav", "_slot0_listen.wav"):
        result[suffix] = Path(str(prefix) + suffix)
    return result
