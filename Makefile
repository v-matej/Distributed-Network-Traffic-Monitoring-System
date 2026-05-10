BACKEND_DIR=backend
BACKEND_BUILD_DIR=$(BACKEND_DIR)/build

FRONTEND_DIR=frontend/controller-ui

.PHONY: help \
        build rebuild clean-all \
        backend-build backend-clean backend-rebuild \
        backend-run-agent backend-run-controller backend-run-sniffer \
        frontend-install frontend-dev frontend-build frontend-preview frontend-clean \
        clean

help:
	@echo "Available targets:"
	@echo "  build                  Alias for backend-build"
	@echo "  rebuild                Alias for backend-rebuild"
	@echo "  backend-build          Configure and build the C++ backend"
	@echo "  backend-clean          Remove backend build directory"
	@echo "  backend-rebuild        Clean and rebuild backend"
	@echo "  backend-run-agent      Run agent_server from backend/"
	@echo "  backend-run-controller Run controller_server from backend/"
	@echo "  backend-run-sniffer    Run packet_sniffer --list from backend/"
	@echo "  frontend-install       Install frontend dependencies"
	@echo "  frontend-dev           Start frontend dev server"
	@echo "  frontend-build         Build frontend for production"
	@echo "  frontend-preview       Preview frontend production build"
	@echo "  frontend-clean         Remove frontend node_modules and dist"
	@echo "  clean                  Clean backend and frontend build artifacts"

build: backend-build
rebuild: backend-rebuild
clean-all: clean

backend-build:
	cmake -S $(BACKEND_DIR) -B $(BACKEND_BUILD_DIR)
	cmake --build $(BACKEND_BUILD_DIR)

backend-clean:
	rm -rf $(BACKEND_BUILD_DIR)

backend-rebuild: backend-clean backend-build

backend-run-agent:
	cd $(BACKEND_DIR) && sudo ./build/agent_server

backend-run-controller:
	cd $(BACKEND_DIR) && ./build/controller_server

backend-run-sniffer:
	cd $(BACKEND_DIR) && sudo ./build/packet_sniffer --list

frontend-install:
	@test -f $(FRONTEND_DIR)/package.json || (echo "Frontend not initialized. Run: npm create vite@latest frontend/controller-ui -- --template react-ts" && exit 1)
	cd $(FRONTEND_DIR) && npm install

frontend-dev:
	@test -f $(FRONTEND_DIR)/package.json || (echo "Frontend not initialized. Run: npm create vite@latest frontend/controller-ui -- --template react-ts" && exit 1)
	cd $(FRONTEND_DIR) && npm run dev

frontend-build:
	@test -f $(FRONTEND_DIR)/package.json || (echo "Frontend not initialized. Run: npm create vite@latest frontend/controller-ui -- --template react-ts" && exit 1)
	cd $(FRONTEND_DIR) && npm run build

frontend-preview:
	@test -f $(FRONTEND_DIR)/package.json || (echo "Frontend not initialized. Run: npm create vite@latest frontend/controller-ui -- --template react-ts" && exit 1)
	cd $(FRONTEND_DIR) && npm run preview

frontend-clean:
	rm -rf $(FRONTEND_DIR)/node_modules $(FRONTEND_DIR)/dist

clean: backend-clean frontend-clean
