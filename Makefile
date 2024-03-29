all: frontend mlir isqc simulator isq-simulator.bc

.PHONY: check-env frontend mlir isqc simulator isq-simulator.bc all lock develop run clean upload

check-env:
ifndef ISQ_ROOT
	$(error ISQ_ROOT is undefined)
endif
	$(info $(shell mkdir -p $(ISQ_ROOT)/bin))

frontend: check-env
	cd frontend && make all
	cd ${ISQ_ROOT}/bin && \
	rm -f isqc1 && \
	ln -s ../frontend/dist/build/isqc1/isqc1 isqc1

mlir: check-env
	cd mlir && mkdir -p build && cd build && cmake ../ -GNinja && ninja
	cd ${ISQ_ROOT}/bin && \
	rm -f isq-opt && rm -f isq-codegen && \
	ln -s ../mlir/build/tools/isq-opt isq-opt && \
	ln -s ../mlir/build/tools/isq-codegen isq-codegen

isqc: check-env
	cd isqc && cargo build;
	cd ${ISQ_ROOT}/bin && \
	rm -f isqc && \
	ln -s ../target/debug/isqc isqc

simulator: check-env
	cd simulator &&	cargo build --release
	cd ${ISQ_ROOT}/bin && \
	rm -f simulator && \
	ln -s ../target/release/simulator simulator

isq-simulator.bc: check-env
	mkdir -p ${ISQ_ROOT}/share/isq-simulator;
	linker=`which llvm-link` && if [ $$linker == "" ]; then linker=$(ISQ_ROOT)/bin/llvm-link; fi \
	&& cd simulator && eval $$linker src/facades/qir/shim/qir_builtin/shim.ll src/facades/qir/shim/qsharp_core/shim.ll \
	src/facades/qir/shim/qsharp_foundation/shim.ll src/facades/qir/shim/isq/shim.ll -o ${ISQ_ROOT}/share/isq-simulator/isq-simulator.bc

upload:
	nix flake archive --json \
	| jq -r '.path,(.inputs|to_entries[].value.path)' \
	| cachix push arclight-quantum
	nix build --json \
	| jq -r '.[].outputs | to_entries[].value' \
	| cachix push arclight-quantum

cargo2nix:
	yes yes | cargo2nix
	nixpkgs-fmt Cargo.nix
clean: check-env
	rm .build -r
	mkdir -p .build
develop:
	@exec nix develop
bin:
	mkdir -p bin
	cd bin && ln -s ../frontend/dist/build/isqc1/isqc1 ./isqc1
	cd bin && ln -s ../mlir/build/tools/isq-opt ./isq-opt
	cd bin && ln -s ../mlir/build/tools/isq-example ./isq-example
