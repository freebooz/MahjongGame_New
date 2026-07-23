import os
import runpy

scripts_dir = os.path.dirname(os.path.abspath(__file__))
runpy.run_path(os.path.join(scripts_dir, "SanitizeMahjongClientMap.py"), run_name="__main__")
