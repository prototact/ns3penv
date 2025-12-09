# Copyright (c) 2019-2023 Huazhong University of Science and Technology
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License version 2 as
# published by the Free Software Foundation;
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
#
# Author: Pengyu Liu <eic_lpy@hust.edu.cn>
#         Hao Yin <haoyin@uw.edu>
#         Muyuan Shen <muyuan_shen@hust.edu.cn>
# Modified by: Orfeas Karachalios <okarachalios@iit.demokritos.gr>

"""Module that represents a single ns3 simulation program being started, run and ended as an experiment.
Warning:
    Currently if the python process that initiates the ns3 process dies early, i.e. from SIGINT signal,
    the ns3 process will continue to run unhindered and needs to be killed manually.
"""

import os
import signal
import subprocess
import time
import traceback
import types
from collections.abc import Callable
from typing import Any
from pathlib import Path

import psutil

try:
    from . import ns3penv_gym_msg_py as msg
except ImportError:
    import ns3penv_gym_msg_py as msg

SIMULATION_EARLY_ENDING = (
    0.5  # wait and see if the subprocess is running after creation
)


def get_setting(setting_map: dict[str, str | float | int]) -> str:
    """Serialize the settings to be fed to the ns3 tool in the launched shell
    Settings here represent any additional options for the scenario to be run.
    """
    return " ".join(f"--{key}={value}" for key, value in setting_map.items())


def run_single_ns3(
    path: str,
    pname: str,
    setting: dict[str, str | float | int] | None = None,
    env: dict[str, str] | None = None,
    show_output: bool = False,
) -> tuple[str, subprocess.Popen[str]]:
    """Run a single instance of ns3"""
    if env is None:
        env = {}
    env.update(os.environ)
    env["LD_LIBRARY_PATH"] = os.path.abspath(os.path.join(path, "build", "lib"))
    # import pdb; pdb.set_trace()
    exec_path = os.path.join(path, "ns3")
    cmd = (
        f"{exec_path} run {pname}"
        if setting is None
        else f"{exec_path} run {pname} --{get_setting(setting)}"
    )
    proc = subprocess.Popen(
        cmd,
        shell=True,
        text=True,
        env=env,
        stdin=subprocess.PIPE,
        stdout=subprocess.PIPE if not show_output else None,
        stderr=subprocess.PIPE if not show_output else None,
        preexec_fn=os.setpgrp,
    )

    return cmd, proc


# used to kill the ns-3 script process and its child processes
def kill_proc_tree(
    p: int | psutil.Process | subprocess.Popen[str],
    timeout: int | None = None,
    on_terminate: Callable[[psutil.Process], object] | None = None,
):
    print("ns3penv_utils: Killing subprocesses...")
    if isinstance(p, int):
        p = psutil.Process(p)
    elif isinstance(p, subprocess.Popen):
        p = psutil.Process(p.pid)
    ch = [p] + p.children(recursive=True)
    for c in ch:
        try:
            # print("\t-- {}, pid={}, ppid={}".format(psutil.Process(c.pid).name(), c.pid, c.ppid()))
            # print("\t   \"{}\"".format(" ".join(c.cmdline())))
            c.kill()
        except Exception as exc:
            traceback.print_tb(exc.__traceback__)
            continue
    succ, err = psutil.wait_procs(ch, timeout=timeout, callback=on_terminate)
    return succ, err


# According to Python signal docs, after a signal is received, the
# low-level signal handler sets a flag which tells the virtual machine
# to execute the corresponding Python signal handler at a later point.
#
# As a result, a long ns-3 simulation, during which no C++-Python
# interaction occurs (such as the Multi-BSS example), may run uninterrupted
# for a long time regardless of any signals received.
def sigint_handler(sig: int, frame: types.FrameType | None) -> Any:
    print("ns3penv_utils: SIGINT detected")
    exit(1)  # this will execute the `finally` block


# This class sets up the shared memory and runs the simulation process.
class Experiment:
    proc: subprocess.Popen[str] | None
    msgModule: types.ModuleType
    _created = False

    # init ns-3 environment
    # \param[in] memSize : share memory size
    # \param[in] targetName : program name of ns3
    # \param[in] path : current working directory
    def __init__(
        self,
        targetName: str,
        ns3Path: Path,
        msgType: type[msg.Ns3penvMsgInterfaceImpl],
        handleFinish: bool = False,
        useVector: bool = False,
        vectorSize: int | None = None,
        shmSize: int = 4096,
        segName: str = "My Seg",
        cpp2pyMsgName: str = "My Cpp to Python Msg",
        py2cppMsgName: str = "My Python to Cpp Msg",
        lockableName: str = "My Lockable",
    ):
        if self._created:
            raise Exception("ns3penv_utils: Error: Experiment is singleton")
        self._created = True
        self.targetName = targetName  # ns-3 target name, not file name
        os.chdir(ns3Path)
        self.handleFinish = handleFinish
        self.useVector = useVector
        self.vectorSize = vectorSize
        self.shmSize = shmSize
        self.segName = segName
        self.cpp2pyMsgName = cpp2pyMsgName
        self.py2cppMsgName = py2cppMsgName
        self.lockableName = lockableName

        # FIXME: msg module is not any module, how to type it?
        # one way is to add a protocol
        self.msgInterface = msgType(
            True,
            self.useVector,
            self.handleFinish,
            self.shmSize,
            self.segName,
            self.cpp2pyMsgName,
            self.py2cppMsgName,
            self.lockableName,
        )
        self.proc = None
        self.simCmd = None
        print("ns3penv_utils: Experiment initialized")

    def __del__(self):
        self.kill()
        # should i kill it?
        # self.msgInterface.CleanSharedMemory()
        del self.msgInterface
        print("ns3penv_utils: Experiment destroyed")

    # run ns3 script in cmd with the setting being input
    # \param[in] setting : ns3 script input parameters(default : None)
    # \param[in] show_output : whether to show output or not(default : False)
    def run(
        self,
        setting: dict[str, str | int | float] | None = None,
        show_output: bool = False,
    ) -> msg.Ns3penvMsgInterfaceImpl:
        self.kill()
        self.simCmd, self.proc = run_single_ns3(
            "./", self.targetName, setting=setting, show_output=show_output
        )
        print("ns3penv_utils: Running ns-3 with: ", self.simCmd)
        # exit if an early error occurred, such as wrong target name
        time.sleep(SIMULATION_EARLY_ENDING)
        if not self.isalive():
            print("ns3penv_utils: Subprocess died very early")
            exit(1)
        signal.signal(signal.SIGINT, sigint_handler)
        return self.msgInterface

    def kill(self) -> None:
        if self.proc is not None and self.isalive():
            kill_proc_tree(self.proc, timeout=None, on_terminate=None)
            self.proc = None
            self.simCmd = None

    def isalive(self) -> bool:
        return self.proc.poll() is None if self.proc is not None else False


__all__ = ["Experiment"]
