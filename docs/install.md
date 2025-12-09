# ns3penv Installation

This installation works on Ubuntu 22.04, RockyOS 10, and macOS 15.0 or higher.

## Dependencies OS-level 

1. Boost C++ libraries
    - Ubuntu: `sudo apt install libboost-all-dev`
    - macOS: `brew install boost`
    - RedHat/Rocky OS/Fedora: `sudo dnf install boost-devel python3-devel`
2. CMake libraries:
    - ReadHat/Rocky OS/Fedora: `sudo dnf install cmake clang clang-tools-extra ccache ninja-build pkgconf-pkg-config`
3. Protocol buffers
    - Ubuntu: `sudo apt install libprotobuf-dev protobuf-compiler`
    - macOS: `brew install protobuf`
    - RedHat/RockyOS/Fedora: `sudo dnf install protobuf-compiler protobuf-devel`
4. pybind11
    - Ubuntu: `sudo apt install pybind11-dev`
    - macOS: `brew install pybind11`
    - RedHat/RockyOs/Fedora: `sudo dnf install pybind11-devel`
5. A Python virtual environment dedicated for ns3-ai (highly recommended)
    - For example, to use conda to create an environment named `ns3ai_env` with python version 3.11: `conda create -n ns3ai_env python=3.11`.
    - `uv` by astral is an excellent package manager that manages virtual-envs too. check the `astral.sh` website for more.

## Module Setup

1. Follow the ns3 installation instrcutions

2. Clone this repository at `contrib/`

```shell
cd $NS3_DIRECTORY/contrib
git clone https://github.com/hust-diangroup/ns3penv.git 
```

3. Add type stubs generation tools

```shell
uv add pybind11-stubgen mypy-protobuf cmake-format`
ln -s $PYTHON_PROJECT/.venv/bin/pybind11-stubgen $HOME/.local/bin/pybind11-stubgen && \
ln -s $PYTHON_PROJECT/.venv/bin/protoc-gen-mypy $HOME/.local/bin/protoc-gen-mypy
```

4. Configure and build the ns3penv (choosing optimized or debug)

```shell
cd $NS3_DIRECTORY
CXX="clang++" C="clang" ./ns3 configure --enable-examples --enable-tests --enable-clang-tidy --cxx-standard=23 --disable-warnings -d optimized
./ns3 build ns3penv
uv add -editable $NS3DIRECTORY/contrib/python/src/ns3env
```
__NOTE__: The ns3 module is called `ns3penv` and the python package is called `ns3env`

5. Build your example that is used as an executable for your environment

`./ns3 build <your-project>`
