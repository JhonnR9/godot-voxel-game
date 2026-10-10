"""Regression checks for error detection, timeouts, discovery, and registry fixtures."""

from contextlib import redirect_stdout
import io
import json
import os
from pathlib import Path
import sys
import tempfile
import unittest
from unittest.mock import patch

import run_tests


class RunnerTests(unittest.TestCase):
    def run_child(self, source: str, timeout: float = 3):
        with tempfile.TemporaryDirectory() as directory, redirect_stdout(io.StringIO()):
            log = Path(directory) / "child.log"
            result = run_tests.run_process([sys.executable, "-u", "-c", source], os.environ.copy(), log, timeout)
            return result, log.read_text()

    def test_success_preserves_stdout_and_stderr(self):
        (status, _, reason), log = self.run_child("import sys; print('progress'); print('diagnostic', file=sys.stderr)")
        self.assertEqual(status, "PASS")
        self.assertEqual(reason, "")
        self.assertIn("progress\n", log)
        self.assertIn("diagnostic\n", log)

    def test_nonzero_exit_fails(self):
        (status, _, reason), _ = self.run_child("raise SystemExit(3)")
        self.assertEqual(status, "FAIL")
        self.assertIn("3", reason)

    def test_error_with_zero_exit_still_fails(self):
        for message in ["SCRIPT ERROR: bad script", "ERROR: invalid fixture", "Tests: 2 failures.",
                        "\x1b[31mERROR: colored diagnostic\x1b[0m"]:
            with self.subTest(message=message):
                (status, _, _), _ = self.run_child(f"print({message!r})")
                self.assertEqual(status, "FAIL")

    def test_zero_failures_is_success(self):
        (status, _, _), _ = self.run_child("print('Tests: 0 failures.')")
        self.assertEqual(status, "PASS")

    def test_timeout_terminates_process(self):
        (status, duration, _), _ = self.run_child("import time; print('started', flush=True); time.sleep(20)", 0.2)
        self.assertEqual(status, "TIMEOUT")
        self.assertLess(duration, 5)

    def test_discovery_excludes_helpers_and_separates_benchmarks(self):
        functional = {test.name for test in run_tests.discover(False)}
        complete = {test.name for test in run_tests.discover(True)}
        self.assertIn("godot/inventory_disk_test", functional)
        self.assertIn("python/runner_test", functional)
        self.assertNotIn("godot/test_case", functional)
        self.assertNotIn("godot/chunk_pipeline_benchmark", functional)
        self.assertIn("godot/chunk_pipeline_benchmark", complete)
        self.assertIn("native/chunk_queue_benchmark", complete)
        self.assertTrue(next(t for t in run_tests.discover(False) if t.name == "godot/voxel_ao_test").renderer)

    def test_snapshot_resolves_reordered_blocks_and_live_profile_values(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            data = root / "project/data"
            data.mkdir(parents=True)
            blocks = [{"id": 83, "name": "test_plant", "flag_mask": 81920},
                      {"id": 0, "name": "air", "flag_mask": 0}]
            for name in ["block_registry.json", "block_registry.generated.json"]:
                (data / name).write_text(json.dumps({"blocks": blocks}))
            (data / "biome_registry.json").write_text(json.dumps({"world": {"sea_level": 17}, "biomes": [
                {"id": 27, "name": "test_region", "rarity": 7, "vegetation": {
                    "patch_size": 18, "coverage_min": 35, "coverage_max": 60,
                    "plants": [{"block": "test_plant", "weight": 9, "min_height": 2, "max_height": 4}] }},
                {"id": 28, "name": "defaults_region", "vegetation": {
                    "coverage_min": 35, "flowers_min": 6,
                    "plants": [{"block": "test_plant", "min_height": 3}]}}
            ]}))
            fixture = root / "registry.txt"
            with patch.object(run_tests, "ROOT", root):
                run_tests.registry_snapshot(fixture)
            text = fixture.read_text()
            self.assertIn("biome 27 test_region 7", text)
            self.assertIn("profile test_region 18 1000 0 35 60", text)
            self.assertIn("plant test_region plants 83 9 2 4", text)
            self.assertIn("profile defaults_region 12 1000 0 35 35 6 6", text)
            self.assertIn("plant defaults_region plants 83 1 3 3", text)

    def test_stale_generated_ids_fail_before_execution(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            data = root / "project/data"
            data.mkdir(parents=True)
            (data / "biome_registry.json").write_text('{"world": {}, "biomes": []}')
            (data / "block_registry.json").write_text('{"blocks": [{"name": "air", "id": 0}]}')
            (data / "block_registry.generated.json").write_text('{"blocks": [{"name": "air", "id": 1}]}')
            with patch.object(run_tests, "ROOT", root), self.assertRaisesRegex(ValueError, "IDs differ"):
                run_tests.registry_snapshot(root / "registry.txt")


if __name__ == "__main__":
    unittest.main(verbosity=2)
