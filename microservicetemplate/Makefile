PROJECT_NAME := microservicetemplate

CMD_DIR := cmd
COMPILER ?= ../cmake-build-debug/aura-compiler

RED := \033[0;31m
GREEN := \033[0;32m
YELLOW := \033[0;33m
BLUE := \033[0;34m
NC := \033[0m

.PHONY: all check run migrate showcase test help
.PHONY: test-e2e docker-build docker-migrate docker-up docker-down

all: help

check:
	@echo "$(BLUE)Checking $(PROJECT_NAME) entrypoints...$(NC)"
	$(COMPILER) check ./$(CMD_DIR)/main.aura
	$(COMPILER) check ./$(CMD_DIR)/migrate.aura
	$(COMPILER) check ./$(CMD_DIR)/showcase.aura

run:
	@echo "$(BLUE)Running $(PROJECT_NAME)...$(NC)"
	$(COMPILER) run ./$(CMD_DIR)/main.aura

migrate:
	@echo "$(BLUE)Running Aura migration bootstrap for $(PROJECT_NAME)...$(NC)"
	$(COMPILER) run ./$(CMD_DIR)/migrate.aura

showcase:
	@echo "$(BLUE)Running Aura language showcase...$(NC)"
	$(COMPILER) run ./$(CMD_DIR)/showcase.aura

test: check showcase

test-e2e:
	./scripts/e2e.sh

docker-build:
	@echo "$(BLUE)Building $(PROJECT_NAME) container...$(NC)"
	docker build -f ./Dockerfile -t $(PROJECT_NAME):latest ..

docker-migrate:
	@echo "$(BLUE)Running dockerized migration for $(PROJECT_NAME)...$(NC)"
	docker compose -f ./docker-compose.yml run --rm migrate

docker-up:
	@echo "$(BLUE)Starting $(PROJECT_NAME) stack...$(NC)"
	docker compose -f ./docker-compose.yml up app

docker-down:
	@echo "$(BLUE)Stopping $(PROJECT_NAME) stack...$(NC)"
	docker compose -f ./docker-compose.yml down -v

help:
	@echo "$(BLUE)$(PROJECT_NAME) Makefile$(NC)"
	@echo ""
	@echo "Usage:"
	@echo "  make [target]"
	@echo ""
	@echo "Targets:"
	@echo "  $(GREEN)check$(NC)        - Compile all Aura entrypoints without running them"
	@echo "  $(GREEN)run$(NC)          - Compile and run the Aura service"
	@echo "  $(GREEN)migrate$(NC)      - Run the Aura bootstrap migration for tasks"
	@echo "  $(GREEN)showcase$(NC)     - Run the Aura language feature showcase"
	@echo "  $(GREEN)test$(NC)         - Check entrypoints and execute the showcase"
	@echo "  $(GREEN)test-e2e$(NC)     - Run the Docker Compose JWT and CRUD test"
	@echo "  $(GREEN)docker-build$(NC) - Build the microservice container image"
	@echo "  $(GREEN)docker-migrate$(NC)- Run the migration container against compose MySQL"
	@echo "  $(GREEN)docker-up$(NC)    - Start the compose app stack"
	@echo "  $(GREEN)docker-down$(NC)  - Stop the compose stack and remove volumes"
	@echo "  $(GREEN)help$(NC)         - Show this help message"
