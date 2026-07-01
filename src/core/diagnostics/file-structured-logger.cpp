#include "core/diagnostics/file-structured-logger.hpp"

#include <algorithm>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <string>
#include <utility>

namespace uburu::diagnostics
{
  namespace
  {

    constexpr std::uintmax_t minimumMaximumFileSizeBytes = 1;

    [[nodiscard]] std::filesystem::path rotatedPath(const std::filesystem::path& path, std::size_t index)
    {
      return std::filesystem::path(path.string() + "." + std::to_string(index));
    }

    [[nodiscard]] std::uintmax_t currentFileSize(const std::filesystem::path& path)
    {
      std::error_code error;
      const auto size = std::filesystem::file_size(path, error);

      if (error)
        return 0;

      return size;
    }

    [[nodiscard]] std::string escapedJsonString(std::string_view value)
    {
      std::string escaped;
      escaped.reserve(value.size());

      for (const auto character : value) {
        switch (character) {
        case '"':
          escaped += "\\\"";
          break;
        case '\\':
          escaped += "\\\\";
          break;
        case '\n':
          escaped += "\\n";
          break;
        case '\r':
          escaped += "\\r";
          break;
        case '\t':
          escaped += "\\t";
          break;
        default:
          escaped += character;
          break;
        }
      }

      return escaped;
    }

    [[nodiscard]] std::int64_t timestampMilliseconds(std::chrono::system_clock::time_point timestamp)
    {
      return std::chrono::duration_cast<std::chrono::milliseconds>(timestamp.time_since_epoch()).count();
    }

    [[nodiscard]] std::string serializeLogEvent(const LogEvent& event)
    {
      std::string line;
      line += "{\"timestamp_ms\":";
      line += std::to_string(timestampMilliseconds(event.timestamp));
      line += ",\"level\":\"";
      line += logLevelName(event.level);
      line += "\",\"category\":\"";
      line += logCategoryName(event.category);
      line += "\",\"message\":\"";
      line += escapedJsonString(event.message);
      line += "\",\"fields\":{";

      for (std::size_t index = 0; index < event.fields.size(); ++index) {
        if (index > 0)
          line += ",";

        const auto& field = event.fields[index];

        line += "\"";
        line += escapedJsonString(field.key);
        line += "\":\"";
        line += escapedJsonString(field.value);
        line += "\"";
      }

      line += "}}\n";

      return line;
    }

  } // namespace

  FileStructuredLogger::FileStructuredLogger(FileStructuredLogOptions options) : options(std::move(options))
  {
    if (this->options.path.empty())
      throw std::invalid_argument("FileStructuredLogger requires a log path");

    this->options.maximumFileSizeBytes = std::max(this->options.maximumFileSizeBytes, minimumMaximumFileSizeBytes);

    const auto parentPath = this->options.path.parent_path();

    if (!parentPath.empty())
      std::filesystem::create_directories(parentPath);
  }

  void FileStructuredLogger::write(LogEvent event)
  {
    if (!shouldRecordLogEvent(event, options.structuredOptions))
      return;

    sanitizeLogEvent(event, options.structuredOptions);
    const auto line = serializeLogEvent(event);

    rotateIfNeeded(line.size());

    std::ofstream stream(options.path, std::ios::app | std::ios::binary);
    if (!stream)
      throw std::runtime_error("Could not open structured log file");

    stream << line;
  }

  void FileStructuredLogger::rotateIfNeeded(std::uintmax_t nextLineSize)
  {
    if (currentFileSize(options.path) + nextLineSize <= options.maximumFileSizeBytes)
      return;

    std::error_code error;

    if (options.maximumRotatedFiles == 0) {
      std::filesystem::remove(options.path, error);

      return;
    }

    for (std::size_t index = options.maximumRotatedFiles; index > 1; --index) {
      const auto source = rotatedPath(options.path, index - 1);
      const auto destination = rotatedPath(options.path, index);

      std::filesystem::remove(destination, error);
      error.clear();

      if (std::filesystem::exists(source))
        std::filesystem::rename(source, destination, error);

      error.clear();
    }

    const auto firstRotatedPath = rotatedPath(options.path, 1);

    std::filesystem::remove(firstRotatedPath, error);
    error.clear();

    if (std::filesystem::exists(options.path))
      std::filesystem::rename(options.path, firstRotatedPath, error);
  }

} // namespace uburu::diagnostics
