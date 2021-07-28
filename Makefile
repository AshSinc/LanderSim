# CMAKE generated file: DO NOT EDIT!
# Generated by "Unix Makefiles" Generator, CMake Version 3.21

# Default target executed when no arguments are given to make.
default_target: all
.PHONY : default_target

# Allow only one "make -f Makefile2" at a time, but pass parallelism.
.NOTPARALLEL:

#=============================================================================
# Special targets provided by cmake.

# Disable implicit rules so canonical targets will work.
.SUFFIXES:

# Disable VCS-based implicit rules.
% : %,v

# Disable VCS-based implicit rules.
% : RCS/%

# Disable VCS-based implicit rules.
% : RCS/%,v

# Disable VCS-based implicit rules.
% : SCCS/s.%

# Disable VCS-based implicit rules.
% : s.%

.SUFFIXES: .hpux_make_needs_suffix_list

# Command-line flag to silence nested $(MAKE).
$(VERBOSE)MAKESILENT = -s

#Suppress display of executed commands.
$(VERBOSE).SILENT:

# A target that is always out of date.
cmake_force:
.PHONY : cmake_force

#=============================================================================
# Set environment variables for the build.

# The shell in which to execute make rules.
SHELL = /bin/sh

# The CMake executable.
CMAKE_COMMAND = /opt/cmake-3.21.0-linux-x86_64/bin/cmake

# The command to remove a file.
RM = /opt/cmake-3.21.0-linux-x86_64/bin/cmake -E rm -f

# Escaping for special characters.
EQUALS = =

# The top-level source directory on which CMake was run.
CMAKE_SOURCE_DIR = /home/ash/projects/C++/LanderSim

# The top-level build directory on which CMake was run.
CMAKE_BINARY_DIR = /home/ash/projects/C++/LanderSim

#=============================================================================
# Targets provided globally by CMake.

# Special rule for the target rebuild_cache
rebuild_cache:
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --cyan "Running CMake to regenerate build system..."
	/opt/cmake-3.21.0-linux-x86_64/bin/cmake --regenerate-during-build -S$(CMAKE_SOURCE_DIR) -B$(CMAKE_BINARY_DIR)
.PHONY : rebuild_cache

# Special rule for the target rebuild_cache
rebuild_cache/fast: rebuild_cache
.PHONY : rebuild_cache/fast

# Special rule for the target edit_cache
edit_cache:
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --cyan "Running CMake cache editor..."
	/opt/cmake-3.21.0-linux-x86_64/bin/ccmake -S$(CMAKE_SOURCE_DIR) -B$(CMAKE_BINARY_DIR)
.PHONY : edit_cache

# Special rule for the target edit_cache
edit_cache/fast: edit_cache
.PHONY : edit_cache/fast

# The main all target
all: cmake_check_build_system
	$(CMAKE_COMMAND) -E cmake_progress_start /home/ash/projects/C++/LanderSim/CMakeFiles /home/ash/projects/C++/LanderSim//CMakeFiles/progress.marks
	$(MAKE) $(MAKESILENT) -f CMakeFiles/Makefile2 all
	$(CMAKE_COMMAND) -E cmake_progress_start /home/ash/projects/C++/LanderSim/CMakeFiles 0
.PHONY : all

# The main clean target
clean:
	$(MAKE) $(MAKESILENT) -f CMakeFiles/Makefile2 clean
.PHONY : clean

# The main clean target
clean/fast: clean
.PHONY : clean/fast

# Prepare targets for installation.
preinstall: all
	$(MAKE) $(MAKESILENT) -f CMakeFiles/Makefile2 preinstall
.PHONY : preinstall

# Prepare targets for installation.
preinstall/fast:
	$(MAKE) $(MAKESILENT) -f CMakeFiles/Makefile2 preinstall
.PHONY : preinstall/fast

# clear depends
depend:
	$(CMAKE_COMMAND) -P /home/ash/projects/C++/LanderSim/CMakeFiles/VerifyGlobs.cmake
	$(CMAKE_COMMAND) -S$(CMAKE_SOURCE_DIR) -B$(CMAKE_BINARY_DIR) --check-build-system CMakeFiles/Makefile.cmake 1
.PHONY : depend

#=============================================================================
# Target rules for targets named main

# Build rule for target.
main: cmake_check_build_system
	$(MAKE) $(MAKESILENT) -f CMakeFiles/Makefile2 main
