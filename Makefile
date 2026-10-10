# The repository's fast checks (the ROM itself is built by build.sh, the port by cport/Makefile).
#   make check   the commit gate on the whole working tree: lint, symbol database and stamps, tools/wf tests
#   make hooks   install the gate as the pre-commit hook (tools/install_hooks.sh)
.PHONY: check hooks
check:
	python3 tools/gate.py --all
hooks:
	tools/install_hooks.sh
