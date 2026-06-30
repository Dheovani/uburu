#include "core/index/content-hash.hpp"

#include <array>
#include <bit>
#include <cstdint>
#include <fstream>
#include <iomanip>
#include <limits>
#include <sstream>
#include <stdexcept>

namespace uburu::index
{
  namespace
  {

    constexpr std::size_t sha256DigestWordCount = 8;
    constexpr std::size_t sha256BlockByteCount = 64;
    constexpr std::size_t sha256MessageScheduleWordCount = 64;
    constexpr std::size_t sha256InitialMessageWordCount = 16;
    constexpr std::size_t sha256LengthFieldByteCount = 8;
    constexpr std::size_t sha256WordByteCount = 4;
    constexpr std::size_t hashFileBufferByteCount = 64U * 1024U;
    constexpr unsigned int bitsPerByte = 8;
    constexpr unsigned int byteHexWidth = 2;
    constexpr std::byte sha256PaddingStartByte{0x80U};
    constexpr std::uint32_t byteMask = 0xFFU;

    constexpr std::array<std::uint32_t, sha256DigestWordCount> sha256InitialState{
      0x6a09e667U,
      0xbb67ae85U,
      0x3c6ef372U,
      0xa54ff53aU,
      0x510e527fU,
      0x9b05688cU,
      0x1f83d9abU,
      0x5be0cd19U,
    };

    constexpr std::array<std::uint32_t, sha256MessageScheduleWordCount> sha256RoundConstants{
      0x428a2f98U, 0x71374491U, 0xb5c0fbcfU, 0xe9b5dba5U, 0x3956c25bU, 0x59f111f1U, 0x923f82a4U, 0xab1c5ed5U,
      0xd807aa98U, 0x12835b01U, 0x243185beU, 0x550c7dc3U, 0x72be5d74U, 0x80deb1feU, 0x9bdc06a7U, 0xc19bf174U,
      0xe49b69c1U, 0xefbe4786U, 0x0fc19dc6U, 0x240ca1ccU, 0x2de92c6fU, 0x4a7484aaU, 0x5cb0a9dcU, 0x76f988daU,
      0x983e5152U, 0xa831c66dU, 0xb00327c8U, 0xbf597fc7U, 0xc6e00bf3U, 0xd5a79147U, 0x06ca6351U, 0x14292967U,
      0x27b70a85U, 0x2e1b2138U, 0x4d2c6dfcU, 0x53380d13U, 0x650a7354U, 0x766a0abbU, 0x81c2c92eU, 0x92722c85U,
      0xa2bfe8a1U, 0xa81a664bU, 0xc24b8b70U, 0xc76c51a3U, 0xd192e819U, 0xd6990624U, 0xf40e3585U, 0x106aa070U,
      0x19a4c116U, 0x1e376c08U, 0x2748774cU, 0x34b0bcb5U, 0x391c0cb3U, 0x4ed8aa4aU, 0x5b9cca4fU, 0x682e6ff3U,
      0x748f82eeU, 0x78a5636fU, 0x84c87814U, 0x8cc70208U, 0x90befffaU, 0xa4506cebU, 0xbef9a3f7U, 0xc67178f2U,
    };

    [[nodiscard]] std::uint32_t choose(std::uint32_t x, std::uint32_t y, std::uint32_t z)
    {
      return (x & y) ^ (~x & z);
    }

    [[nodiscard]] std::uint32_t majority(std::uint32_t x, std::uint32_t y, std::uint32_t z)
    {
      return (x & y) ^ (x & z) ^ (y & z);
    }

    [[nodiscard]] std::uint32_t bigSigma0(std::uint32_t value)
    {
      return std::rotr(value, 2) ^ std::rotr(value, 13) ^ std::rotr(value, 22);
    }

    [[nodiscard]] std::uint32_t bigSigma1(std::uint32_t value)
    {
      return std::rotr(value, 6) ^ std::rotr(value, 11) ^ std::rotr(value, 25);
    }

    [[nodiscard]] std::uint32_t smallSigma0(std::uint32_t value)
    {
      return std::rotr(value, 7) ^ std::rotr(value, 18) ^ (value >> 3);
    }

    [[nodiscard]] std::uint32_t smallSigma1(std::uint32_t value)
    {
      return std::rotr(value, 17) ^ std::rotr(value, 19) ^ (value >> 10);
    }

    [[nodiscard]] std::uint32_t readBigEndianWord(std::span<const std::byte> bytes)
    {
      std::uint32_t word = 0;

      for (const auto byte : bytes) {
        word = (word << bitsPerByte) | static_cast<std::uint32_t>(byte);
      }

      return word;
    }

    [[nodiscard]] std::string digestToHex(const std::array<std::uint32_t, sha256DigestWordCount>& state)
    {
      std::ostringstream output;
      output << std::hex << std::setfill('0');

      for (const auto word : state) {
        for (auto byteIndex = sha256WordByteCount; byteIndex > 0; --byteIndex) {
          const auto shift = static_cast<unsigned int>((byteIndex - 1U) * bitsPerByte);
          const auto byte = static_cast<unsigned int>((word >> shift) & byteMask);
          output << std::setw(byteHexWidth) << byte;
        }
      }

      return output.str();
    }

