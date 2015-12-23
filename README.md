U-Sphere
========

This repository contains a reference implementation of the U-Sphere protocol. For more
information about the protocol see the following paper:

  * U-Sphere: Strengthening scalable flat-name routing for decentralized networks.
    http://dx.doi.org/10.1016/j.comnet.2015.07.006

Running the testbed
===================
In order to ease running of the distributed protocol testbed, a [Docker](https://www.docker.com)
image has been provided in the Docker registry. It contains all the dependencies that are needed
to run the testbed. By default, the image will run the `pf-b4` test, which uses a synthetic
128-node topology and performs some basic protocol tests (the `StandardTests` scenario).

After you have Docker and Docker Compose installed, you can simply start the testbed from the
top-level directory as follows (assuming `unisphere` repository is in `~/unisphere`):

```
$ cd ~/unisphere
$ docker-compose up
```

This will pull all the required images (a MongoDB database is required) and run the above
tests. The above instructions will work on any Linux distribution, which is able to run Docker
containers. For more information about installing Docker and Docker Compose, see:

  * https://docs.docker.com/engine/installation
  * https://docs.docker.com/compose/install

The scenario that is used by default will run for around 20 minutes.

Testbed configuration
=====================
The testbed is configured using a single settings file located at [`tools/settings.py`](tools/settings.py). This
file contains the cluster configuration (by default a single machine is used for everything,
which is suitable only for emulating small topologies), topology configuration and scenario
configuration. For example the `pf-b4` test run is defined under `RUNS` as follows:

```python
dict(name="pf-b4", topology="basic_single", size=128, scenario="StandardTests"),
```

This basically specifies that the `basic_single` topology generator should be used, which
is passed the `size=128` parameter. It also specifies that the testbed should run the `StandardTests`
scenario.

Scenarios are defined as C++ classes in [`apps/testbed/scenarios.cpp`](apps/testbed/scenarios.cpp) and
they may call test cases which are defined in [`apps/testbed/tests.cpp`](apps/testbed/tests.cpp).

Results
=======
After the test completes its execution, all results will be stored under a unique, randomly generated,
test run identifier (for example `5b9e0`) in the `output` folder. The results contain whatever the scenario's
test cases output (usually CSV and GraphML files).
