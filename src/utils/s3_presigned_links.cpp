#include "s3_presigned_links.hpp"

#include <aws/core/Aws.h>
#include <aws/core/auth/AWSCredentialsProvider.h>
#include <aws/core/http/HttpTypes.h>
#include <aws/http/http.h>
#include <aws/s3/S3Client.h>

namespace utils::s3_presigned_links {

namespace {

std::string EscapeFilenameForContentDisposition(const std::string& name) {
  std::string out;
  out.reserve(name.size() + 8);
  for (unsigned char c : name) {
    if (c == '\\') {
      out += "\\\\";
    } else if (c == '"') {
      out += "\\\"";
    } else {
      out += static_cast<char>(c);
    }
  }
  return out;
}

}  // namespace

std::string GeneratePresignedLink(const std::string& key, const LinkType type,
                                  bool is_testing,
                                  const std::string& bucket) {
  std::string result;
  if (is_testing) {
    switch (type) {
      case LinkType::Upload:
        result = "s3 upload test link";
        break;

      case LinkType::Download:
        result = "s3 download test link";
        break;

      default:
        break;
    }
  } else {
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

std::string GeneratePresignedLink(const std::string& key, const LinkType type,
  const std::string& bucket) {
return GeneratePresignedLink(key, type, false, bucket);
}

std::string GeneratePhotoPresignedLink(const std::string& key,
                                       const LinkType type, bool is_testing) {
  return GeneratePresignedLink(key, type, is_testing, "working-day-photos");
}

std::string GenerateDocumentPresignedLink(const std::string& key,
                                          const LinkType type, bool is_testing) {
  return GeneratePresignedLink(key, type, is_testing, "working-day-documents");
}

std::string GenerateDocumentPresignedDownloadWithFilename(const std::string& key,
                                                          const std::string& download_filename,
                                                          bool is_testing) {
  if (is_testing) {
    return "s3 download test link";
  }
  Aws::Client::ClientConfiguration config;
  config.region = Aws::String("ru-central1");
  config.endpointOverride = Aws::String("https://storage.yandexcloud.net");

  Aws::S3::S3Client s3_client(config);
  const std::string disposition =
      std::string("attachment; filename=\"") +
      EscapeFilenameForContentDisposition(download_filename) + "\"";
  Aws::Http::HeaderValueCollection headers;
  headers.emplace("response-content-disposition", disposition);

  return s3_client.GeneratePresignedUrl(Aws::String("working-day-documents"),
                                        Aws::String(key.c_str()),
                                        Aws::Http::HttpMethod::HTTP_GET, headers,
                                        600);
}

std::string GenerateTrackerTasksMediaPresignedLink(const std::string& key,
                                                   const LinkType type, bool is_testing) {
  // return GeneratePresignedLink(key, type, "working-day-tracker-tasks-media");
  return GeneratePresignedLink(key, type, is_testing, "working-day-photos");
}

std::string GenerateTrackerProjectsMediaPresignedLink(const std::string& key,
                                                   const LinkType type, bool is_testing) {
  // return GeneratePresignedLink(key, type, "working-day-tracker-projects-media");
  return GeneratePresignedLink(key, type, is_testing, "working-day-photos");
}

}  // namespace utils::s3_presigned_links
