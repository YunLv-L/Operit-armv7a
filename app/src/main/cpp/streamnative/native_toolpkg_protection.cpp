#include <jni.h>
#include <fcntl.h>
#include <unistd.h>

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstring>
#include <limits>
#include <random>
#include <string>
#include <vector>

#ifndef TOOLPKG_PROTECTION_SECRET
#define TOOLPKG_PROTECTION_SECRET ""
#endif

namespace {

constexpr std::array<uint8_t, 8> kMagic = {'O', 'P', 'T', 'P', 'R', 'O', 'T', '1'};
constexpr size_t kNonceSize = 16;
constexpr size_t kTagSize = 32;
constexpr size_t kHeaderSize = kMagic.size() + kNonceSize + kTagSize;

constexpr std::array<uint32_t, 64> kSha256K = {
        0x428a2f98U, 0x71374491U, 0xb5c0fbcfU, 0xe9b5dba5U, 0x3956c25bU, 0x59f111f1U, 0x923f82a4U, 0xab1c5ed5U,
        0xd807aa98U, 0x12835b01U, 0x243185beU, 0x550c7dc3U, 0x72be5d74U, 0x80deb1feU, 0x9bdc06a7U, 0xc19bf174U,
        0xe49b69c1U, 0xefbe4786U, 0x0fc19dc6U, 0x240ca1ccU, 0x2de92c6fU, 0x4a7484aaU, 0x5cb0a9dcU, 0x76f988daU,
        0x983e5152U, 0xa831c66dU, 0xb00327c8U, 0xbf597fc7U, 0xc6e00bf3U, 0xd5a79147U, 0x06ca6351U, 0x14292967U,
        0x27b70a85U, 0x2e1b2138U, 0x4d2c6dfcU, 0x53380d13U, 0x650a7354U, 0x766a0abbU, 0x81c2c92eU, 0x92722c85U,
        0xa2bfe8a1U, 0xa81a664bU, 0xc24b8b70U, 0xc76c51a3U, 0xd192e819U, 0xd6990624U, 0xf40e3585U, 0x106aa070U,
        0x19a4c116U, 0x1e376c08U, 0x2748774cU, 0x34b0bcb5U, 0x391c0cb3U, 0x4ed8aa4aU, 0x5b9cca4fU, 0x682e6ff3U,
        0x748f82eeU, 0x78a5636fU, 0x84c87814U, 0x8cc70208U, 0x90befffaU, 0xa4506cebU, 0xbef9a3f7U, 0xc67178f2U};

uint32_t rotr(uint32_t value, uint32_t bits) {
    return (value >> bits) | (value << (32U - bits));
}

std::array<uint8_t, 32> sha256(const uint8_t* data, size_t length) {
    uint32_t h[8] = {
            0x6a09e667U, 0xbb67ae85U, 0x3c6ef372U, 0xa54ff53aU,
            0x510e527fU, 0x9b05688cU, 0x1f83d9abU, 0x5be0cd19U};

    std::vector<uint8_t> padded(data, data + length);
    padded.push_back(0x80U);
    while ((padded.size() % 64U) != 56U) {
        padded.push_back(0U);
    }
    const uint64_t bit_length = static_cast<uint64_t>(length) * 8U;
    for (int shift = 56; shift >= 0; shift -= 8) {
        padded.push_back(static_cast<uint8_t>((bit_length >> shift) & 0xffU));
    }

    for (size_t offset = 0; offset < padded.size(); offset += 64) {
        uint32_t w[64] = {};
        for (int index = 0; index < 16; ++index) {
            const size_t base = offset + static_cast<size_t>(index) * 4U;
            w[index] =
                    (static_cast<uint32_t>(padded[base]) << 24U) |
                    (static_cast<uint32_t>(padded[base + 1]) << 16U) |
                    (static_cast<uint32_t>(padded[base + 2]) << 8U) |
                    static_cast<uint32_t>(padded[base + 3]);
        }
        for (int index = 16; index < 64; ++index) {
            const uint32_t s0 = rotr(w[index - 15], 7U) ^ rotr(w[index - 15], 18U) ^ (w[index - 15] >> 3U);
            const uint32_t s1 = rotr(w[index - 2], 17U) ^ rotr(w[index - 2], 19U) ^ (w[index - 2] >> 10U);
            w[index] = w[index - 16] + s0 + w[index - 7] + s1;
        }

        uint32_t a = h[0];
        uint32_t b = h[1];
        uint32_t c = h[2];
        uint32_t d = h[3];
        uint32_t e = h[4];
        uint32_t f = h[5];
        uint32_t g = h[6];
        uint32_t hh = h[7];

        for (int index = 0; index < 64; ++index) {
            const uint32_t s1 = rotr(e, 6U) ^ rotr(e, 11U) ^ rotr(e, 25U);
            const uint32_t ch = (e & f) ^ ((~e) & g);
            const uint32_t temp1 = hh + s1 + ch + kSha256K[index] + w[index];
            const uint32_t s0 = rotr(a, 2U) ^ rotr(a, 13U) ^ rotr(a, 22U);
            const uint32_t maj = (a & b) ^ (a & c) ^ (b & c);
            const uint32_t temp2 = s0 + maj;
            hh = g;
            g = f;
            f = e;
            e = d + temp1;
            d = c;
            c = b;
            b = a;
            a = temp1 + temp2;
        }

        h[0] += a;
        h[1] += b;
        h[2] += c;
        h[3] += d;
        h[4] += e;
        h[5] += f;
        h[6] += g;
        h[7] += hh;
    }

    std::array<uint8_t, 32> out = {};
    for (int index = 0; index < 8; ++index) {
        out[static_cast<size_t>(index) * 4U] = static_cast<uint8_t>((h[index] >> 24U) & 0xffU);
        out[static_cast<size_t>(index) * 4U + 1U] = static_cast<uint8_t>((h[index] >> 16U) & 0xffU);
        out[static_cast<size_t>(index) * 4U + 2U] = static_cast<uint8_t>((h[index] >> 8U) & 0xffU);
        out[static_cast<size_t>(index) * 4U + 3U] = static_cast<uint8_t>(h[index] & 0xffU);
    }
    return out;
}

std::array<uint8_t, 32> sha256(const std::vector<uint8_t>& data) {
    return sha256(data.data(), data.size());
}

std::array<uint8_t, 32> hmacSha256(const std::vector<uint8_t>& key, const std::vector<uint8_t>& message) {
    std::array<uint8_t, 64> block_key = {};
    if (key.size() > block_key.size()) {
        const auto hashed = sha256(key);
        std::copy(hashed.begin(), hashed.end(), block_key.begin());
    } else {
        std::copy(key.begin(), key.end(), block_key.begin());
    }

    std::vector<uint8_t> inner;
    std::vector<uint8_t> outer;
    inner.reserve(block_key.size() + message.size());
    outer.reserve(block_key.size() + 32U);
    for (uint8_t value : block_key) {
        inner.push_back(static_cast<uint8_t>(value ^ 0x36U));
        outer.push_back(static_cast<uint8_t>(value ^ 0x5cU));
    }
    inner.insert(inner.end(), message.begin(), message.end());
    const auto inner_hash = sha256(inner);
    outer.insert(outer.end(), inner_hash.begin(), inner_hash.end());
    return sha256(outer);
}

std::vector<uint8_t> protectionKey() {
    const std::string secret = TOOLPKG_PROTECTION_SECRET;
    return std::vector<uint8_t>(secret.begin(), secret.end());
}

bool secretConfigured() {
    return !protectionKey().empty();
}

bool fillRandom(uint8_t* data, size_t size) {
    size_t offset = 0;
    const int fd = open("/dev/urandom", O_RDONLY | O_CLOEXEC);
    if (fd >= 0) {
        while (offset < size) {
            const ssize_t count = read(fd, data + offset, size - offset);
            if (count <= 0) break;
            offset += static_cast<size_t>(count);
        }
        close(fd);
    }
    if (offset == size) return true;

    std::random_device rd;
    for (; offset < size; ++offset) {
        data[offset] = static_cast<uint8_t>(rd() & 0xffU);
    }
    return true;
}

std::vector<uint8_t> buildTagMessage(
        const std::array<uint8_t, kNonceSize>& nonce,
        const std::vector<uint8_t>& ciphertext) {
    std::vector<uint8_t> message;
    message.reserve(kMagic.size() + nonce.size() + ciphertext.size());
    message.insert(message.end(), kMagic.begin(), kMagic.end());
    message.insert(message.end(), nonce.begin(), nonce.end());
    message.insert(message.end(), ciphertext.begin(), ciphertext.end());
    return message;
}

std::array<uint8_t, 32> keystreamBlock(
        const std::vector<uint8_t>& key,
        const std::array<uint8_t, kNonceSize>& nonce,
        uint64_t counter) {
    std::vector<uint8_t> message;
    const char domain[] = "operit-toolpkg-stream-v1";
    message.insert(message.end(), domain, domain + sizeof(domain) - 1U);
    message.insert(message.end(), nonce.begin(), nonce.end());
    for (int shift = 56; shift >= 0; shift -= 8) {
        message.push_back(static_cast<uint8_t>((counter >> shift) & 0xffU));
    }
    return hmacSha256(key, message);
}

std::vector<uint8_t> xorWithKeystream(
        const std::vector<uint8_t>& input,
        const std::vector<uint8_t>& key,
        const std::array<uint8_t, kNonceSize>& nonce) {
    std::vector<uint8_t> output(input.size());
    uint64_t counter = 0;
    size_t offset = 0;
    while (offset < input.size()) {
        const auto block = keystreamBlock(key, nonce, counter++);
        const size_t block_count = std::min(block.size(), input.size() - offset);
        for (size_t index = 0; index < block_count; ++index) {
            output[offset + index] = static_cast<uint8_t>(input[offset + index] ^ block[index]);
        }
        offset += block_count;
    }
    return output;
}

bool constantTimeEquals(const uint8_t* left, const uint8_t* right, size_t size) {
    uint8_t diff = 0;
    for (size_t index = 0; index < size; ++index) {
        diff = static_cast<uint8_t>(diff | (left[index] ^ right[index]));
    }
    return diff == 0;
}

std::vector<uint8_t> jbyteArrayToVector(JNIEnv* env, jbyteArray bytes) {
    if (bytes == nullptr) return {};
    const jsize length = env->GetArrayLength(bytes);
    std::vector<uint8_t> out(static_cast<size_t>(length));
    if (length > 0) {
        env->GetByteArrayRegion(bytes, 0, length, reinterpret_cast<jbyte*>(out.data()));
    }
    return out;
}

jbyteArray vectorToJbyteArray(JNIEnv* env, const std::vector<uint8_t>& bytes) {
    if (bytes.size() > static_cast<size_t>(std::numeric_limits<jsize>::max())) {
        jclass exception_class = env->FindClass("java/lang/IllegalArgumentException");
        env->ThrowNew(exception_class, "Protected payload is too large");
        return nullptr;
    }
    auto result = env->NewByteArray(static_cast<jsize>(bytes.size()));
    if (result != nullptr && !bytes.empty()) {
        env->SetByteArrayRegion(result, 0, static_cast<jsize>(bytes.size()), reinterpret_cast<const jbyte*>(bytes.data()));
    }
    return result;
}

void throwJava(JNIEnv* env, const char* class_name, const char* message) {
    jclass exception_class = env->FindClass(class_name);
    if (exception_class != nullptr) {
        env->ThrowNew(exception_class, message);
    }
}

std::vector<uint8_t> encryptBytes(JNIEnv* env, const std::vector<uint8_t>& plaintext) {
    const auto key = protectionKey();
    if (key.empty()) {
        throwJava(env, "java/lang/IllegalStateException", "ToolPkg protection secret is not configured");
        return {};
    }
    std::array<uint8_t, kNonceSize> nonce = {};
    fillRandom(nonce.data(), nonce.size());
    const auto ciphertext = xorWithKeystream(plaintext, key, nonce);
    const auto tag = hmacSha256(key, buildTagMessage(nonce, ciphertext));

    std::vector<uint8_t> output;
    output.reserve(kHeaderSize + ciphertext.size());
    output.insert(output.end(), kMagic.begin(), kMagic.end());
    output.insert(output.end(), nonce.begin(), nonce.end());
    output.insert(output.end(), tag.begin(), tag.end());
    output.insert(output.end(), ciphertext.begin(), ciphertext.end());
    return output;
}

std::vector<uint8_t> decryptBytes(JNIEnv* env, const std::vector<uint8_t>& protected_bytes) {
    const auto key = protectionKey();
    if (key.empty()) {
        throwJava(env, "java/lang/IllegalStateException", "ToolPkg protection secret is not configured");
        return {};
    }
    if (protected_bytes.size() < kHeaderSize ||
        !std::equal(kMagic.begin(), kMagic.end(), protected_bytes.begin())) {
        throwJava(env, "java/lang/IllegalArgumentException", "Not an Operit protected payload");
        return {};
    }

    std::array<uint8_t, kNonceSize> nonce = {};
    std::copy(
            protected_bytes.begin() + static_cast<std::ptrdiff_t>(kMagic.size()),
            protected_bytes.begin() + static_cast<std::ptrdiff_t>(kMagic.size() + nonce.size()),
            nonce.begin());
    const uint8_t* provided_tag = protected_bytes.data() + kMagic.size() + nonce.size();
    std::vector<uint8_t> ciphertext(
            protected_bytes.begin() + static_cast<std::ptrdiff_t>(kHeaderSize),
            protected_bytes.end());
    const auto expected_tag = hmacSha256(key, buildTagMessage(nonce, ciphertext));
    if (!constantTimeEquals(provided_tag, expected_tag.data(), expected_tag.size())) {
        throwJava(env, "java/lang/SecurityException", "Protected payload authentication failed");
        return {};
    }
    return xorWithKeystream(ciphertext, key, nonce);
}

}  // namespace