.PHONY : main

# fast build rule for target.
main/fast:
	$(MAKE) $(MAKESILENT) -f CMakeFiles/main.dir/build.make CMakeFiles/main.dir/build
.PHONY : main/fast

src/Domain/dmn_myScene.o: src/Domain/dmn_myScene.cpp.o
.PHONY : src/Domain/dmn_myScene.o

# target to build an object file
src/Domain/dmn_myScene.cpp.o:
	$(MAKE) $(MAKESILENT) -f CMakeFiles/main.dir/build.make CMakeFiles/main.dir/src/Domain/dmn_myScene.cpp.o
.PHONY : src/Domain/dmn_myScene.cpp.o

src/Domain/dmn_myScene.i: src/Domain/dmn_myScene.cpp.i
.PHONY : src/Domain/dmn_myScene.i

# target to preprocess a source file
src/Domain/dmn_myScene.cpp.i:
	$(MAKE) $(MAKESILENT) -f CMakeFiles/main.dir/build.make CMakeFiles/main.dir/src/Domain/dmn_myScene.cpp.i
.PHONY : src/Domain/dmn_myScene.cpp.i

src/Domain/dmn_myScene.s: src/Domain/dmn_myScene.cpp.s
.PHONY : src/Domain/dmn_myScene.s

# target to generate assembly for a file
src/Domain/dmn_myScene.cpp.s:
	$(MAKE) $(MAKESILENT) -f CMakeFiles/main.dir/build.make CMakeFiles/main.dir/src/Domain/dmn_myScene.cpp.s
.PHONY : src/Domain/dmn_myScene.cpp.s

src/Domain/world_camera.o: src/Domain/world_camera.cpp.o
.PHONY : src/Domain/world_camera.o

# target to build an object file
src/Domain/world_camera.cpp.o:
	$(MAKE) $(MAKESILENT) -f CMakeFiles/main.dir/build.make CMakeFiles/main.dir/src/Domain/world_camera.cpp.o
.PHONY : src/Domain/world_camera.cpp.o

src/Domain/world_camera.i: src/Domain/world_camera.cpp.i
.PHONY : src/Domain/world_camera.i

# target to preprocess a source file
src/Domain/world_camera.cpp.i:
	$(MAKE) $(MAKESILENT) -f CMakeFiles/main.dir/build.make CMakeFiles/main.dir/src/Domain/world_camera.cpp.i
.PHONY : src/Domain/world_camera.cpp.i

src/Domain/world_camera.s: src/Domain/world_camera.cpp.s
.PHONY : src/Domain/world_camera.s

# target to generate assembly for a file
src/Domain/world_camera.cpp.s:
	$(MAKE) $(MAKESILENT) -f CMakeFiles/main.dir/build.make CMakeFiles/main.dir/src/Domain/world_camera.cpp.s
.PHONY : src/Domain/world_camera.cpp.s

src/Domain/world_input.o: src/Domain/world_input.cpp.o
.PHONY : src/Domain/world_input.o

# target to build an object file
src/Domain/world_input.cpp.o:
	$(MAKE) $(MAKESILENT) -f CMakeFiles/main.dir/build.make CMakeFiles/main.dir/src/Domain/world_input.cpp.o
.PHONY : src/Domain/world_input.cpp.o

src/Domain/world_input.i: src/Domain/world_input.cpp.i
.PHONY : src/Domain/world_input.i

# target to preprocess a source file
src/Domain/world_input.cpp.i:
	$(MAKE) $(MAKESILENT) -f CMakeFiles/main.dir/build.make CMakeFiles/main.dir/src/Domain/world_input.cpp.i
.PHONY : src/Domain/world_input.cpp.i

src/Domain/world_input.s: src/Domain/world_input.cpp.s
.PHONY : src/Domain/world_input.s

# target to generate assembly for a file
src/Domain/world_input.cpp.s:
	$(MAKE) $(MAKESILENT) -f CMakeFiles/main.dir/build.make CMakeFiles/main.dir/src/Domain/world_input.cpp.s
.PHONY : src/Domain/world_input.cpp.s

src/Domain/world_state.o: src/Domain/world_state.cpp.o
.PHONY : src/Domain/world_state.o

