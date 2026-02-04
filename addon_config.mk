meta:
	ADDON_NAME = ofxPipeWire
	ADDON_DESCRIPTION = PipeWire video/audio streaming addon for openFrameworks (Linux)
	ADDON_AUTHOR = You + Codex
	ADDON_TAGS = "PipeWire" "video" "audio" "streaming" "linux"
	ADDON_URL = https://pipewire.org

common:
	# ADDON_DEPENDENCIES =
	# ADDON_INCLUDES =
	# ADDON_CFLAGS =
	# ADDON_LDFLAGS =
	# ADDON_PKG_CONFIG_LIBRARIES =
	# ADDON_FRAMEWORKS =
	# ADDON_SOURCES =
	# ADDON_DATA =
	# ADDON_LIBS_EXCLUDE =
	# ADDON_SOURCES_EXCLUDE =
	# ADDON_INCLUDES_EXCLUDE =

linux64:
	ADDON_PKG_CONFIG_LIBRARIES = libpipewire-0.3

linux:
	ADDON_PKG_CONFIG_LIBRARIES = libpipewire-0.3

linuxarmv6l:
	ADDON_PKG_CONFIG_LIBRARIES = libpipewire-0.3

linuxarmv7l:
	ADDON_PKG_CONFIG_LIBRARIES = libpipewire-0.3

linuxaarch64:
	ADDON_PKG_CONFIG_LIBRARIES = libpipewire-0.3
