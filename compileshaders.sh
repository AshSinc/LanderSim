#!/bin/bash

#A helper script to compile vert and frag glsl shaders to spv format
#uses glslc program which is included with Vulkan SDK to achieve this

SRC_PATH="shaders/src"
DST_PATH="shaders"

$VULKAN_SDK/bin/glslc $SRC_PATH/default_lit.vert -o $DST_PATH/default_lit_vert.spv
$VULKAN_SDK/bin/glslc $SRC_PATH/default_lit.frag -o $DST_PATH/default_lit_frag.spv

$VULKAN_SDK/bin/glslc $SRC_PATH/textured_lit.frag -o $DST_PATH/textured_lit_frag.spv

$VULKAN_SDK/bin/glslc $SRC_PATH/unlit.frag -o $DST_PATH/unlit_frag.spv

$VULKAN_SDK/bin/glslc $SRC_PATH/skybox.vert -o $DST_PATH/skybox_vert.spv
$VULKAN_SDK/bin/glslc $SRC_PATH/skybox.frag -o $DST_PATH/skybox_frag.spv