# target to build an object file
src/Domain/world_state.cpp.o:
	$(MAKE) $(MAKESILENT) -f CMakeFiles/main.dir/build.make CMakeFiles/main.dir/src/Domain/world_state.cpp.o
.PHONY : src/Domain/world_state.cpp.o

src/Domain/world_state.i: src/Domain/world_state.cpp.i
.PHONY : src/Domain/world_state.i

# target to preprocess a source file
src/Domain/world_state.cpp.i:
	$(MAKE) $(MAKESILENT) -f CMakeFiles/main.dir/build.make CMakeFiles/main.dir/src/Domain/world_state.cpp.i
.PHONY : src/Domain/world_state.cpp.i

src/Domain/world_state.s: src/Domain/world_state.cpp.s
.PHONY : src/Domain/world_state.s

# target to generate assembly for a file
src/Domain/world_state.cpp.s:
	$(MAKE) $(MAKESILENT) -f CMakeFiles/main.dir/build.make CMakeFiles/main.dir/src/Domain/world_state.cpp.s
.PHONY : src/Domain/world_state.cpp.s

src/Service/sv_meshBuilder.o: src/Service/sv_meshBuilder.cpp.o
.PHONY : src/Service/sv_meshBuilder.o

# target to build an object file
src/Service/sv_meshBuilder.cpp.o:
	$(MAKE) $(MAKESILENT) -f CMakeFiles/main.dir/build.make CMakeFiles/main.dir/src/Service/sv_meshBuilder.cpp.o
.PHONY : src/Service/sv_meshBuilder.cpp.o

src/Service/sv_meshBuilder.i: src/Service/sv_meshBuilder.cpp.i
.PHONY : src/Service/sv_meshBuilder.i

# target to preprocess a source file
src/Service/sv_meshBuilder.cpp.i:
	$(MAKE) $(MAKESILENT) -f CMakeFiles/main.dir/build.make CMakeFiles/main.dir/src/Service/sv_meshBuilder.cpp.i
.PHONY : src/Service/sv_meshBuilder.cpp.i

src/Service/sv_meshBuilder.s: src/Service/sv_meshBuilder.cpp.s
.PHONY : src/Service/sv_meshBuilder.s

# target to generate assembly for a file
src/Service/sv_meshBuilder.cpp.s:
	$(MAKE) $(MAKESILENT) -f CMakeFiles/main.dir/build.make CMakeFiles/main.dir/src/Service/sv_meshBuilder.cpp.s
.PHONY : src/Service/sv_meshBuilder.cpp.s

src/Ui/ui_handler.o: src/Ui/ui_handler.cpp.o
.PHONY : src/Ui/ui_handler.o

# target to build an object file
src/Ui/ui_handler.cpp.o:
	$(MAKE) $(MAKESILENT) -f CMakeFiles/main.dir/build.make CMakeFiles/main.dir/src/Ui/ui_handler.cpp.o
.PHONY : src/Ui/ui_handler.cpp.o

src/Ui/ui_handler.i: src/Ui/ui_handler.cpp.i
.PHONY : src/Ui/ui_handler.i

# target to preprocess a source file
src/Ui/ui_handler.cpp.i:
	$(MAKE) $(MAKESILENT) -f CMakeFiles/main.dir/build.make CMakeFiles/main.dir/src/Ui/ui_handler.cpp.i
.PHONY : src/Ui/ui_handler.cpp.i

src/Ui/ui_handler.s: src/Ui/ui_handler.cpp.s
.PHONY : src/Ui/ui_handler.s

# target to generate assembly for a file
src/Ui/ui_handler.cpp.s:
	$(MAKE) $(MAKESILENT) -f CMakeFiles/main.dir/build.make CMakeFiles/main.dir/src/Ui/ui_handler.cpp.s
.PHONY : src/Ui/ui_handler.cpp.s

src/Vk/vk_images.o: src/Vk/vk_images.cpp.o
.PHONY : src/Vk/vk_images.o

# target to build an object file
src/Vk/vk_images.cpp.o:
	$(MAKE) $(MAKESILENT) -f CMakeFiles/main.dir/build.make CMakeFiles/main.dir/src/Vk/vk_images.cpp.o
.PHONY : src/Vk/vk_images.cpp.o

src/Vk/vk_images.i: src/Vk/vk_images.cpp.i
.PHONY : src/Vk/vk_images.i

