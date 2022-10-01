#!/usr/bin/env python3

import os
import sys
import urllib.error
import urllib.request
import zipfile

deps_urls = {
    'glfw': "https://github.com/glfw/glfw/releases/download/3.3.8/glfw-3.3.8.zip",
    'glm': "https://github.com/g-truc/glm/releases/download/0.9.9.8/glm-0.9.9.8.zip",
}

deps_folder = "examples/deps/"


def main():
    if not os.path.isdir(deps_folder):
        os.mkdir(deps_folder)

    for name, url in deps_urls.items():
        try:
            file_path, _ = urllib.request.urlretrieve(url, f'{deps_folder}{name}.zip')
            with zipfile.ZipFile(file_path, "r") as zip_ref:
                names = zip_ref.namelist()
                if len(names) == 0:
                    break
                # Perhaps the file exists already
                if not os.path.isdir(f'{deps_folder}{name}'):
                    zip_ref.extractall(f'{deps_folder}')
                    # The zip file contains another folder called the same name.
                    if name in names[0]:
                        os.rename(f'{deps_folder}{names[0]}', f'{deps_folder}{name}')

            os.remove(file_path)
            print(f'Finished downloading {name}')
        except urllib.error.HTTPError:
            print(f'Could not download {url}.', file=sys.stderr)
            break


if __name__ == '__main__':
    main()
