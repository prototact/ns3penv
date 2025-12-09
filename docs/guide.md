# Guide to building an interface between ns3 and python controllers

In the following texts, pointers always means the ns3 smart pointers `Ptr<Ns3Component>`

## Design and Implementation

The two processes, python controller and C++ ns3 simulation, communicate
fast via OS-level shared memory provided via the boost library.

The main entrypoint in Python is a GymEnv wrapper that uses bindings to C++
down the stack. The standard gym library is used to define the wrapper.

The main entrypoint in C++ is an environment class that inherits from OpenGymEnv,
which mirrors the python bindings, however the two classes are independent
and manage communicate only through the shared message format declared via the Protobuf library.

In this manner, the standard Gym library in python can be used to embed the
ns3 simulation and treat it as a gym environment. In principle, you
only need to define this class and its interactions with the ns3 simulation in C++,
while the Python side is simple in that the GymEnv wrapper effectively proxies
the same `OpenGymEnv`. Defining control of the simulation through the class using
Python is the job of the developer.

### High-Level Description

A high-level description of the underlying communication follows:

- a shared memory space using the boost library is created and utilized to exchange messages.
- __WARNING__: This exists externally to the c++ or python process, and can be seen by any
OS process that has a handle to the shared memory. This means it should
also be cleaned-up properly when ending the simulation, otherwise it will
persist until OS reboot.
- the ns3 OpenGymEnv utilizes the ns3penv interface to write into the shared
memory using a format dictated by the Google Protobuf library declared
in `ns3penv/proto/messages.proto`. These messages live in the namespace
`ns3penv`.
- __NOTE__: protobuf generates a message module that binds on the C++ side, while Python
python bindings to read and write messages are generated via the pybind11 library.
- On C++ in particular, the module `ns3penv-msg-interface.h` defines a singleton that embeds access to the shared memory.
- __WARNING__: the singleton pattern guarantees ownership over the
shared memory access. However, it causes the following issue on the Python side:
multiple environments cannot be created in the same python process because
the C++ runtime (including OS libraries) will be shared across python processes and only one shared memory will be available. To run multiple environments, it is necessary to spawn
new python subprocesses controlled by a main process, and these processes
should have their own namespaces, e.g. they need to load the `ns3env` module
in their own scope, and __not in the global scope__.
- Python starts the ns3 simulation via calling `reset` on the GymEnv wrapper
and the ns3 process reaches the `Simulator::Run` state.
The control is yielded back to python at that point, which calls the step method on the environment,
then control is yielded back to ns3. From that point, the developer has to decide on
the strategy when the control is yielded back to python and so on. Examples are provided later on.
- At the end of the simulation, it is important to properly clean up the shared memory space.
This should be done automatically by C++ destructors, but if the program is killed prematurely, depending on how it was killed, the memory may not be cleaned up
- __NOTE__: It is reccomended to use a container for developing, testing and deploying, so it is easy
to restart the container and have a fresh process tree.

### The OpenGymEnv in C++

The class OpenGymEnv defines the environment observations and actions,
and specifies when control is yielded back to the python process.

Note that OpenGymEnv is a child of Object, so it needs the usual
boilerplate `GetTypeId` and `DoDispose`.

The header file should have the following declaration

```cpp
#include <ns3/ns3penv-module.h>

class MyEnvironment : public OpenGymEnv 
{
  public:
    // useful for adding Attributes for configuration
    static TypeId GetTypeId();
    
    // connect some tracesources here
    void DoInitialize() override;

    // remove pointers to ns3 components
    void DoDispose() override;
};
```

A constructor can be written as well, that can take any arguments
the developer team deems necessary for the environment to have.
For example, it could take as input pointers to certain ns3 network
components, so it can collect statistics and configuration and
apply configuration on the same components.

Note that if pointers are passed to the environment, they
should be cleaned up at the `DoDispose` step in general

In addition to that, the environment needs the following definitions:

1. The observation and action spaces should always be defined as the types
of observation and action. They are not the values themselves, and they
are necessary for the shared memory to know their expected format.

```cpp
  // inside the class def
  public:
    Ptr<OpenGymSpace> MyEnvironment::GetObservationSpace();

    Ptr<OpenGymSpace> MyEnvironment::GetActionSpace();
```

Spaces are build from the basic building blocks and can be
found in `ns3penv/model/spaces.*`. They are

- `OpenGymDiscreteSpace` is just a scalar space with levels from $0$ to $n-1$
- `OpenGymBoxSpace` is a vector of numeric values, where the type of each and the range of values has to be specified
- `OpenGymTupleSpace` is a tuple of other Spaces
- `OpenGymDictSpace` is a dictionary with string keys and other Spaces as values

The two first are base spaces while the last two may nest any type of space.

