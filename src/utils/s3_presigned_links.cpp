#include "s3_presigned_links.hpp"

#include <aws/core/Aws.h>
#include <aws/core/auth/AWSCredentialsProvider.h>
#include <aws/http/http.h>
#include <aws/s3/S3Client.h>

namespace utils::s3_presigned_links {

std::string GeneratePresignedLink(const std::string& key, const LinkType type,
                                  const std::string& bucket) {
  std::string result;

  {
    Aws::Client::ClientConfiguration config;
    config.region = Aws::String("ru-central1");
    config.endpointOverride = Aws::String("https://storage.yandexcloud.net");

    Aws::String bucket_name = bucket;
    Aws::S3::S3Client s3_client(config);

    switch (type) {
      case LinkType::Upload:
        result = s3_client.GeneratePresignedUrl(
            bucket_name, key, Aws::Http::HttpMethod::HTTP_PUT, 600);
        break;

      case LinkType::Download:
        result = s3_client.GeneratePresignedUrl(
            bucket_name, key, Aws::Http::HttpMethod::HTTP_GET, 600);
        break;

      default:
        break;
    }
  }

  return result;
}

std::string GeneratePhotoPresignedLink(const std::string& key,
                                       const LinkType type) {
  return GeneratePresignedLink(key, type, "working-day-photos");
}

std::string GenerateDocumentPresignedLink(const std::string& key,
                                          const LinkType type) {
  return GeneratePresignedLink(key, type, "working-day-documents");
}

}  // namespace utils::s3_presigned_links