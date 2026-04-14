// core/ListCommand.h
#pragma once

#include "ICommand.h"
#include "Agreement.h"
#include <vector>
#include <string>
#include <cstdint>

namespace winuxlink {

struct FileInfo {
    std::string name;
    uint64_t size;
    bool isDir;
    uint64_t mtime;     // Unix 时间戳（秒）
};

class ListCommand : public ICommand {
public:
    explicit ListCommand(const std::string& path = ".");
    ~ListCommand() override = default;

    // ICommand 接口实现
    Agreement::Command getCommandCode() const override;
    std::vector<uint8_t> buildRequestPayload() const override;
    bool parseResponse(const std::vector<uint8_t>& payload) override;
    std::vector<uint8_t> executeServer(const std::vector<uint8_t>& requestPayload) override;
    std::string getDescription() const override { return "List directory contents"; }

    // 获取解析后的文件列表（客户端调用）
    const std::vector<FileInfo>& getFiles() const { return m_files; }

private:
    std::string m_path;
    std::vector<FileInfo> m_files;

    // 跨平台目录遍历辅助
    std::vector<FileInfo> listDirectory(const std::string& path);
};

} // namespace winuxlink