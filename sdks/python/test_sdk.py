import os
import shutil
import tempfile
import unittest
from minigit_sdk import MiniGitClient

class TestPythonSDK(unittest.TestCase):
    def setUp(self):
        self.test_dir = tempfile.mkdtemp(prefix="minigit_py_test_")

    def tearDown(self):
        if os.path.exists(self.test_dir):
            shutil.rmtree(self.test_dir, ignore_errors=True)

    def test_init_and_open(self):
        repo_dir = os.path.join(self.test_dir, "repo")
        client = MiniGitClient(repo_dir)
        self.assertEqual(client.repo_path, repo_dir)

if __name__ == "__main__":
    unittest.main()
