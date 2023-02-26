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
	ln -s ../../frontend/dist/build/isqc1/isqc1 isqc1

mlir: check-env
	cd mlir && mkdir -p build && cd build && cmake ../ -GNinja && ninja
	cd ${ISQ_ROOT}/bin && \
	rm -f isq-opt && \
	ln -s ../../mlir/build/tools/isq-opt isq-opt

isqc: check-env
	cd isqc && cargo build;
	cd ${ISQ_ROOT}/bin && \
	rm -f isqc && \
	ln -s ../../isqc/target/debug/isqc isqc

simulator: check-env
	cd simulator &&	cargo build
	cd ${ISQ_ROOT}/bin && \
	rm -f simulator && \
	ln -s ../../simulator/target/debug/simulator simulator

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

clean: check-env
	rm .build -r
	mkdir -p .build
develop:
	@exec nix develop
lock:
	cd vendor && nix flake lock --update-input isqc-base
	cd simulator && nix flake lock --update-input isqc-base --update-input mlir
	cd isqc && nix flake lock --update-input isqc-base
	cd mlir && nix flake lock --update-input isqc-base --update-input vendor
	cd frontend && nix flake lock --update-input isqc-base
	cd docs && nix flake lock --update-input isqc-base
	nix flake lock --update-input isqc-base --update-input vendor --update-input isqc1 --update-input isq-opt --update-input isqc-driver --update-input isq-simulator --update-input isqc-docs
