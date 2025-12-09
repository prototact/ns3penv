import gymnasium as gym
import numpy as np
from gymnasium import spaces
from .experiment import Experiment
from types import FrameType
from typing import Any
from pathlib import Path
from .proto import messages_pb2 as pb
from numpy.typing import NDArray

POLL_INTERVAL: int = 1  # seconds

DataType = np.generic | NDArray[np.generic] | tuple["DataType"] | dict[str, "DataType"]


class Ns3Env(gym.Env[Any, Any]):
    exp: Experiment
    extraInfo: dict[str, Any] | str | None
    _created = False

    def _poll_handler(self, signum: int, frame: FrameType | None) -> None:
        if not self.exp.isalive():
            print("Ns3 process died! resetting environment.")
            del self.exp

    def _create_space(self, spaceDesc: pb.SpaceDescription) -> spaces.Space[DataType]:
        match spaceDesc.WhichOneof("space_variant"):
            case "discrete":
                return spaces.Discrete(n=spaceDesc.discrete.n)
            case "box":
                low = spaceDesc.box.low
                high = spaceDesc.box.high
                shape = tuple(spaceDesc.box.shape)
                dtype = spaceDesc.box.dtype

                match dtype:
                    case pb.INT:
                        mtype = np.int32
                    case pb.UINT:
                        mtype = np.uint32
                    case pb.DOUBLE:
                        mtype = np.float32
                    case pb.FLOAT:
                        mtype = np.float64
                    case _:
                        raise ValueError(f"Unknown box dtype {dtype}")

                return spaces.Box(low=low, high=high, shape=shape, dtype=mtype)

            case "tuple":
                return spaces.Tuple(
                    tuple(
                        self._create_space(space_desc)
                        for space_desc in spaceDesc.tuple.element
                    )
                )

            case "dict":
                return spaces.Dict(
                    {
                        space_desc.name: self._create_space(space_desc)
                        for space_desc in spaceDesc.dict.element
                    }
                )
            case type_:
                raise TypeError(f"Unknown space type {type_}")

    def _create_data(self, dataContainer: pb.DataContainer) -> Any:
        match dataContainer.WhichOneof("data"):
            case "discrete":
                return dataContainer.discrete.data

            case "box":
                box = dataContainer.box
                # TODO: reshape data
                match box.dtype:
                    case pb.INT:
                        return np.array(box.intData)
                    case pb.UINT:
                        return np.array(box.uintData)
                    case pb.DOUBLE:
                        return np.array(box.doubleData)
                    case pb.FLOAT:
                        return np.array(box.floatData)
                    case _:
                        raise ValueError(f"Unknown box dtype {box.dtype}")

            case pb.Tuple:
                return tuple(
                    self._create_data(sub_data)
                    for sub_data in dataContainer.tuple.element
                )

            case pb.Dict:
                return {
                    sub_data.name: self._create_data(sub_data)
                    for sub_data in dataContainer.dict.element
                }
            case type_:
                raise TypeError(f"Unknown data type {type_}")

    def initialize_env(self) -> bool:
        simInitMsg = pb.SimInitMsg()
        if self.msgInterface is not None:
            self.msgInterface.PyRecvBegin()
            request = self.msgInterface.GetCpp2PyStruct().get_buffer()
            simInitMsg.ParseFromString(request)
            self.msgInterface.PyRecvEnd()

            self.action_space = self._create_space(simInitMsg.actSpace)
            self.observation_space = self._create_space(simInitMsg.obsSpace)

            reply = pb.SimInitAck()
            reply.done = True
            reply.stopSimReq = False
            reply_str = reply.SerializeToString()
            assert len(reply_str) <= self.py_binding.msg_buffer_size

            self.msgInterface.PySendBegin()
            self.msgInterface.GetPy2CppStruct().size = len(reply_str)
            self.msgInterface.GetPy2CppStruct().get_buffer_full()[: len(reply_str)] = (
                reply_str
            )
            self.msgInterface.PySendEnd()
            return True
        return False

    def send_close_command(self) -> bool:
        reply = pb.EnvActMsg()
        reply.stopSimReq = True

        replyMsg = reply.SerializeToString()
        assert len(replyMsg) <= self.py_binding.msg_buffer_size
        if self.msgInterface is not None:
            self.msgInterface.PySendBegin()
            self.msgInterface.GetPy2CppStruct().size = len(replyMsg)
            self.msgInterface.GetPy2CppStruct().get_buffer_full()[: len(replyMsg)] = (
                replyMsg
            )
            self.msgInterface.PySendEnd()

            self.newStateRx = False
            return True
        return False

    def rx_env_state(self) -> None:
        if self.newStateRx:
            return

        envStateMsg = pb.EnvStateMsg()
        if self.msgInterface is not None:
            self.msgInterface.PyRecvBegin()
            request = self.msgInterface.GetCpp2PyStruct().get_buffer()
            envStateMsg.ParseFromString(request)
            self.msgInterface.PyRecvEnd()

            self.obsData = self._create_data(envStateMsg.obsData)
            self.reward = envStateMsg.reward
            self.gameOver = envStateMsg.isGameOver
            self.gameOverReason = envStateMsg.reason

            if self.gameOver:
                self.send_close_command()

            self.extraInfo = envStateMsg.info
            if not self.extraInfo:
                self.extraInfo = {}

            self.newStateRx = True

    def get_obs(self) -> Any | None:
        return self.obsData

    def get_reward(self) -> float:
        return self.reward

    def is_game_over(self) -> bool:
        return self.gameOver

    def get_extra_info(self) -> dict[str, Any] | str | None:
        return self.extraInfo

    def _pack_data(
        self, actions: DataType, spaceDesc: spaces.Space[Any]
    ) -> pb.DataContainer:
        match type(spaceDesc):
            case spaces.Discrete:
                assert isinstance(actions, int)
                discrete = pb.DiscreteDataContainer(data=actions)
                return pb.DataContainer(discrete=discrete)

            case spaces.Box:
                assert isinstance(actions, np.ndarray)
                match spaceDesc.dtype:
                    case type_ if type_ in ["int", "int8", "int16", "int32", "int64"]:
                        return pb.DataContainer(
                            box=pb.BoxDataContainer(
                                dtype=pb.INT,
                                shape=actions.shape,
                                intData=actions,
                            )
                        )
                    case type_ if type_ in [
                        "uint",
                        "uint8",
                        "uint16",
                        "uint32",
                        "uint64",
                    ]:
                        return pb.DataContainer(
                            box=pb.BoxDataContainer(
                                dtype=pb.UINT,
                                shape=actions.shape,
                                uintData=actions,
                            )
                        )
                    case type_ if type_ in ["float", "float32", "float64"]:
                        return pb.DataContainer(
                            box=pb.BoxDataContainer(
                                dtype=pb.FLOAT,
                                shape=actions.shape,
                                floatData=actions,
                            )
                        )
                    case type_ if type_ in ["double"]:
                        return pb.DataContainer(
                            box=pb.BoxDataContainer(
                                dtype=pb.DOUBLE,
                                shape=actions.shape,
                                doubleData=actions,
                            )
                        )
                    case _:
                        raise ValueError(f"Unknown box dtype {spaceDesc.dtype}")
            case spaces.Tuple:
                assert isinstance(actions, tuple)
                assert isinstance(self.action_space, spaces.Tuple)
                return pb.DataContainer(
                    tuple=pb.TupleDataContainer(
                        element=tuple(
                            self._pack_data(sub_action, sub_space)
                            for sub_action, sub_space in zip(
                                actions, self.action_space.spaces
                            )
                        )
                    )
                )

            case spaces.Dict:
                assert isinstance(actions, dict)
                assert isinstance(self.action_space, spaces.Dict)

                def rename(dat: pb.DataContainer, name: str) -> pb.DataContainer:
                    dat.name = name
                    return dat

                return pb.DataContainer(
                    dict=pb.DictDataContainer(
                        element=[
                            rename(
                                self._pack_data(sub_data, self.action_space[name]), name
                            )
                            for name, sub_data in actions.items()
                        ]
                    )
                )
            case type_:
                raise TypeError(f"Unknown space type {type_}")

    def send_actions(self, actions: DataType) -> bool:
        reply = pb.EnvActMsg()

        actionMsg = self._pack_data(actions, self.action_space)
        reply.actData.CopyFrom(actionMsg)

        replyMsg = reply.SerializeToString()
        assert len(replyMsg) <= self.py_binding.msg_buffer_size
        if self.msgInterface is not None:
            self.msgInterface.PySendBegin()
            self.msgInterface.GetPy2CppStruct().size = len(replyMsg)
            self.msgInterface.GetPy2CppStruct().get_buffer_full()[: len(replyMsg)] = (
                replyMsg
            )
            self.msgInterface.PySendEnd()
            self.newStateRx = False
            return True
        return False

    def get_state(
        self,
    ) -> tuple[Any, float, bool, bool, dict[str, Any]]:
        obs = self.get_obs()
        reward = self.get_reward()
        done = self.is_game_over()
        extraInfo = {"info": self.get_extra_info()}
        return obs, reward, done, False, extraInfo

    def __init__(
        self,
        targetName: str,
        ns3Path: str | Path,
        ns3Settings: dict[str, str | int | float] | None = None,
        msg_interface_settings: dict[str, str | bool] | None = None,
        shmSize: int = 4096,
    ):
        try:
            import ns3penv_gym_msg_py
        except ImportError:
            from . import ns3penv_gym_msg_py

        self.py_binding = ns3penv_gym_msg_py

        if self._created:
            raise Exception("Error: Ns3Env is singleton")
        self._created = True
        self._msg_interface_settings = msg_interface_settings
        self._shmSize = shmSize
        ns3_path = ns3Path if isinstance(ns3Path, Path) else Path(ns3Path)
        if msg_interface_settings is not None:
            self.exp = Experiment(
                targetName,
                ns3_path,
                self.py_binding.Ns3penvMsgInterfaceImpl,
                segName=msg_interface_settings["segName"],
                cpp2pyMsgName=msg_interface_settings["cpp2pyMsgName"],
                py2cppMsgName=msg_interface_settings["py2cppMsgName"],
                lockableName=msg_interface_settings["lockableName"],
                handleFinish=msg_interface_settings["handleFinish"],
                shmSize=shmSize,
            )
        else:
            self.exp = Experiment(
                targetName,
                ns3_path,
                self.py_binding.Ns3penvMsgInterfaceImpl,
                shmSize=shmSize,
            )
        self.ns3Settings = ns3Settings

        self.newStateRx = False
        self.obsData = None
        self.reward = 0
        self.gameOver = False
        self.gameOverReason = None
        self.extraInfo = None

        self.msgInterface = self.exp.run(setting=self.ns3Settings, show_output=True)
        self.initialize_env()
        # get first observations
        self.rx_env_state()
        self.envDirty = False

    def step(self, action: DataType) -> tuple[Any, float, bool, bool, dict[str, Any]]:
        self.send_actions(action)
        self.rx_env_state()
        self.envDirty = True
        return self.get_state()

    def reset(
        self, *, seed: int | None = None, options: dict[str, Any] | None = None
    ) -> tuple[Any, dict[str, Any]]:
        if not self.envDirty:
            obs = self.get_obs()
            return obs, {}

        # not using self.exp.kill() here in order for semaphores to reset to initial state
        if not self.gameOver:
            self.rx_env_state()
            self.send_close_command()

        self.msgInterface = None
        self.newStateRx = False
        self.obsData = None
        self.reward = 0
        self.gameOver = False
        self.gameOverReason = None
        self.extraInfo = None

        self.msgInterface = self.exp.run(show_output=True)
        self.initialize_env()
        # get first observations
        self.rx_env_state()
        self.envDirty = False

        obs = self.get_obs()
        return obs, {}

    def render(self, mode: str = "human") -> None:
        return

    def get_random_action(self) -> NDArray[np.generic]:
        act = self.action_space.sample()
        return act

    def close(self):
        # environment is not needed anymore, so kill subprocess in a straightforward way
        self.exp.kill()
        # destroy the message interface and its shared memory segment
        del self.exp
