"""The install freeze: installs, renames and commits wait while it is on and go on after the
thaw; reading bn6f goes on; starting one waits for the install under way."""
import threading

import pytest

from wf.tools import locks
from wf.tools.locks import BN6F, MAIN_TREE, freeze_installs, held, install_freeze, thaw_installs


@pytest.fixture(autouse=True)
def quick_polls(monkeypatch: pytest.MonkeyPatch) -> None:
    monkeypatch.setattr(locks, "FREEZE_POLL_SECONDS", 0.01)


def install_in_background(done: threading.Event) -> threading.Thread:
    def install() -> None:
        with held(MAIN_TREE):
            done.set()
    worker = threading.Thread(target=install)
    worker.start()
    return worker


def test_an_install_waits_out_the_freeze() -> None:
    _ = freeze_installs("the db rewrite")
    installed = threading.Event()
    worker = install_in_background(installed)
    assert not installed.wait(0.3)
    thawed = thaw_installs()
    assert thawed is not None and thawed.reason == "the db rewrite"
    assert installed.wait(5)
    worker.join()
    assert install_freeze() is None


def test_reading_bn6f_goes_on_during_a_freeze() -> None:
    _ = freeze_installs("the regroup")
    with held(BN6F, exclusive=False):
        pass


def test_a_bare_flag_file_is_a_freeze() -> None:
    locks.FREEZE.parent.mkdir(parents=True)
    locks.FREEZE.touch()
    freeze = install_freeze()
    assert freeze is not None and freeze.reason == "no reason given"


def test_a_freeze_starts_once_the_install_under_way_is_done() -> None:
    finish = threading.Event()
    holding = threading.Event()

    def long_install() -> None:
        with held(MAIN_TREE):
            holding.set()
            _ = finish.wait(5)
    worker = threading.Thread(target=long_install)
    worker.start()
    assert holding.wait(5)
    frozen = threading.Event()
    freezer = threading.Thread(target=lambda: (freeze_installs("the regroup"), frozen.set()))
    freezer.start()
    assert not frozen.wait(0.3)   # the flag is up, but the install under way is not done
    assert install_freeze() is not None
    finish.set()
    assert frozen.wait(5)
    worker.join()
    freezer.join()
