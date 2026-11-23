.PHONY: deps build test run probe install deb browser clean

deps:
	./scripts/install-deps.sh

build:
	./scripts/build.sh

test:
	./scripts/test-core.sh

run: build
	./build/sys-monit-anime

probe: build
	./build/sys-monit-anime-probe

install: build
	./scripts/install.sh

deb: build
	./scripts/package-deb.sh

browser: build
	./scripts/install-browser-companion.sh

clean:
	cmake -E remove_directory build
	cmake -E remove_directory build-core
