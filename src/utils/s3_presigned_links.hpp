#include <string>

namespace utils::s3_presigned_links {

enum LinkType {
  Upload = 1,
  Download = 2,
};

std::string GeneratePhotoPresignedLink(const std::string& key,
                                       const LinkType type,
                                       bool is_testing = false);

std::string GenerateDocumentPresignedLink(const std::string& key,
                                          const LinkType type,
                                          bool is_testing = false);

/// Presigned GET with Content-Disposition so the browser saves `download_filename`.
std::string GenerateDocumentPresignedDownloadWithFilename(const std::string& key,
                                                          const std::string& download_filename,
                                                          bool is_testing = false);

std::string GenerateTrackerTasksMediaPresignedLink(const std::string& key,
                                                   const LinkType type,
                                                   bool is_testing = false);

std::string GenerateTrackerProjectsMediaPresignedLink(const std::string& key,
                                                   const LinkType type,
                                                   bool is_testing = false);

}  // namespace utils::s3_presigned_links