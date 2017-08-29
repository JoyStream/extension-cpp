import os
import shutil
from base import ExtensionBase

# Use this recipe when checking out a tagged version locally and you wish to use it in your projects
# on the stable channel. Source will be copied to cache from local folder
# do not upload the recipie if you don't want to include source files with the recipe

class ExtensionRelease(ExtensionBase):
    exports = "base.py"

    exports_sources = "../sources*"

    def source(self):
        os.mkdir("repo")
        shutil.move("sources", "repo/")
