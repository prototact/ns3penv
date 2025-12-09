# ns3penv (modified by Orfeas Karachalios)

## Introduction

[ns–3](https://www.nsnam.org/) is widely recognized as an excellent open-source networking simulation
tool utilized in network research and education. In recent times, there has been a growing interest in
integrating AI algorithms into network research, with many researchers opting for open-source frameworks
such as [TensorFlow](https://www.tensorflow.org/) and [PyTorch](https://pytorch.org/). Integrating the
ML frameworks with simulation tools in source code level has proven to be a challenging task due to their
independent development. As a result, it is more practical and convenient to establish a connection
between the two through interprocess data transmission.

<p align="center">
    <img src="./docs/architecture.png" alt="arch" width="500"/>
</p>

The original model offered an efficient solution to facilitate the data exchange between ns-3 and Python-based
AI frameworks or any other control logic. It does not implement any specific AI algorithms. Instead, it focuses on enabling
interconnectivity between Python and C++. Therefore, it is necessary to separately install the desired AI framework or other control framework. Then it is necessary to define the data exchanged between ns-3 and your AI algorithms.

The approach for enabling this data exchange is inspired by [ns3-gym](https://github.com/tkn-tub/ns3-gym),
but it utilizes a shared-memory-based approach, which not only ensures faster execution but also provides greater flexibility.

At the same time, the ns3-ai did not allow for parallel execution of environments,
because the shared memory created had the same name, and since it uses OS memory directly,
different processes attempted to utilize the same shared memory segment and failed.

This version allows parallel environments and reorganized the module to simplify the build
process and python package installation.

## Modifications to the original ns3-ai

The initial implementation was by the [ns3-ai](https://github.com/hust-diangroup/ns3-ai) and it was refactor to
- automatically generate type annotations for the protobuf generated modules and bindings to python
- restructure directory into ns3 standards, with a refactored CMakeLists.txt
- make the generated python package into a proper package
- modified the protobuf declarations to be more performant

### Features

- High-performance data interaction module in both C++ and Python side.
- A high-level [Gym interface](model/gym-interface) for using Gymnasium APIs, and a low-level
  [message interface](model/msg-interface) for customizing the shared data.
- able to run multiple ns3 simulations at the same time using torch-rl `MultiSyncDataCollector`

### To Do

- currently if the python process orchestrating the subpython processes (using the multiprocessing module) that launch the ns3 processes are killed unexpectedly, the ns3 processes will continue to run and need to be killed manually. One way to do this is (in bash)

`ps | grep ns3 | awk '{print $1}' | xargs kill`

- the python processes take a lot of memory so it is advised to use at least 32 GB of RAM

## Installation

The installation has been simplified compared to the original

First, install necessary dependencies. These are mentioned in `docs/install.md`

Second, clone the github repo into the contrib folder of the ns3 project.

Then, build the ns3 project normally with

`./ns3 build ns3penv`

After building the module with the ns3 tool, install the generated python package with
Note the `gymnasium` should be installe first.

```bash
uv add --editable $HOME/ns-3-dev/contrib/ns3penv/python
```

or

```bash
pip install --editable $HOME/ns-3-dev/contrib/ns3penv/python
```

depending on your python package manager.

## Introduction to ns3-penv

### Documentation 

The directory structure is the following

- `ns3penv`: the ns3 module
  - `model`: standard C++ side ns3 model. This contains the following modules
    - `spaces`: space definitions, that represent the domain of data being exchanged between ns3 and the python process, according to the gymnasium standard for RL environments
    - `container`: the data containers that were declared via the spaces module, and carry the actual data.
    - `ns3penv-semaphore`: a synchronization object, useful for safe interprocess communication
    - `ns3penv-msg-interface`: the message exchange object, that utilizes a singleton to guarantee safe access to the boost lib memory segment
    - `ns3penv-gym-msg`: the message structure that represents low level serialized data that is used by protobuf to transmit messages between the two processes.
    - `ns3penv-gym-interface`: the gym interface that receives and sends data between the message interface and the ns3 environment/experiment.
    - `ns3penv-gym-env`: the actual environment that can collect statistics and apply configurations on the ns3 enviroment. It follows the gymnasium standard and it is supposed to be subclassed by
    the developers for defining the actual experiment.
  - `bindings`: the bindings between the message interface towards python generated by the pybind11 tool
  - `proto`: the protobuf definition for messages being exchanged 
  - `python/src/ns3env`: the python package
    - `experiment`: the object that initializes, monitors and destroys the ns3 process as a separate process.
    - `ns3env`: the declaration of the ns3 environment in python, that interprets messages and handles the communication. Note the actual simulation logic is implemented in `ns3penv-gym-env` methods.

Once modules have been built with CMake and the python package is installed, typing-related files
and messages will be generated automatically and placed in the right directories.

Note that the python side is just scaffolding for interaction with the subclassed environment from
`ns3penv-gym-env`. The environment is registered automatically to the gymnasium environments during installation, so using a gym-like environment wrapper to load the environment should work.

### Defining your own experiment

It is recommended to built your own ns3 module for your experiment. This should include

- `your_ns3_module`
  - `examples`: the definition of a simulation script, with command line arguments for configuration 
  - `model`: the definition of your environment
- `your_python_package`
  - `script.py`: this can use the `multiprocessing` python module to launch multiple environments via a gymansium env wrapper per python subprocess. Note that the processes should have isolated namespaces (use `spawn` not `fork` as mp creation type).

The only part developers should interact with directly is the `ns3penv-gym-env` class,
when it becomes subclassed in your ns3 module model/environment. Note
the environment will have to use existing `TraceSources` of ns3 network components if the developers
do not want to modify or add modules to ns3.
The `example` should include calls to the environment, see the `docs/guide.md` for more.

## Tutorials

For a more detailed installation procedure see `docs/instal.md` and 
for a more in-depth guide see `docs/guide.md`

### Google Summer of Code 2023 (ns3-ai)

Notes from the original ns3-ai package:

'ns3-ai improvements' has been chosen as one of the [project ideas](https://www.nsnam.org/wiki/GSOC2023Projects)
for the ns-3 projects in [GSoC 2023](https://summerofcode.withgoogle.com/programs/2023). The project
developed the message interface (struct-based & vector-based) and Gym interface, provided more examples
and enhanced stability and usability.

- Project wiki page: [GSOC2023ns3-ai](https://www.nsnam.org/wiki/GSOC2023ns3-ai)

## Cite The Original Team's Work (ns3-ai)

Please use the following bibtex:

```
@inproceedings{10.1145/3389400.3389404,
author = {Yin, Hao and Liu, Pengyu and Liu, Keshu and Cao, Liu and Zhang, Lytianyang and Gao, Yayu and Hei, Xiaojun},
title = {Ns3-Ai: Fostering Artificial Intelligence Algorithms for Networking Research},
year = {2020},
isbn = {9781450375375},
publisher = {Association for Computing Machinery},
address = {New York, NY, USA},
url = {https://doi.org/10.1145/3389400.3389404},
doi = {10.1145/3389400.3389404},
booktitle = {Proceedings of the 2020 Workshop on Ns-3},
pages = {57–64},
numpages = {8},
keywords = {AI, network simulation, ns-3},
location = {Gaithersburg, MD, USA},
series = {WNS3 2020}
}

```
