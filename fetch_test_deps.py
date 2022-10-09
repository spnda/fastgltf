#!/usr/bin/env python3

import os
import sys
import urllib.error
import urllib.request
import zipfile

example_deps_urls = {
    'glfw': "https://github.com/glfw/glfw/releases/download/3.3.8/glfw-3.3.8.zip",
    'glm': "https://github.com/g-truc/glm/releases/download/0.9.9.8/glm-0.9.9.8.zip",
}
test_deps_urls = {
    'catch2': "https://github.com/catchorg/Catch2/archive/refs/tags/v3.1.0.zip"
}

example_deps_folder = "examples/deps/"
test_deps_folder = "tests/deps/"


def download_zip_and_extract(url, output_folder, name):
    output = f'{output_folder}{name}'
    file_path, _ = urllib.request.urlretrieve(url, f'{output}.zip')
    with zipfile.ZipFile(file_path, "r") as zip_ref:
        names = zip_ref.namelist()
        if len(names) == 0:
            return
        # Perhaps the file exists already
        if not os.path.isdir(output):
            zip_ref.extractall(output_folder)
            # The zip file contains another folder called the same name.
            if name.lower() in names[0].lower():
                os.rename(f'{output_folder}{names[0]}', output)

    os.remove(file_path)


def main():
    if not os.path.isdir(example_deps_folder):
        os.mkdir(example_deps_folder)
    if not os.path.isdir(test_deps_folder):
        os.mkdir(test_deps_folder)

    for name, url in example_deps_urls.items():
        try:
            download_zip_and_extract(url, example_deps_folder, name)
            print(f'Finished downloading {name}')
        except urllib.error.HTTPError:
            print(f'Could not download {url}.', file=sys.stderr)
            break

    for name, url in test_deps_urls.items():
        try:
            download_zip_and_extract(url, test_deps_folder, name)
            print(f'Finished downloading {name}')
        except urllib.error.HTTPError:
            print(f'Could not download {url}.', file=sys.stderr)


if __name__ == '__main__':
    main()
