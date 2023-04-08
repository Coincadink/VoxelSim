-- premake5.lua
workspace "VoxelSim"
   architecture "x64"
   configurations { "Debug", "Release", "Dist" }
   startproject "VoxelSim"

outputdir = "%{cfg.buildcfg}-%{cfg.system}-%{cfg.architecture}"
include "Walnut/WalnutExternal.lua"

include "VoxelSim"