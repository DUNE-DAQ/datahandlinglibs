/**
 * @file FileSourceBuffer.hpp Reads in data from raw binary dump files
 *
 * This is part of the DUNE DAQ , copyright 2020.
 * Licensing/copyright details are in the COPYING file that you should have
 * received with this code.
 */
#ifndef DATAHANDLINGLIBS_INCLUDE_DATAHANDLINGLIBS_UTILS_FILESOURCEBUFFER_HPP_
#define DATAHANDLINGLIBS_INCLUDE_DATAHANDLINGLIBS_UTILS_FILESOURCEBUFFER_HPP_

#include "datahandlinglibs/DataHandlingIssues.hpp"
#include "datahandlinglibs/ReadoutLogging.hpp"

#include "logging/Logging.hpp"

#include <fstream>
#include <limits>
#include <string>
#include <vector>

using dunedaq::datahandlinglibs::logging::TLVL_BOOKKEEPING;

namespace dunedaq {
namespace datahandlinglibs {

class FileSourceBuffer
{
public:
  explicit FileSourceBuffer(int input_limit, int chunk_size = 0)
    : m_input_limit(input_limit)
    , m_chunk_size(chunk_size)
    , m_element_count(0)
    , m_source_filename("")
  {}

  FileSourceBuffer(const FileSourceBuffer&) = delete;            ///< FileSourceBuffer is not copy-constructible
  FileSourceBuffer& operator=(const FileSourceBuffer&) = delete; ///< FileSourceBuffer is not copy-assginable
  FileSourceBuffer(FileSourceBuffer&&) = delete;                 ///< FileSourceBuffer is not move-constructible
  FileSourceBuffer& operator=(FileSourceBuffer&&) = delete;      ///< FileSourceBuffer is not move-assignable

  void read(const std::string& sourcefile)
  {
    m_source_filename = sourcefile;
    try {

      // Open file
      m_rawdata_ifs.open(m_source_filename, std::ios::in | std::ios::binary);
      if (!m_rawdata_ifs) {
        throw CannotOpenFile(ERS_HERE, m_source_filename);
      }

      // Check file size
      m_rawdata_ifs.ignore(std::numeric_limits<std::streamsize>::max());
      std::streamsize filesize = m_rawdata_ifs.gcount();
      if (filesize > m_input_limit) { // bigger than configured limit
        std::ostringstream oss;
        oss << "File size limit exceeded, "
            << "filesize is " << filesize << ", "
            << "configured limit is " << m_input_limit << ", "
            << "filename is " << m_source_filename;
        ers::warning(GenericConfigurationError(ERS_HERE, oss.str()));
      }

      // Check for exact match
      if (m_chunk_size > 0) {
        int remainder = filesize % m_chunk_size;
        if (remainder > 0) {
          std::ostringstream oss;
          oss << "Binary file contains more data than expected, "
              << "filesize is " << filesize << ", "
              << "chunk_size is " << m_chunk_size << ", "
              << "filename is " << m_source_filename;
          ers::warning(GenericConfigurationError(ERS_HERE, oss.str()));
        }
        // Set usable element count
        m_element_count = filesize / m_chunk_size;
        TLOG_DEBUG(TLVL_BOOKKEEPING) << "Available elements: " << std::to_string(m_element_count);
      }

      // Read file into input buffer
      m_rawdata_ifs.seekg(0, std::ios::beg);
      m_input_buffer.reserve(filesize);
      m_input_buffer.insert(
        m_input_buffer.begin(), std::istreambuf_iterator<char>(m_rawdata_ifs), std::istreambuf_iterator<char>());
      TLOG_DEBUG(TLVL_BOOKKEEPING) << "Available bytes " << std::to_string(m_input_buffer.size());

    } catch (const std::exception& ex) {
      throw GenericConfigurationError(ERS_HERE, "Cannot read file: " + m_source_filename, ex.what());
    }
  }

  const int& num_elements() { return std::ref(m_element_count); }

  std::vector<std::uint8_t>& get() // NOLINT(build/unsigned)
  {
    return std::ref(m_input_buffer);
  }

private:
  // Configuration
  int m_input_limit;
  int m_chunk_size;
  int m_element_count;
  std::string m_source_filename;

  // Internals
  std::ifstream m_rawdata_ifs;
  std::vector<std::uint8_t> m_input_buffer; // NOLINT(build/unsigned)
};

} // namespace datahandlinglibs
} // namespace dunedaq

#endif // DATAHANDLINGLIBS_INCLUDE_DATAHANDLINGLIBS_UTILS_FILESOURCEBUFFER_HPP_
