# ============================================================
#  winuxLink 跨平台 Makefile
#  支持 Windows (MinGW) 和 Linux
#  默认编译：库 + server + client
#  单独编译：make server  或  make client
# ============================================================

# ---------- 编译器与基础选项 ----------
CXX      = g++
CXXFLAGS = -std=c++17 -Wall -Wextra -Icore 
LDFLAGS  = -static

# ---------- 平台检测 ----------
ifeq ($(OS),Windows_NT)
    PLATFORM = Windows
    RM       = del /Q /F
    RM_DIR   = rmdir /S /Q
    MKDIR    = if not exist $(1) mkdir $(1)
    EXE_EXT  = .exe
    LIB_PREFIX = lib
    LIB_EXT  = .a
    PLATFORM_LIBS = -lws2_32 -ladvapi32 -static
    SEP = \\
else
    PLATFORM = Linux
    RM       = rm -f
    RM_DIR   = rm -rf
    MKDIR    = mkdir -p $(1)
    EXE_EXT  =
    LIB_PREFIX = lib
    LIB_EXT  = .a
    PLATFORM_LIBS = -lpthread -lpam -static
    SEP = /
endif

# ---------- 目录与文件 ----------
CORE_DIR    = core
SERVER_DIR  = server
CLIENT_DIR  = client
BIN_DIR     = bin

# core 源文件（新增文件请在此添加）
CORE_SRCS = $(CORE_DIR)/Agreement.cpp \
            $(CORE_DIR)/PacketBuilder.cpp \
            $(CORE_DIR)/Transfer.cpp \
            $(CORE_DIR)/Network.cpp \
            $(CORE_DIR)/Logger.cpp \
			$(CORE_DIR)/Connector.cpp \
			$(CORE_DIR)/CmdManager.cpp \
			$(CORE_DIR)/ListCommand.cpp \
			$(CORE_DIR)/LoginCommand.cpp \
			$(CORE_DIR)/GetCommand.cpp \
			$(CORE_DIR)/PutCommand.cpp \
			$(CORE_DIR)/ExecCommand.cpp \
			$(CORE_DIR)/DownloadCommand.cpp \
			$(CORE_DIR)/UploadCommand.cpp \
			$(CORE_DIR)/FileInfoCommand.cpp

# server 源文件
SERVER_SRCS = $(SERVER_DIR)/main.cpp

# client 源文件
CLIENT_SRCS = $(CLIENT_DIR)/main.cpp

# 目标文件
CORE_OBJS   = $(CORE_SRCS:.cpp=.o)
SERVER_OBJS = $(SERVER_SRCS:.cpp=.o)
CLIENT_OBJS = $(CLIENT_SRCS:.cpp=.o)

# 静态库
CORE_LIB = $(CORE_DIR)/$(LIB_PREFIX)winuxlink_core$(LIB_EXT)

# 最终可执行文件
SERVER_TARGET = $(BIN_DIR)/Winuxlink_server$(EXE_EXT)
CLIENT_TARGET = $(BIN_DIR)/Winuxlink_client$(EXE_EXT)

# ---------- 默认目标：编译全部 ----------
.PHONY: all clean run-server run-client

all: $(CORE_LIB) $(SERVER_TARGET) $(CLIENT_TARGET)
	@echo [OK] All targets built.

# ---------- 单独编译目标 ----------
server: $(CORE_LIB) $(SERVER_TARGET)
	@echo [OK] Server built.

client: $(CORE_LIB) $(CLIENT_TARGET)
	@echo [OK] Client built.

# ---------- 编译 core 静态库 ----------
$(CORE_LIB): $(CORE_OBJS)
	@echo [AR] $@
	@$(call MKDIR,$(CORE_DIR))
	ar rcs $@ $^

# ---------- 链接 server ----------
$(SERVER_TARGET): $(CORE_LIB) $(SERVER_OBJS)
	@echo [LD] $@
	@$(call MKDIR,$(BIN_DIR))
	$(CXX) $(CXXFLAGS) -o $@ $(SERVER_OBJS) -L$(CORE_DIR) -lwinuxlink_core $(PLATFORM_LIBS) $(LDFLAGS)

# ---------- 链接 client ----------
$(CLIENT_TARGET): $(CORE_LIB) $(CLIENT_OBJS)
	@echo [LD] $@
	@$(call MKDIR,$(BIN_DIR))
	$(CXX) $(CXXFLAGS) -o $@ $(CLIENT_OBJS) -L$(CORE_DIR) -lwinuxlink_core $(PLATFORM_LIBS) $(LDFLAGS)

# ---------- 通用编译规则 ----------
%.o: %.cpp
	@echo [CXX] $<
	$(CXX) $(CXXFLAGS) -c $< -o $@

# ---------- 清理 ----------
clean:
ifeq ($(PLATFORM),Windows)
	@echo Cleaning...
	-del /Q /F $(subst /,\,$(CORE_DIR))\*.o 2>nul
	-del /Q /F $(subst /,\,$(SERVER_DIR))\*.o 2>nul
	-del /Q /F $(subst /,\,$(CLIENT_DIR))\*.o 2>nul
	-del /Q /F $(subst /,\,$(CORE_LIB)) 2>nul
	-del /Q /F $(subst /,\,$(BIN_DIR))\*.exe 2>nul
else
	@echo Cleaning...
	$(RM) $(CORE_DIR)/*.o
	$(RM) $(SERVER_DIR)/*.o
	$(RM) $(CLIENT_DIR)/*.o
	$(RM) $(CORE_LIB)
	$(RM) $(BIN_DIR)/*
endif

# ---------- 运行（测试用） ----------
run-server: $(SERVER_TARGET)
	@echo Running server...
	$(SERVER_TARGET) 127.0.0.1 8899

run-client: $(CLIENT_TARGET)
	@echo Running client...
	$(CLIENT_TARGET) 127.0.0.1 8899