# target to preprocess a source file
src/Vk/vk_images.cpp.i:
	$(MAKE) $(MAKESILENT) -f CMakeFiles/main.dir/build.make CMakeFiles/main.dir/src/Vk/vk_images.cpp.i
.PHONY : src/Vk/vk_images.cpp.i

src/Vk/vk_images.s: src/Vk/vk_images.cpp.s
.PHONY : src/Vk/vk_images.s

# target to generate assembly for a file
src/Vk/vk_images.cpp.s:
	$(MAKE) $(MAKESILENT) -f CMakeFiles/main.dir/build.make CMakeFiles/main.dir/src/Vk/vk_images.cpp.s
.PHONY : src/Vk/vk_images.cpp.s

src/Vk/vk_init_queries.o: src/Vk/vk_init_queries.cpp.o
.PHONY : src/Vk/vk_init_queries.o

# target to build an object file
src/Vk/vk_init_queries.cpp.o:
	$(MAKE) $(MAKESILENT) -f CMakeFiles/main.dir/build.make CMakeFiles/main.dir/src/Vk/vk_init_queries.cpp.o
.PHONY : src/Vk/vk_init_queries.cpp.o

src/Vk/vk_init_queries.i: src/Vk/vk_init_queries.cpp.i
.PHONY : src/Vk/vk_init_queries.i

# target to preprocess a source file
src/Vk/vk_init_queries.cpp.i:
	$(MAKE) $(MAKESILENT) -f CMakeFiles/main.dir/build.make CMakeFiles/main.dir/src/Vk/vk_init_queries.cpp.i
.PHONY : src/Vk/vk_init_queries.cpp.i

src/Vk/vk_init_queries.s: src/Vk/vk_init_queries.cpp.s
.PHONY : src/Vk/vk_init_queries.s

# target to generate assembly for a file
src/Vk/vk_init_queries.cpp.s:
	$(MAKE) $(MAKESILENT) -f CMakeFiles/main.dir/build.make CMakeFiles/main.dir/src/Vk/vk_init_queries.cpp.s
.PHONY : src/Vk/vk_init_queries.cpp.s

src/Vk/vk_mesh.o: src/Vk/vk_mesh.cpp.o
.PHONY : src/Vk/vk_mesh.o

# target to build an object file
src/Vk/vk_mesh.cpp.o:
	$(MAKE) $(MAKESILENT) -f CMakeFiles/main.dir/build.make CMakeFiles/main.dir/src/Vk/vk_mesh.cpp.o
.PHONY : src/Vk/vk_mesh.cpp.o

src/Vk/vk_mesh.i: src/Vk/vk_mesh.cpp.i
.PHONY : src/Vk/vk_mesh.i

# target to preprocess a source file
src/Vk/vk_mesh.cpp.i:
	$(MAKE) $(MAKESILENT) -f CMakeFiles/main.dir/build.make CMakeFiles/main.dir/src/Vk/vk_mesh.cpp.i
.PHONY : src/Vk/vk_mesh.cpp.i

src/Vk/vk_mesh.s: src/Vk/vk_mesh.cpp.s
.PHONY : src/Vk/vk_mesh.s

# target to generate assembly for a file
src/Vk/vk_mesh.cpp.s:
	$(MAKE) $(MAKESILENT) -f CMakeFiles/main.dir/build.make CMakeFiles/main.dir/src/Vk/vk_mesh.cpp.s
.PHONY : src/Vk/vk_mesh.cpp.s

src/Vk/vk_pipeline.o: src/Vk/vk_pipeline.cpp.o
.PHONY : src/Vk/vk_pipeline.o

# target to build an object file
src/Vk/vk_pipeline.cpp.o:
	$(MAKE) $(MAKESILENT) -f CMakeFiles/main.dir/build.make CMakeFiles/main.dir/src/Vk/vk_pipeline.cpp.o
.PHONY : src/Vk/vk_pipeline.cpp.o

src/Vk/vk_pipeline.i: src/Vk/vk_pipeline.cpp.i
.PHONY : src/Vk/vk_pipeline.i

# target to preprocess a source file
src/Vk/vk_pipeline.cpp.i:
	$(MAKE) $(MAKESILENT) -f CMakeFiles/main.dir/build.make CMakeFiles/main.dir/src/Vk/vk_pipeline.cpp.i
