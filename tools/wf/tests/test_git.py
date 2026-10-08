"""git status parsing (a stripped " M bn6f" once became "n6f" and failed a record run's commit)."""
from wf.tools.git import porcelain_paths


def test_a_leading_space_is_part_of_the_status_not_the_path() -> None:
    assert porcelain_paths(" M bn6f\0M  symbols/SYMBOLS.md\0?? symbols/db/new.yaml\0") == (
        "bn6f", "symbols/SYMBOLS.md", "symbols/db/new.yaml")
