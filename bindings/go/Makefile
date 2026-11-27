# Sets CGO_LDFLAGS from NRE_LIB_DIR (cgo directives can't read environment variables) and runs go.
#   make test | make vet | make run      NRE_LIB_DIR=/path/to/dir/with/libnre_c.so make run
NRE_LIB_DIR ?= $(abspath $(CURDIR)/../../cmake-build-debug/bin)
export CGO_LDFLAGS := -L$(NRE_LIB_DIR) -Wl,-rpath,$(NRE_LIB_DIR) $(CGO_LDFLAGS)

.PHONY: test vet build run clean
test:
	go test ./...
vet:
	go vet ./...
build:
	go build -o build/orbit ./examples/orbit
run: build
	./build/orbit
clean:
	rm -rf build