.PHONY : src/Vk/vk_pipeline.cpp.i

src/Vk/vk_pipeline.s: src/Vk/vk_pipeline.cpp.s
.PHONY : src/Vk/vk_pipeline.s

# target to generate assembly for a file
src/Vk/vk_pipeline.cpp.s:
	$(MAKE) $(MAKESILENT) -f CMakeFiles/main.dir/build.make CMakeFiles/main.dir/src/Vk/vk_pipeline.cpp.s
.PHONY : src/Vk/vk_pipeline.cpp.s

src/Vk/vk_renderer.o: src/Vk/vk_renderer.cpp.o
.PHONY : src/Vk/vk_renderer.o

# target to build an object file
src/Vk/vk_renderer.cpp.o:
	$(MAKE) $(MAKESILENT) -f CMakeFiles/main.dir/build.make CMakeFiles/main.dir/src/Vk/vk_renderer.cpp.o
.PHONY : src/Vk/vk_renderer.cpp.o

src/Vk/vk_renderer.i: src/Vk/vk_renderer.cpp.i
.PHONY : src/Vk/vk_renderer.i

# target to preprocess a source file
src/Vk/vk_renderer.cpp.i:
	$(MAKE) $(MAKESILENT) -f CMakeFiles/main.dir/build.make CMakeFiles/main.dir/src/Vk/vk_renderer.cpp.i
.PHONY : src/Vk/vk_renderer.cpp.i

src/Vk/vk_renderer.s: src/Vk/vk_renderer.cpp.s
.PHONY : src/Vk/vk_renderer.s

# target to generate assembly for a file
src/Vk/vk_renderer.cpp.s:
	$(MAKE) $(MAKESILENT) -f CMakeFiles/main.dir/build.make CMakeFiles/main.dir/src/Vk/vk_renderer.cpp.s
.PHONY : src/Vk/vk_renderer.cpp.s

src/Vk/vk_structs.o: src/Vk/vk_structs.cpp.o
.PHONY : src/Vk/vk_structs.o

# target to build an object file
src/Vk/vk_structs.cpp.o:
	$(MAKE) $(MAKESILENT) -f CMakeFiles/main.dir/build.make CMakeFiles/main.dir/src/Vk/vk_structs.cpp.o
.PHONY : src/Vk/vk_structs.cpp.o

src/Vk/vk_structs.i: src/Vk/vk_structs.cpp.i
.PHONY : src/Vk/vk_structs.i

# target to preprocess a source file
src/Vk/vk_structs.cpp.i:
	$(MAKE) $(MAKESILENT) -f CMakeFiles/main.dir/build.make CMakeFiles/main.dir/src/Vk/vk_structs.cpp.i
.PHONY : src/Vk/vk_structs.cpp.i

src/Vk/vk_structs.s: src/Vk/vk_structs.cpp.s
.PHONY : src/Vk/vk_structs.s

# target to generate assembly for a file
src/Vk/vk_structs.cpp.s:
	$(MAKE) $(MAKESILENT) -f CMakeFiles/main.dir/build.make CMakeFiles/main.dir/src/Vk/vk_structs.cpp.s
.PHONY : src/Vk/vk_structs.cpp.s

src/Vk/vk_windowHandler.o: src/Vk/vk_windowHandler.cpp.o
.PHONY : src/Vk/vk_windowHandler.o

# target to build an object file
src/Vk/vk_windowHandler.cpp.o:
	$(MAKE) $(MAKESILENT) -f CMakeFiles/main.dir/build.make CMakeFiles/main.dir/src/Vk/vk_windowHandler.cpp.o
.PHONY : src/Vk/vk_windowHandler.cpp.o

src/Vk/vk_windowHandler.i: src/Vk/vk_windowHandler.cpp.i
.PHONY : src/Vk/vk_windowHandler.i

# target to preprocess a source file
src/Vk/vk_windowHandler.cpp.i:
	$(MAKE) $(MAKESILENT) -f CMakeFiles/main.dir/build.make CMakeFiles/main.dir/src/Vk/vk_windowHandler.cpp.i
.PHONY : src/Vk/vk_windowHandler.cpp.i

src/Vk/vk_windowHandler.s: src/Vk/vk_windowHandler.cpp.s
.PHONY : src/Vk/vk_windowHandler.s

