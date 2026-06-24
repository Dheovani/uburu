#pragma once

#include <chrono>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace uburu
{

  using RepositoryId = std::string;
  using WorktreeId = std::string;

  enum class SearchMode
  {
    literal,
    regex
  };

  enum class SearchTarget
  {
    content,
    file_name,
    content_and_file_name
  };

  enum class SearchResultKind
  {
    content,
    file_name
  };

  enum class GitFileStatus
  {
    clean,
    modified,
    added,
    deleted,
    untracked,
    ignored,
    conflicted
  };

  struct SearchOptions
  {
    SearchMode mode{SearchMode::literal};
    SearchTarget target{SearchTarget::content};
    bool case_sensitive{false};
    bool whole_word{false};
    bool whole_identifier{false};
    bool respect_gitignore{true};
    bool include_hidden{false};
    bool include_binary{false};
    bool follow_symlinks{false};
    std::uintmax_t maximum_file_size{16U * 1024U * 1024U};
    std::size_t result_limit{10'000};
    std::uint32_t regex_match_limit{100'000};
    std::uint32_t regex_depth_limit{1'000};
    std::uint32_t regex_heap_limit_kib{16U * 1024U};
    std::chrono::milliseconds regex_timeout{100};
    std::vector<std::string> extensions;
    std::vector<std::filesystem::path> included_directories;
    std::vector<std::filesystem::path> excluded_directories;
    std::vector<std::string> included_globs;
    std::vector<std::string> excluded_globs;
  };

  struct SearchQuery
  {
    std::filesystem::path root;
    std::string expression;
    SearchOptions options;
  };

  struct SearchResult
  {
    SearchResultKind kind{SearchResultKind::content};
    std::filesystem::path path;
    std::size_t line{0};
    std::size_t column{0};
    std::size_t match_length{0};
    std::string line_text;
  };

  struct FileEntry
  {
    std::filesystem::path absolute_path;
    std::filesystem::path relative_path;
    std::uintmax_t size{0};
    std::filesystem::file_time_type modified_at{};
    bool hidden{false};
    bool binary{false};
    bool symlink{false};
  };

  struct RepositoryInfo
  {
    RepositoryId id;
    std::filesystem::path common_git_directory;
    std::optional<std::string> current_branch;
    std::string head_oid;
    bool detached_head{false};
  };

  struct WorktreeInfo
  {
    WorktreeId id;
    RepositoryId repository_id;
    std::filesystem::path root;
    std::filesystem::path git_directory;
    std::optional<std::string> branch;
    std::string head_oid;
  };

  struct IndexDocument
  {
    RepositoryId repository_id;
    WorktreeId worktree_id;
    std::filesystem::path relative_path;
    std::string content_hash;
    std::optional<std::string> git_blob_hash;
    GitFileStatus status{GitFileStatus::clean};
    std::uintmax_t size{0};
    std::chrono::system_clock::time_point indexed_at{};
    bool deleted{false};
  };

} // namespace uburu
