#pragma once

/// Multimodal utilities — image encoding, content type detection.

#include <string>
#include <vector>
#include <optional>

namespace zeroclaw {
namespace multimodal {

/// Supported content types for multimodal input
enum class ContentType { Text, Image, Audio, Video, Document };

/// Encode a file as base64 for multimodal API calls
std::string encode_file_base64(const std::string& file_path);

/// Detect content type from file extension
ContentType detect_content_type(const std::string& file_path);

/// MIME type string from content type
std::string mime_type(ContentType type);

/// Check if a provider supports multimodal input
bool provider_supports_multimodal(const std::string& provider_name);

} // namespace multimodal
} // namespace zeroclaw
