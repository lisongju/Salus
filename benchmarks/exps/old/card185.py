# -*- coding: future_fstrings -*-
#
# Copyright 2019 Peifeng Yu <peifeng@umich.edu>
# 
# This file is part of Salus
# (see https://github.com/SymbioticLab/Salus).
# 
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
# 
#    http://www.apache.org/licenses/LICENSE-2.0
# 
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#
"""
For debugging. Reproduces memory outage detailed in card#185.
Initially found by running exp15 with 50 jobs.

Scheduler: fair
Work conservation: True
Collected data: memory usage over time
"""
from __future__ import absolute_import, print_function, division, unicode_literals

from absl import flags

from benchmarks.driver.server.config import presets
from benchmarks.driver.workload import WTL
from benchmarks.exps import run_seq, parse_actions_from_cmd, maybe_forced_preset, Pause


FLAGS = flags.FLAGS


def case1(scfg):
    run_seq(scfg.copy(output_dir=FLAGS.save_dir),
            WTL.create("inception3", 100, 165),
            WTL.create("resnet50", 50, 798),
            WTL.create("resnet152", 75, 19),
            Pause.Wait,
            Pause.Manual,
            WTL.create("inception3", 100, 165),
            WTL.create("resnet50", 50, 798),
            WTL.create("resnet152", 75, 19))


def case2(scfg):
    run_seq(scfg.copy(output_dir=FLAGS.save_dir),
            WTL.create("resnet50", 50, 798),
            Pause.Wait,
            Pause.Manual,
            WTL.create("resnet50", 50, 798))


def test(scfg):
    run_seq(scfg.copy(output_dir=FLAGS.save_dir),
            WTL.create("inception3", 100, 165),
            WTL.create("resnet50", 50, 798),
            WTL.create("resnet152", 75, 19),
            Pause.Wait,
            WTL.create("inception3", 100, 165),
            WTL.create("resnet50", 50, 798),
            WTL.create("resnet152", 75, 19))


def main(argv):
    scfg = maybe_forced_preset(presets.Debugging)

    command = argv[0] if argv else "test"

    {
        "case1": case1,
        "case2": case2,
        "test": test,
    }[command](scfg)
