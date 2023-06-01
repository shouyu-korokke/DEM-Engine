# SBEL GPU DEM-Engine
_A Dual-GPU DEM solver with complex grain geometry support_

### Description

DEM-Engine, nicknamed _DEME_, does Discrete Element Method simulations:

- Using up to two GPUs at the same time (works great on consumer _and_ data center GPUs).
- With the particles having complex shapes represented by clumped spheres.
- With support for customizable contact force models (want to add a non-standard cohesive force, or an electrostatic repulsive force? You got this).
- With an emphasis on computational efficiency. As a rule of thumb, using 3-sphere clump elements, simulating 1 million elements for 1 million time steps takes around 1 hour on two RTX 3080s.
- Supporting a wide range of problems with flexible API designs. Deformable meshes and grain breakage can be simulated by leveraging the explicit controls given to the user.
- With support for co-simulation with other C/C++ packages, such as [Chrono](https://github.com/projectchrono/chrono).

<p>
  <img width="380" src="https://i.imgur.com/DKGlM14.jpg">
  <img width="380" src="https://i.imgur.com/c9DWwqk.gif">
</p>

<p>
  <img width="380" src="https://i.imgur.com/YOEbAd8.gif">
  <img width="380" src="https://i.imgur.com/4R25TPX.gif">
</p>

Currently _DEME_ is a C++ package with an API design similar to Chrono's, and should be easy to learn for existing Chrono users. We are building a Python wrapper for _DEME_.

You can find the movies of some of _DEME_'s demos [here](https://uwmadison.app.box.com/s/u4m9tee3k1vizf097zkq3rgv54orphyv).

You are welcome to discuss _DEME_ on [Project Chrono's forum](https://groups.google.com/g/projectchrono). 

### Licensing

This project should be treated as the collective intellectual property of the Author(s) and the University of Wisconsin - Madison. The following copyright statement should be included in any new or modified source files
```
Copyright (c) 2021, Simulation-Based Engineering Laboratory
Copyright (c) 2021, University of Wisconsin - Madison

SPDX-License-Identifier: BSD-3-Clause
```

New authors should add their name to the file `CONTRIBUTORS.md` rather than to individual copyright headers.

### Installation

On a Linux machine, install CUDA if you do not already have it. Useful installation instructions may be found [here](https://developer.nvidia.com/cuda-downloads). 

Some additional troubleshooting tips for getting CUDA ready:

- On WSL this code may be buildable (and [this](https://docs.nvidia.com/cuda/wsl-user-guide/index.html) is the guide for installing CUDA on WSL), but may not run. This is due to the [many limitations on unified memory and pinned memory support](https://docs.nvidia.com/cuda/wsl-user-guide/index.html#known-limitations-for-linux-cuda-applications) on WSL. A native Linux machine or cluster is recommended.

Once CUDA is ready, clone this project and then:

```
git submodule init
git submodule update
```

This will pull the submodule NVIDIA/jitify so that we can do runtime compilation. 

Then, one typical choice is to make a build directory in it. Then in the build directory, use `cmake` to configure the compilation. An example:

```
mkdir build
cd build
cmake -DCMAKE_BUILD_TYPE=Release ..
```

You may want to use [this information](https://askubuntu.com/questions/1203635/installing-latest-cmake-on-ubuntu-18-04-3-lts-run-via-wsl-openssl-error) if you need to update cmake to the newest. 

We suggest that you install a `cmake` GUI such as `ccmake`, and `ninja_build` generator, to better help you configure the project. In this case, the example above can be done like this alternatively:

```
mkdir build
cd build
ccmake -G Ninja ..
```

You generally do not have to change the build options in the GUI, but preferably you can change `CMAKE_BUILD_TYPE` to `Release`, and if you need to install this package as a library you can specify a `CMAKE_INSTALL_PREFIX`. 

Some additional troubleshooting tips for generating the project:

- If some dependencies such as CUB are not found, then you probably need to manually set `$PATH` and `$LD_LIBRARY_PATH`. An example is given below for a specific version of CUDA, note it may be different on your machine or cluster. You should also inspect if `nvidia-smi` and `nvcc --version` give correct returns.
```
export CPATH=/usr/local/cuda-12.0/targets/x86_64-linux/include${CPATH:+:${CPATH}}
export PATH=/usr/local/cuda-12.0/bin${PATH:+:${PATH}}
export PATH=/usr/local/cuda-12.0/lib64/cmake${PATH:+:${PATH}}
export LD_LIBRARY_PATH=/usr/local/cuda-12.0/lib64${LD_LIBRARY_PATH:+:${LD_LIBRARY_PATH}}
export CUDA_HOME=/usr/local/cuda-12.0
```

Finally, build the project.

```
ninja
```

Some additional troubleshooting tips for building the project:

- If you see some grammatical errors during compilation, such as `filesystem` not being a member of `std` or arguments not expanded with `...`, then manually setting the flag `TargetCXXStandard` to `STD_CXX17` might help.

### Examples

After the build process is done, you can start trying out the demos.

- An all-rounder beginner example featuring a bladed mixer interacting with complex shaped particles: `./src/demo/DEMdemo_Mixer`.
- A place to learn how prescribed motions work in this package, using either analytical boundaries or particle-represented boundaries: `./src/demo/DEMdemo_Centrifuge` and `./src/demo/DEMdemo_Sieve`.
- A few representative engineering experiments reproduced in DEM simulations, which potentially serve as starting points for your own DEM scripts: `/src/demo/DEMdemo_BallDrop`, `./src/demo/DEMdemo_ConePenetration`, `/src/demo/DEMdemo_Sieve`, `./src/demo/DEMdemo_Repose`.
- `./src/demo/DEMdemo_WheelDP` shows how to load a checkpointed configuration file to instantly generate a settled granular terrain, then run a drawbar-pull test on it. This demo therefore requires you to first finish the two GRCPrep demos to obtain the terrain checkpoint file. The granular terrain in these demos features DEM particles with a variety of sizes and shapes.
- `./src/demo/DEMdemo_WheelDPSimplified` is a simplified version of the previous drawbar-pull test which has no prerequisite. The terrain is simpilified to be made of only one type of irregular-shaped particles. It serves as a quick starting point for people who want to create similar experiments.
- More advanced examples showing the usage of the custom additional properties (called _wildcards_) that you can associate with the simulation entities, and use them in the force model and/or change them in simulation then deposit them into the output files: `./src/demo/DEMdemo_Indentation`.
- `./src/demo/DEMdemo_Electrostatic` simulates a pile of complex-shaped and charged granular particles interacting with a mesh that is also charged. Its purpose is to show how to define a non-local force (electrostatic force) which takes effect even when the bodies are not in contact, using a custom force model file. This idea can be extended to modeling a custom cohesion force etc.
- `./src/demo/DEMdemo_FlexibleMesh` simulates a deforming mesh interacting with DEM particles. The intension is to show that the user can extract the force pairs acting on a mesh, then update the mesh with deformation information. _DEME_ does not care how this deformation is calculated. Presumably the user can feed the forces to their own solid mechanics solver to get the deformation. _DEME_ does not come with a built-in linear solver so for simplicity, in this demo the mesh deformation is instead prescribed.
- A fun game-of-life simulator built with the package, showing the flexibility in terms of how you can use this tool: `./src/demo/DEMdemo_GameOfLife`.
- `./src/demo/DEMdemo_SolarSystem` simulates our solar system. It is yet another fun simulation that is not strictly DEM per se, but shows how to define a mid-to-long-ranged force (gravitational force) using a custom force model file.
- It is a good idea to read the comment lines at the top of the demo files to understand what they each does.

[The documentations for _DEME_](https://api.projectchrono.org/) are hosted on Chrono website (work in progress).

Some additional troubleshooting tips for running the demos:

- If errors similar to `CUDA_ERROR_UNSUPPORTED_PTX_VERSION` are encountered while you run the demos, or (rarely) the simulations proceed without detecting any contacts, then please make sure the CUDA installation is the same version as when the code is compiled.
- Used your own force model but got runtime compilation error like `expression must have pointer-to-object type but it has type "float"`, or `unknown variable "delta_time"`? Check out what we did in demo `DEMdemo_Electrostatic`. You may need to manually specify what material properties are pairwise and what contact wildcards you have using `SetMustPairwiseMatProp` and `SetPerContactWildcards`.
- Just running provided demos or a script that used to work, but the jitification of the force model failed or the simulation fails at the first kernel call (probably in `DEMCubContactDetection.cu`)? Then did you pull a new version and just re-built in-place? A new update may modify the force model, and the force model in _DEME_ are given as text files so might not be automatically copied over when the project is re-built. I am sorry for the trouble it might cause, but you can do a clean re-build from an empty directory and it should fix the problem. Do not forget to first commit your own branches' changes and relocate the data you generated in the build directory. Another solution is to copy everything in `src/DEM` to the `DEM` directory in the build directory, then everything in `src/kernel` to the `kernel` directory in the build directory, then try again.

### Limitations

_DEME_ is designed to simulate the interaction among clump-represented particles, the interaction between particles and mesh-represented bodies, as well as the interaction between particles and analytical boundaries. _DEME_ does not resolve mesh&ndash;mesh or mesh&ndash;analytical contacts.

- It is able to handle mesh-represented bodies with relatively simple physics, for example a meshed plow moving through granular materials with a prescribed velocity, or several meshed projectiles flying and hitting the granular ground. 
- However, if the bodies' physics are complex multibody problems, say it is a vehicle that has joint-connected parts and a motor with certain driving policies, or the meshed bodies have collisions among themselves that needs to be simulated, then _DEME_ alone does not have the infrastructure to handle them. But you can install _DEME_ as a library and do coupled simulations with other tools such as [Chrono](https://github.com/projectchrono/chrono), where _DEME_ is exclusively tasked with handling the granular materials and the influence they exert on the outside world (with high efficiency, of course). See the following section.

### Install as C++ library

Set the `CMAKE_INSTALL_PREFIX` flag in `cmake` GUI to your desired installation path and then 

```
ninja install
```

We provide examples of linking against both [Chrono](https://github.com/projectchrono/chrono) and _DEME_ for co-simulations in [chrono-projects](https://github.com/projectchrono/chrono-projects/tree/feature/DEME).

Assuming you know how to build `chrono-projects` linking against a Chrono installation, then the extra things that you should do to link against _DEME_ are

- Set `ENABLE_DEME_TESTS` to `ON`;
- Set `ChPF_DIR` when prompted. It should be in `<your_install_dir>/lib64/cmake/ChPF`;
- Set `DEME_DIR` when prompted. It should be in `<your_install_dir>/lib64/cmake/DEME`.

Then build the project and you should be able to run the demo scripts that demonstrate the co-simulation between _DEME_ and Chrono.

More documentations on using this package for co-simulations are being added.

#### Notes on code included from Project Chrono

This project exists independently of Chrono so developers should be sure to include the appropriate BSD license header on any code which is sourced from Chrono::GPU(DEM) or other parts of Chrono.

> #### SAMPLE header for files sourced from Chrono

> ```
> Copyright (c) 2021, SBEL GPU Development Team
> Copyright (c) 2021, University of Wisconsin - Madison
> 
> SPDX-License-Identifier: BSD-3-Clause
> 
> 
> This file contains modifications of the code authored by the Project Chrono 
> Development Team. The original license can be found below:
>
> Copyright (c) 2016, Project Chrono Development Team
> All rights reserved.
> 
> Use of this source code is governed by a BSD-style license that can be found
> in the LICENSE file at the top level of the distribution and at
> http://projectchrono.org/license-chrono.txt. A copy of the license is below.
>
> Redistribution and use in source and binary forms, with or without modification, are permitted provided that the following conditions are met:
> 
>  - Redistributions of source code must retain the above copyright notice, this list of conditions and the following disclaimer. 
>  - Redistributions in binary form must reproduce the above copyright notice, this list of conditions and the following disclaimer in the documentation and/or other materials provided with the distribution. 
>  - Neither the name of the nor the names of its contributors may be used to endorse or promote products derived from this software without specific prior written permission. 
> 
> THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
> ```
