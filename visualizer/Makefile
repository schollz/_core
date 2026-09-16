.DEFAULT_GOAL := serve

.PHONY: serve build

# Rebuild on every invocation so changes in reference are always included.
serve: build
	npm run preview

build: node_modules/.visualizer-installed
	npm run build

node_modules/.visualizer-installed: package.json package-lock.json
	npm ci
	touch $@
