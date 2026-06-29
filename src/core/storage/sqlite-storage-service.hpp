#pragma once

#include "core/storage/storage-service.hpp"

#include <filesystem>

namespace uburu::storage
{

  class SQLiteStorageService final : public StorageService
  {
  public:
    explicit SQLiteStorageService(std::filesystem::path databasePath);
    ~SQLiteStorageService() override;

    SQLiteStorageService(const SQLiteStorageService&) = delete;
    SQLiteStorageService& operator=(const SQLiteStorageService&) = delete;
    SQLiteStorageService(SQLiteStorageService&&) noexcept = delete;
    SQLiteStorageService& operator=(SQLiteStorageService&&) noexcept = delete;

    void initialize() override;
    void upsertRepository(const RepositoryInfo& repository) override;
    void upsertWorktree(const WorktreeInfo& worktree) override;
    void upsertDocument(const IndexDocument& document) override;
    void publishGeneration(const IndexGeneration& generation) override;
    [[nodiscard]] std::size_t recoverIncompleteGenerations() override;
    [[nodiscard]] std::size_t collectOrphanDocuments() override;
    void removeDocument(const WorktreeId& worktreeId, const std::filesystem::path& relativePath) override;
    [[nodiscard]] std::optional<IndexDocument>
    findDocument(const WorktreeId& worktreeId, const std::filesystem::path& relativePath) const override;

  private:
    std::filesystem::path databasePath;
    void* databaseHandle{nullptr};
  };

} // namespace uburu::storage
