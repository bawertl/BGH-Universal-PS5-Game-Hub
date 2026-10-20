"""The native build must survive upstream loss without changing its pinned kit."""
from __future__ import annotations

import os
from pathlib import Path
import subprocess

ROOT = Path(__file__).resolve().parents[1]
FETCHER = ROOT / "tooling/fetch_pinned_ui_kit.sh"


def run(*args, env=None, check=True):
    return subprocess.run(args, env=env, capture_output=True, text=True, check=check, timeout=25)


def test_fallback_fetches_exact_commit_not_fork_head(tmp_path):
    source = tmp_path / "source"
    source.mkdir()
    run("git", "init", "-q", "-b", "main", str(source))
    run("git", "-C", str(source), "config", "user.email", "ci@example.invalid")
    run("git", "-C", str(source), "config", "user.name", "CI fixture")
    (source / "Makefile").write_text("PINNED=1\n")
    run("git", "-C", str(source), "add", "Makefile")
    run("git", "-C", str(source), "commit", "-qm", "original pinned kit")
    pin = run("git", "-C", str(source), "rev-parse", "HEAD").stdout.strip()
    (source / "Makefile").write_text("UNREVIEWED_NEW_HEAD=1\n")
    run("git", "-C", str(source), "commit", "-qam", "newer unreviewed kit")
    target = tmp_path / "checkout"
    env = os.environ.copy()
    env["BGH_UI_KIT_PRIMARY"] = (tmp_path / "not-present").as_uri()
    env["BGH_UI_KIT_FALLBACK"] = source.as_uri()
    result = run("bash", str(FETCHER), str(target), pin, env=env)
    assert "verified at EXACT commit" in result.stdout
    assert (target / "Makefile").read_text() == "PINNED=1\n"
    assert run("git", "-C", str(target), "rev-parse", "HEAD").stdout.strip() == pin


def test_both_sources_inaccessible_fails_closed(tmp_path):
    target = tmp_path / "checkout"
    env = os.environ.copy()
    env["BGH_UI_KIT_PRIMARY"] = (tmp_path / "absentA").as_uri()
    env["BGH_UI_KIT_FALLBACK"] = (tmp_path / "absentB").as_uri()
    result = run("bash", str(FETCHER), str(target), "a" * 40, env=env, check=False)
    assert result.returncode != 0
    assert "Neither source provides the PINNED" in result.stderr


def test_invalid_revision_rejected(tmp_path):
    result = run("bash", str(FETCHER), str(tmp_path / "kit"), "main", check=False)
    assert result.returncode != 0
    assert "full 40-character" in result.stderr
