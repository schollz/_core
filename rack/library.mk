# Root entry points for the VCV Library; the plugin itself stays in rack/.
RACK_LIBRARY_GOALS := all dep cleandep dist cleandist install
RACK_LIBRARY_SDK = $(if $(strip $(RACK_DIR)),RACK_DIR="$(abspath $(RACK_DIR))")
RACK_LIBRARY_CLEAN := $(strip $(RACK_DIR)$(filter $(RACK_LIBRARY_GOALS) install-vcv,$(MAKECMDGOALS)))

.PHONY: $(RACK_LIBRARY_GOALS)
all dep cleandep install:
	$(MAKE) -C rack $(RACK_LIBRARY_SDK) $@

# VCV collects packages from the source root. Never leave an older package
# there when the current build fails, and keep rack/dist for existing releases.
dist:
	rm -rf dist
	$(MAKE) -C rack $(RACK_LIBRARY_SDK) dist
	mkdir -p dist
	cp rack/dist/*.vcvplugin dist/

cleandist:
	rm -rf dist
	$(MAKE) -C rack $(RACK_LIBRARY_SDK) cleandist

ifneq ($(RACK_LIBRARY_CLEAN),)
clean:
	rm -rf dist
	$(MAKE) -C rack $(RACK_LIBRARY_SDK) clean
endif
