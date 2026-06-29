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

  inline constexpr std::uint32_t currentIndexDocumentFormatVersion = 1;

  enum class SearchMode
  {
    literal,
    regex
  };

  enum class SearchTarget
  {
    content,
    fileName,
    contentAndFileName
  };

  enum class SearchResultKind
  {
    content,
    fileName
  };

  enum class TextEncoding
  {
    utf8,
    utf16Le,
    utf16Be,
    latin1
  };

  enum class InvalidUtf8Policy
  {
    replace,
    skip,
    fail
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

  enum class GitObjectHashAlgorithm
  {
    unknown,
    sha1,
    sha256
  };

  enum class ContentHashAlgorithm
  {
    unknown,
    sha256
  };

  enum class GitRepositoryBoundaryKind
  {
    none,
    submodule,
    nestedRepository
  };

  enum class GitOverlayDisposition
  {
    useIndexedContent,
    replaceWithWorkingTree,
    addWorkingTreeFile,
    hideIndexedContent,
    conflict
  };

  struct SearchOptions
  {
    SearchMode mode{SearchMode::literal};
    SearchTarget target{SearchTarget::content};
    bool caseSensitive{false};
    bool wholeWord{false};
    bool wholeIdentifier{false};
    bool respectGitignore{true};
    bool includeHidden{false};
    bool includeBinary{false};
    bool followSymlinks{false};
    std::uintmax_t maximumFileSize{16U * 1024U * 1024U};
    std::size_t resultLimit{10'000};
    std::size_t perFileResultLimit{1'000};
    std::uint32_t regexMatchLimit{100'000};
    std::uint32_t regexDepthLimit{1'000};
    std::uint32_t regexHeapLimitKib{16U * 1024U};
    std::chrono::milliseconds regexTimeout{100};
    std::size_t binarySampleSize{8192};
    std::size_t maximumLineLength{1024U * 1024U};
    TextEncoding fallbackEncoding{TextEncoding::latin1};
    InvalidUtf8Policy invalidUtf8Policy{InvalidUtf8Policy::replace};
    std::size_t contextBeforeLines{0};
    std::size_t contextAfterLines{0};
    std::vector<std::string> extensions;
    std::vector<std::filesystem::path> includedDirectories;
    std::vector<std::filesystem::path> excludedDirectories;
    std::vector<std::string> includedGlobs;
    std::vector<std::string> excludedGlobs;
    std::vector<std::filesystem::path> globalGitIgnoreFiles;
  };

  struct SearchRoot
  {
    std::filesystem::path path;
    std::vector<std::filesystem::path> includedDirectories;
    std::vector<std::filesystem::path> excludedDirectories;
  };

  struct SearchScope
  {
    std::vector<SearchRoot> roots;
  };

  struct SearchQuery
  {
    std::filesystem::path root;
    SearchScope scope;
    std::string expression;
    SearchOptions options;
  };

  struct MatchSpan
  {
    std::size_t column{0};
    std::size_t byteOffset{0};
    std::size_t byteLength{0};
  };

  struct SearchResult
  {
    SearchResultKind kind{SearchResultKind::content};
    std::filesystem::path path;
    std::size_t line{0};
    std::size_t column{0};
    std::size_t matchLength{0};
    std::string lineText;
    std::vector<MatchSpan> highlights;
    std::vector<std::string> contextBefore;
    std::vector<std::string> contextAfter;
    std::filesystem::path searchRoot;
  };

  struct FileEntry
  {
    std::filesystem::path absolutePath;
    std::filesystem::path relativePath;
    std::uintmax_t size{0};
    std::filesystem::file_time_type modifiedAt{};
    bool hidden{false};
    bool binary{false};
    bool symlink{false};
    bool sparse{false};
    std::filesystem::path searchRoot;
  };

  struct RepositoryInfo
  {
    RepositoryId id;
    std::filesystem::path commonGitDirectory;
    std::optional<std::filesystem::path> worktreeRoot;
    std::optional<std::string> currentBranch;
    std::string headOid;
    bool detachedHead{false};
  };

  struct WorktreeInfo
  {
    WorktreeId id;
    RepositoryId repositoryId;
    std::filesystem::path root;
    std::filesystem::path gitDirectory;
    std::optional<std::string> branch;
    std::string headOid;
    bool locked{false};
    bool prunable{false};
    std::string lockReason;
  };

  struct GitObjectId
  {
    GitObjectHashAlgorithm algorithm{GitObjectHashAlgorithm::unknown};
    std::string value;
  };

  struct GitOverlayEntry
  {
    std::filesystem::path relativePath;
    std::optional<std::filesystem::path> previousRelativePath;
    GitFileStatus status{GitFileStatus::clean};
    GitOverlayDisposition disposition{GitOverlayDisposition::useIndexedContent};
    std::optional<GitObjectId> reusableBlob;
  };

  struct GitRepositoryBoundary
  {
    std::filesystem::path relativePath;
    GitRepositoryBoundaryKind kind{GitRepositoryBoundaryKind::none};
  };

  struct IndexDocument
  {
    std::uint32_t formatVersion{currentIndexDocumentFormatVersion};
    RepositoryId repositoryId;
    WorktreeId worktreeId;
    std::filesystem::path relativePath;
    std::string contentHash;
    ContentHashAlgorithm contentHashAlgorithm{ContentHashAlgorithm::unknown};
    std::optional<std::string> gitBlobHash;
    GitObjectHashAlgorithm gitBlobHashAlgorithm{GitObjectHashAlgorithm::unknown};
    GitFileStatus status{GitFileStatus::clean};
    std::uintmax_t size{0};
    std::chrono::system_clock::time_point indexedAt{};
    bool deleted{false};
  };

  struct IndexGeneration
  {
    RepositoryId repositoryId;
    WorktreeId worktreeId;
    std::string headOid;
    std::optional<std::string> branch;
    std::chrono::system_clock::time_point createdAt{};
    std::vector<IndexDocument> documents;
  };

  struct StoragePragmaSnapshot
  {
    bool foreignKeysEnabled{false};
    std::string journalMode;
    std::string synchronousMode;
    std::chrono::milliseconds busyTimeout{0};
  };

  struct StorageIntegrityReport
  {
    bool ok{false};
    std::string message;
  };

  struct SearchHistoryEntry
  {
    std::filesystem::path root;
    std::string expression;
    std::chrono::system_clock::time_point searchedAt{};
  };

  struct SavedSearch
  {
    std::string name;
    std::filesystem::path root;
    std::string expression;
    std::chrono::system_clock::time_point savedAt{};
  };

  struct IndexingMetric
  {
    std::string name;
    std::int64_t value{0};
    std::chrono::system_clock::time_point recordedAt{};
  };

  struct StorageMigrationResult
  {
    bool copiedDatabase{false};
    bool copiedWriteAheadLog{false};
    bool copiedSharedMemory{false};
  };

} // namespace uburu
