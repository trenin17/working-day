#include <string>

namespace utils::s3_presigned_links {

enum LinkType {
  Upload = 1,
  Download = 2,
};

std::string GeneratePhotoPresignedLink(const std::string& key, const LinkType type);

} // namespace utils::s3_presigned_links