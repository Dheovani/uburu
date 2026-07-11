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

  /** Version constants for persisted index documents. */

  inline constexpr std::uint32_t initialIndexDocumentFormatVersion = 1;
  inline constexpr std::uint32_t latestIndexDocumentFormatVersion = initialIndexDocumentFormatVersion;

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

  /**
   * Search behavior and safety limits shared by direct and indexed search paths.
   */
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
    bool includeSubdirectories{true};
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

  /**
   * One root selected by the user plus optional relative include/exclude folders.
   */
  struct SearchRoot
  {
    std::filesystem::path path;
    std::vector<std::filesystem::path> includedDirectories;
    std::vector<std::filesystem::path> excludedDirectories;
  };

  /**
   * Multi-root search scope used by the core independently from UI widgets.
   */
  struct SearchScope
  {
    std::vector<SearchRoot> roots;
  };

  /**
   * Complete search request consumed by search engines.
   */
  struct SearchQuery
  {
    std::filesystem::path root;
    SearchScope scope;
    std::string expression;
    SearchOptions options;
  };

  /**
   * Byte-precise highlight span with a display column for UI rendering.
   */
  struct MatchSpan
  {
    std::size_t column{0};
    std::size_t byteOffset{0};
    std::size_t byteLength{0};
  };

  /**
   * One search hit, including preview context and the root that produced it.
   */
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

  /**
   * Filesystem metadata collected before a file is read or indexed.
   */
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

  /**
   * Logical Git repository metadata shared by all worktrees.
   */
  struct RepositoryInfo
  {
    RepositoryId id;
    std::filesystem::path commonGitDirectory;
    std::optional<std::filesystem::path> worktreeRoot;
    std::optional<std::string> currentBranch;
    std::string headOid;
    bool detachedHead{false};
  };

  /**
   * Physical worktree state currently visible to the user.
   */
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

  /**
   * Hash of a Git object with its repository hash algorithm.
   */
  struct GitObjectId
  {
    GitObjectHashAlgorithm algorithm{GitObjectHashAlgorithm::unknown};
    std::string value;
  };

  /**
   * Git working-tree overlay entry used to reconcile indexed state with local changes.
   */
  struct GitOverlayEntry
  {
    std::filesystem::path relativePath;
    std::optional<std::filesystem::path> previousRelativePath;
    GitFileStatus status{GitFileStatus::clean};
    GitOverlayDisposition disposition{GitOverlayDisposition::useIndexedContent};
    std::optional<GitObjectId> reusableBlob;
  };

  /**
   * Nested Git boundary discovered during scanning.
   */
  struct GitRepositoryBoundary
  {
    std::filesystem::path relativePath;
    GitRepositoryBoundaryKind kind{GitRepositoryBoundaryKind::none};
  };

  /**
   * Persisted representation of one indexable document version.
   */
  struct IndexDocument
  {
    std::uint32_t formatVersion{latestIndexDocumentFormatVersion};
    RepositoryId repositoryId;
    WorktreeId worktreeId;
    std::filesystem::path relativePath;
    std::string contentHash;
    ContentHashAlgorithm contentHashAlgorithm{ContentHashAlgorithm::unknown};
    std::optional<std::string> gitBlobHash;
    GitObjectHashAlgorithm gitBlobHashAlgorithm{GitObjectHashAlgorithm::unknown};
    GitFileStatus status{GitFileStatus::clean};
    std::uintmax_t size{0};
    std::filesystem::file_time_type modifiedAt{};
    std::chrono::system_clock::time_point indexedAt{};
    bool deleted{false};
    std::optional<std::string> indexedText;
  };

  /**
   * Reusable document identity used to avoid reindexing unchanged content.
   */
  struct IndexedDocumentIdentity
  {
    std::uint32_t formatVersion{latestIndexDocumentFormatVersion};
    std::string contentHash;
    ContentHashAlgorithm contentHashAlgorithm{ContentHashAlgorithm::unknown};
    std::optional<std::string> gitBlobHash;
    GitObjectHashAlgorithm gitBlobHashAlgorithm{GitObjectHashAlgorithm::unknown};
    std::uintmax_t size{0};
    std::chrono::system_clock::time_point indexedAt{};
  };

  /**
   * Full index generation for a worktree and HEAD snapshot.
   */
  struct IndexGeneration
  {
    RepositoryId repositoryId;
    WorktreeId worktreeId;
    std::string headOid;
    std::optional<std::string> branch;
    std::chrono::system_clock::time_point createdAt{};
    std::vector<IndexDocument> documents;
  };

  /**
   * Lightweight generation metadata without document payloads.
   */
  struct IndexGenerationMetadata
  {
    RepositoryId repositoryId;
    WorktreeId worktreeId;
    std::string headOid;
    std::optional<std::string> branch;
    std::chrono::system_clock::time_point createdAt{};
  };

  /**
   * SQLite PRAGMA state captured for storage diagnostics.
   */
  struct StoragePragmaSnapshot
  {
    bool foreignKeysEnabled{false};
    std::string journalMode;
    std::string synchronousMode;
    std::chrono::milliseconds busyTimeout{0};
  };

  /**
   * Result of a storage integrity check.
   */
  struct StorageIntegrityReport
  {
    bool ok{false};
    std::string message;
  };

  /**
   * User search history entry persisted by storage.
   */
  struct SearchHistoryEntry
  {
    std::filesystem::path root;
    std::string expression;
    std::chrono::system_clock::time_point searchedAt{};
  };

  /**
   * Named search saved by the user.
   */
  struct SavedSearch
  {
    std::string name;
    std::filesystem::path root;
    std::string expression;
    std::chrono::system_clock::time_point savedAt{};
  };

  /**
   * Numeric indexing metric persisted for diagnostics and trend analysis.
   */
  struct IndexingMetric
  {
    std::string name;
    std::int64_t value{0};
    std::chrono::system_clock::time_point recordedAt{};
  };

  /**
   * Result of moving or copying database files to the current application data location.
   */
  struct StorageMigrationResult
  {
    bool copiedDatabase{false};
    bool copiedWriteAheadLog{false};
    bool copiedSharedMemory{false};
  };

} // namespace uburu