The standard approach is to define your own observation in the simulator by
collecting statistics and configuration from the ns3 simulation using TraceSources,
processing them in a custom class and reading from that custom class and passing
them to `OpenGymEnv` in order to fill in the observations sent to the Python controller process.
This happens in the `OpenGymEnv::GetObservation` method.

Note that the relevant data should be updated before calling the `Notify` action.

Actions are yielded back from the python controller to the ns3 simulation
and configure certain simulation objects. For example, it could dictate
the slicing allocation on the `NrMacSchedulerNs3` instance, that acts as an upper
limit to allocating resources to a group of ues. How the action is interpreted
exaclty occurs in `ExecuteActions` but usually involves configuring some
ns3 network component.

```cpp
  public:
    Ptr<OpenGymContainer> MyEnvironment::GetObservation();

    Ptr<OpenGymContainer> MyEnvironment::ExecuteActions();
```

2. The `GameOver`, `GetReward` and `GetExtraInfo` are not strictly necessary and can be implemented as empty functions or returning some default value that is ignored in the Python controller's logic.

In general, an ns3 simulation can stop at any step
from the python side or when the simulation time runs out. The method `GameOver` is strictly
for special conditions becoming realised. For example congestion may lead to an
exceedingly high packet drop rate, at which point the `GameOver` condition could occur.
However, you would need to implement an "accumulator" as a member of the environment and
the logic of fetching the statistics and updating the "accumulator". `GameOver` would check
if the member reaches the pre-specified condition and if true, it would end the simulation early.

`GetReward` would require a certain computation to occur on the python side and is useful mostly
in Reinforcement Learning controllers. However, it is a scalar by default, which prevents
more structured rewards that could be interpreted on the python side, by using dynamic weights
for example. A more structured reward can be passed as part of the observation, in order to circumvent this, and the final scalar reward can be computed in the python controller

`GetExtraInfo` usually refers to metadata or some other message for general-purpose
signalling. In principle, complex information can be serialized into a string, but this is not
very efficient or safe.

```cpp
  // inside the class def
  public:
    bool GameOver() override;

    float GetReward() override;

    std::string GetExtraInfo() override;
```

3. Synchronization is critical for proper interoperation and on the ns3 side this is
faciliated via the Event scheduler. One solution is to call a recusive method on the custom OpenGymEnv
that contains a `Notify()` call and a `Simulator::Schedule(Time(interval), method, *vars)`, and bootstrap it in the original simulation script by calling it for the first time before `Simulator::Run`.
__NOTE__ call `env->NotifySimulationEnd()` after destroying the simulator at the end of the script (the clean-up phase), so disposal of shared memory occurs correctly.

An example of this method would be

```cpp
    void MyEnvironment::ScheduleNextDecision(var var1, var var2)
    {
        // optional guard, checks if a condition is met before taking action
        if (var1 && var2)
        {
            // some logic here involving members too
            UpdateObservationMemberWithCollectedData() // dummy method, make your own
            Notify()
            // some other logic here involing members too
        }
        // assume var_1 and var_2 are updated values of var1, var2
        Simulator::Schedule(&MyEnvironment::ScheduleNextDecision, this, var1_, var2_)
    }
```

Notify calls the `GetObservation` and `ExecuteActions` automatically, so the only thing
the developer should be aware of is the state of environment as expressed in data members
of the class that can be pointers to objects that update their own state with simulation
statistics and component configuration or directly methods that are connected to simulation objects' ns3 tracesources or configure ns3 attributes.

4. Clean-up is important, and when the simulation ends on its own, it occurs normally.
However, if a kill signal is sent to ns3 process early via python, it will not be received by the ns3 process immediately.

5. The python-side gym env wrapper needs to be passed certain settings to run the simulation, for example
when using `torch-rl`

```python
   import gymansium
   from torchrl.envs import GymEnv, set_gym_backend

   def create_env_func(format_arguments: str, env_id: int)
       import ns3env
       "msg_interface_settings": {
          "segName": f"seg{env_id}",
          "cpp2pyMsgName": f"cpp2py{env_id}",
          "py2cppMsgName": f"py2cpp{env_id}",
          "lockableName": f"lockable{env_id}",
          "handleFinish": False,
        }  
        with set_gym_backend(gymnasium):
          env = GymEnv(
              "ns3env-v0",
              targetName=f'--no-build "run-marl-environment {format_arguments}"',
              ns3Path="/home/ns3/ns-3-dev",
              shmSize=131072,
              msg_interface_settings=msg_interface_settings,
              auto_reset=False,
              device=device,
            )
```

Note that the ns3penv interface needs a unique id per ns3 simulation to register a new shared memory segment.
In addition, `format_arguments` that are input to the simulation as `cmdArgs` need to be specified in the proper format
`--option=value`.
