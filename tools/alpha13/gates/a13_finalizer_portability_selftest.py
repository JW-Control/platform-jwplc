#!/usr/bin/env python3
"""Regresión de Python explícito, Git LF/CRLF y política HEAD.

Los commits Git de prueba se crean solo bajo TemporaryDirectory, NUNCA
en platform-jwplc. No hay fetch, push ni modificaciones del producto.
"""
from __future__ import annotations

import pathlib
import subprocess
import sys
import tempfile


def call(repo: pathlib.Path, *args: str, check: bool = True) -> subprocess.CompletedProcess[bytes]:
    result = subprocess.run(["git", "-C", str(repo), *args], stdout=subprocess.PIPE,
                            stderr=subprocess.PIPE, check=False)
    if check and result.returncode != 0:
        raise AssertionError(f"git {args!r} failed: {result.stderr.decode(errors='replace')}")
    return result


def assert_case(ok: bool, key: str) -> None:
    if not ok:
        raise AssertionError(key)
    print(f"{key}=PASS", flush=True)


def test_git_and_crlf() -> None:
    with tempfile.TemporaryDirectory(prefix="jwplc_portability_") as path:
        repo = pathlib.Path(path)
        call(repo, "init", "-q")
        for key, value in (("user.name", "JWPLC Test"),
                           ("user.email", "jwplc-test@invalid.example"),
                           ("core.autocrlf", "true"),
                           ("core.safecrlf", "false")):
            call(repo, "config", key, value)
        fn = repo / "runner.py"
        original = b"print('jwplc')\n"
        fn.write_bytes(original)
        call(repo, "add", "--", "runner.py")
        call(repo, "commit", "-qm", "test fixture")
        first = call(repo, "rev-parse", "HEAD").stdout.decode().strip()
        head_blob = call(repo, "show", "HEAD:runner.py").stdout
        assert_case(head_blob == original, "GIT_FIXTURE_LF_BLOB")

        fn.write_bytes(original.replace(b"\n", b"\r\n"))
        assert_case(fn.read_bytes() != head_blob, "GIT_CRLF_RAW_BYTES_DIFFER")
        assert_case(call(repo, "diff", "--quiet", "HEAD", "--", "runner.py",
                         check=False).returncode == 0,
                    "GIT_CRLF_WORKTREE_LOGICALLY_CLEAN")
        assert_case(call(repo, "diff", "--cached", "--quiet", "HEAD", "--", "runner.py",
                         check=False).returncode == 0,
                    "GIT_CRLF_INDEX_CLEAN")
        index_sha = call(repo, "rev-parse", ":runner.py").stdout.strip()
        head_sha = call(repo, "rev-parse", "HEAD:runner.py").stdout.strip()
        assert_case(index_sha == head_sha, "GIT_CRLF_INDEX_EQUALS_HEAD_BLOB")

        fn.write_bytes(b"print('different')\r\n")
        assert_case(call(repo, "diff", "--quiet", "HEAD", "--", "runner.py",
                         check=False).returncode == 1,
                    "GIT_NON_EOL_MUTATION_REJECTED")
        fn.write_bytes(original.replace(b"\n", b"\r\n"))
        (repo / "policy.txt").write_text("Only tooling.\n", encoding="utf-8")
        call(repo, "add", "--", "policy.txt")
        call(repo, "commit", "-qm", "tooling only")
        second = call(repo, "rev-parse", "HEAD").stdout.decode().strip()
        assert_case(first != second, "GIT_TOOLING_HEAD_ADVANCED")
        assert_case(call(repo, "merge-base", "--is-ancestor", first, second,
                         check=False).returncode == 0,
                    "GIT_HEAD_ANCESTRY_TEST")
        dirty_paths = set(call(repo, "diff", "--name-only").stdout.decode().splitlines())
        assert_case(dirty_paths == set(), "GIT_CRLF_NOT_FALSE_DIRTY")
        changed_paths = set(call(repo, "diff", "--name-only", first, second).stdout.decode().splitlines())
        assert_case(changed_paths == {"policy.txt"}, "GIT_REMOTE_TOOLING_SCOPE_EXACT")


def test_python_launcher() -> None:
    path = pathlib.Path(sys.executable)
    assert_case(path.is_file(), "PYTHON_EXECUTABLE_IS_SINGLE_REAL_FILE")
    assert_case(sys.version_info >= (3, 11), "PYTHON_311_OR_NEWER")
    argv = [str(path), "-B", "-c", "print('JWPLC_PYTHON_OK')"]
    result = subprocess.run(argv, stdout=subprocess.PIPE, stderr=subprocess.PIPE,
                            check=False)
    assert_case(result.returncode == 0 and result.stdout.strip() == b"JWPLC_PYTHON_OK",
                "PYTHON_DIRECT_EXE_INVOCATION")


if __name__ == "__main__":
    test_python_launcher()
    test_git_and_crlf()
    print("A13_FINALIZER_PORTABILITY_SELFTEST=PASS", flush=True)
