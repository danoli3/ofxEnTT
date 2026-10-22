meta:
	ADDON_NAME = ofxEnTT
	ADDON_DESCRIPTION = EnTT entity component system for openFrameworks: world, transform hierarchy, instanced rendering, JSON, and ofParameter / ofxGui / ofxImGui / ofxAssimpModelLoader bridges
	ADDON_AUTHOR = Dan Rosser
	ADDON_TAGS = "ecs" "entt" "entity component system" "scene" "instancing"
	ADDON_URL = https://github.com/danoli3/ofxEnTT

common:
	# EnTT 4 needs C++20. The bridges in src/bridges are header-only: include
	# them (and add their addon) only where they're used.
	ADDON_INCLUDES = libs/entt/include
	ADDON_INCLUDES += src
	ADDON_INCLUDES += src/bridges

# these build as C++17 by default; EnTT 4 needs C++20
msys2:
	ADDON_CPPFLAGS = -std=gnu++20
linuxarmv6l:
	ADDON_CPPFLAGS = -std=c++20
linuxarmv7l:
	ADDON_CPPFLAGS = -std=c++20