    class Sha256
    {
    public:
      void update(std::span<const std::byte> bytes)
      {
        addByteCount(bytes.size());

        for (const auto byte : bytes) {
          buffer[bufferSize] = byte;
          ++bufferSize;

          if (bufferSize == sha256BlockByteCount) {
            transform(buffer);
            bufferSize = 0;
          }
        }
      }

      [[nodiscard]] std::string finish()
      {
        const auto totalBitCount = byteCount * bitsPerByte;

        buffer[bufferSize] = sha256PaddingStartByte;
        ++bufferSize;

        if (bufferSize > sha256BlockByteCount - sha256LengthFieldByteCount) {
          while (bufferSize < sha256BlockByteCount) {
            buffer[bufferSize] = std::byte{0};
            ++bufferSize;
          }

          transform(buffer);
          bufferSize = 0;
        }

        while (bufferSize < sha256BlockByteCount - sha256LengthFieldByteCount) {
          buffer[bufferSize] = std::byte{0};
          ++bufferSize;
        }

        writeLength(totalBitCount);
        transform(buffer);

        return digestToHex(state);
      }

    private:
      void addByteCount(std::size_t byteCountToAdd)
      {
        const auto maximumBytesToAdd = (std::numeric_limits<std::uint64_t>::max() / bitsPerByte) - byteCount;

        if (byteCountToAdd > maximumBytesToAdd)
          throw std::overflow_error("content is too large to hash with SHA-256 length encoding");

        byteCount += static_cast<std::uint64_t>(byteCountToAdd);
      }

      void writeLength(std::uint64_t totalBitCount)
      {
        for (std::size_t index = 0; index < sha256LengthFieldByteCount; ++index) {
          const auto shift = static_cast<unsigned int>((sha256LengthFieldByteCount - index - 1U) * bitsPerByte);
          buffer[sha256BlockByteCount - sha256LengthFieldByteCount + index] =
            static_cast<std::byte>((totalBitCount >> shift) & byteMask);
        }
      }

      void transform(std::span<const std::byte, sha256BlockByteCount> block)
      {
        std::array<std::uint32_t, sha256MessageScheduleWordCount> schedule{};

        for (std::size_t index = 0; index < sha256InitialMessageWordCount; ++index) {
          const auto offset = index * sha256WordByteCount;
          schedule[index] = readBigEndianWord(block.subspan(offset, sha256WordByteCount));
        }

        for (std::size_t index = sha256InitialMessageWordCount; index < sha256MessageScheduleWordCount; ++index) {
          schedule[index] = smallSigma1(schedule[index - 2U]) + schedule[index - 7U] +
                            smallSigma0(schedule[index - 15U]) + schedule[index - 16U];
        }

        auto a = state[0];
        auto b = state[1];
        auto c = state[2];
        auto d = state[3];
        auto e = state[4];
        auto f = state[5];
        auto g = state[6];
        auto h = state[7];

        for (std::size_t index = 0; index < sha256MessageScheduleWordCount; ++index) {
          const auto temporary1 = h + bigSigma1(e) + choose(e, f, g) + sha256RoundConstants[index] + schedule[index];
          const auto temporary2 = bigSigma0(a) + majority(a, b, c);

          h = g;
          g = f;
          f = e;
          e = d + temporary1;
          d = c;
          c = b;
          b = a;
          a = temporary1 + temporary2;
        }

        state[0] += a;
        state[1] += b;
        state[2] += c;
        state[3] += d;
        state[4] += e;
        state[5] += f;
        state[6] += g;
        state[7] += h;
      }

      std::array<std::uint32_t, sha256DigestWordCount> state{sha256InitialState};
      std::array<std::byte, sha256BlockByteCount> buffer{};
      std::size_t bufferSize{0};
      std::uint64_t byteCount{0};
    };

    [[nodiscard]] std::span<const std::byte> bytesRead(std::span<const char> buffer, std::streamsize count)
    {
      return std::as_bytes(buffer.first(static_cast<std::size_t>(count)));
    }

  } // namespace

  ContentHash computeContentHash(std::span<const std::byte> bytes)
  {
    Sha256 sha256;
    sha256.update(bytes);

    return ContentHash{.algorithm = ContentHashAlgorithm::sha256, .value = sha256.finish()};
  }

  std::optional<ContentHash> computeFileContentHash(const std::filesystem::path& path, std::stop_token stopToken)
  {
    std::ifstream file(path, std::ios::binary);

    if (!file)
      throw std::runtime_error("failed to open file for content hashing: " + path.string());

    Sha256 sha256;
    std::array<char, hashFileBufferByteCount> buffer{};

    while (file) {
      if (stopToken.stop_requested())
        return std::nullopt;

      file.read(buffer.data(), static_cast<std::streamsize>(buffer.size()));
      const auto count = file.gcount();

      if (count > 0)
        sha256.update(bytesRead(buffer, count));
    }

    if (!file.eof())
      throw std::runtime_error("failed to read file for content hashing: " + path.string());

    return ContentHash{.algorithm = ContentHashAlgorithm::sha256, .value = sha256.finish()};
  }

} // namespace uburu::index