# target to generate assembly for a file
src/Vk/vk_windowHandler.cpp.s:
	$(MAKE) $(MAKESILENT) -f CMakeFiles/main.dir/build.make CMakeFiles/main.dir/src/Vk/vk_windowHandler.cpp.s
.PHONY : src/Vk/vk_windowHandler.cpp.s

src/main.o: src/main.cpp.o
.PHONY : src/main.o

# target to build an object file
src/main.cpp.o:
	$(MAKE) $(MAKESILENT) -f CMakeFiles/main.dir/build.make CMakeFiles/main.dir/src/main.cpp.o
.PHONY : src/main.cpp.o

src/main.i: src/main.cpp.i
.PHONY : src/main.i

# target to preprocess a source file
src/main.cpp.i:
	$(MAKE) $(MAKESILENT) -f CMakeFiles/main.dir/build.make CMakeFiles/main.dir/src/main.cpp.i
.PHONY : src/main.cpp.i

src/main.s: src/main.cpp.s
.PHONY : src/main.s

# target to generate assembly for a file
src/main.cpp.s:
	$(MAKE) $(MAKESILENT) -f CMakeFiles/main.dir/build.make CMakeFiles/main.dir/src/main.cpp.s
.PHONY : src/main.cpp.s

# Help Target
help:
	@echo "The following are some of the valid targets for this Makefile:"
	@echo "... all (the default if no target is provided)"
	@echo "... clean"
	@echo "... depend"
	@echo "... edit_cache"
	@echo "... rebuild_cache"
	@echo "... main"
	@echo "... src/Domain/dmn_myScene.o"
	@echo "... src/Domain/dmn_myScene.i"
	@echo "... src/Domain/dmn_myScene.s"
	@echo "... src/Domain/world_camera.o"
	@echo "... src/Domain/world_camera.i"
	@echo "... src/Domain/world_camera.s"
	@echo "... src/Domain/world_input.o"
	@echo "... src/Domain/world_input.i"
	@echo "... src/Domain/world_input.s"
	@echo "... src/Domain/world_state.o"
	@echo "... src/Domain/world_state.i"
	@echo "... src/Domain/world_state.s"
	@echo "... src/Service/sv_meshBuilder.o"
	@echo "... src/Service/sv_meshBuilder.i"
	@echo "... src/Service/sv_meshBuilder.s"
	@echo "... src/Ui/ui_handler.o"
	@echo "... src/Ui/ui_handler.i"
	@echo "... src/Ui/ui_handler.s"
	@echo "... src/Vk/vk_images.o"
	@echo "... src/Vk/vk_images.i"
	@echo "... src/Vk/vk_images.s"
	@echo "... src/Vk/vk_init_queries.o"
	@echo "... src/Vk/vk_init_queries.i"
	@echo "... src/Vk/vk_init_queries.s"
	@echo "... src/Vk/vk_mesh.o"
	@echo "... src/Vk/vk_mesh.i"
	@echo "... src/Vk/vk_mesh.s"
	@echo "... src/Vk/vk_pipeline.o"
	@echo "... src/Vk/vk_pipeline.i"
	@echo "... src/Vk/vk_pipeline.s"
	@echo "... src/Vk/vk_renderer.o"
	@echo "... src/Vk/vk_renderer.i"
	@echo "... src/Vk/vk_renderer.s"
	@echo "... src/Vk/vk_structs.o"
	@echo "... src/Vk/vk_structs.i"
	@echo "... src/Vk/vk_structs.s"
	@echo "... src/Vk/vk_windowHandler.o"
	@echo "... src/Vk/vk_windowHandler.i"
	@echo "... src/Vk/vk_windowHandler.s"
	@echo "... src/main.o"
	@echo "... src/main.i"
	@echo "... src/main.s"
.PHONY : help



#=============================================================================
# Special targets to cleanup operation of make.

# Special rule to run CMake to check the build system integrity.
# No rule that depends on this can have commands that come from listfiles
# because they might be regenerated.
cmake_check_build_system:
	$(CMAKE_COMMAND) -P /home/ash/projects/C++/LanderSim/CMakeFiles/VerifyGlobs.cmake
	$(CMAKE_COMMAND) -S$(CMAKE_SOURCE_DIR) -B$(CMAKE_BINARY_DIR) --check-build-system CMakeFiles/Makefile.cmake 0
.PHONY : cmake_check_build_system