extern "C" JNIEXPORT jboolean JNICALL
Java_com_ai_assistance_operit_util_ToolPkgProtectionNative_isSecretConfigured(JNIEnv*, jobject) {
    return secretConfigured() ? JNI_TRUE : JNI_FALSE;
}

extern "C" JNIEXPORT jbyteArray JNICALL
Java_com_ai_assistance_operit_util_ToolPkgProtectionNative_encrypt(JNIEnv* env, jobject, jbyteArray bytes) {
    const auto plaintext = jbyteArrayToVector(env, bytes);
    if (env->ExceptionCheck()) return nullptr;
    const auto encrypted = encryptBytes(env, plaintext);
    if (env->ExceptionCheck()) return nullptr;
    return vectorToJbyteArray(env, encrypted);
}

extern "C" JNIEXPORT jbyteArray JNICALL
Java_com_ai_assistance_operit_util_ToolPkgProtectionNative_decrypt(JNIEnv* env, jobject, jbyteArray bytes) {
    const auto protected_bytes = jbyteArrayToVector(env, bytes);
    if (env->ExceptionCheck()) return nullptr;
    const auto decrypted = decryptBytes(env, protected_bytes);
    if (env->ExceptionCheck()) return nullptr;
    return vectorToJbyteArray(env, decrypted);
}
