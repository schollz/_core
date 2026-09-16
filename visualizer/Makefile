.DEFAULT_GOAL := serve

.PHONY: serve build

REFERENCE ?= reference
PORT ?= 4173

# Build the app, then analyse the selected reference folder at server startup.
serve: build
	npm start -- "$(REFERENCE)" --port "$(PORT)"

build: node_modules/.visualizer-installed
	npm run build

node_modules/.visualizer-installed: package.json package-lock.json
	npm ci
	touch $@
