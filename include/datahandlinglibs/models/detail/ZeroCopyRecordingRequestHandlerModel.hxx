// Declarations for ZeroCopyRecordingRequestHandlerModel

namespace dunedaq {
namespace datahandlinglibs {

// Special configuration that checks LB alignment and O_DIRECT flag on output file
template<class ReadoutType, class LatencyBufferType>
void 
ZeroCopyRecordingRequestHandlerModel<ReadoutType, LatencyBufferType>::conf(const appmodel::DataHandlerModule* conf)
{
  auto data_rec_conf = conf->get_module_configuration()->get_request_handler()->get_data_recorder();
  
  if (data_rec_conf != nullptr) {
    if (!data_rec_conf->get_output_file().empty()) {
      inherited::m_sourceid.id = conf->get_source_id();
      inherited::m_sourceid.subsystem = ReadoutType::subsystem;
  
      // Check for alignment restrictions for filesystem block size. (XFS default: 4096) 
      if (inherited::m_latency_buffer->get_alignment_size() == 0 ||
          sizeof(ReadoutType) * inherited::m_latency_buffer->size() % 4096) {
        ers::error(ConfigurationError(ERS_HERE, inherited::m_sourceid, "Latency buffer is not 4kB aligned"));
      }

      // Check for sensible stream chunk size
      inherited::m_stream_buffer_size = data_rec_conf->get_streaming_buffer_size();
      if (inherited::m_stream_buffer_size % 4096 != 0) {
        ers::error(ConfigurationError(ERS_HERE, inherited::m_sourceid, "Streaming chunk size is not divisible by 4kB!"));
      }
  
      // Prepare filename with full path 
      std::string file_full_path = data_rec_conf->get_output_file() + inherited::m_sourceid.to_string() + std::string(".bin");
      inherited::m_output_file = file_full_path;

  
      // RS: This will need to go away with the SNB store handler!
      if (std::remove(file_full_path.c_str()) == 0) {
        TLOG(TLVL_WORK_STEPS) << "Removed existing output file from previous run: " << file_full_path;
      }
  
      m_oflag = O_CREAT | O_WRONLY;
      if (data_rec_conf->get_use_o_direct()) {
        m_oflag |= O_DIRECT;
      }
      m_fd = ::open(file_full_path.c_str(), m_oflag, 0644);
      if (m_fd == -1) {
        TLOG() << "Failed to open file!";
        throw ConfigurationError(ERS_HERE, inherited::m_sourceid, "Failed to open file!");
      }
      inherited::m_recording_configured = true;

    } else { // no output dir specified
      TLOG(TLVL_WORK_STEPS) << "No output path is specified in data recorder config. Recording feature is inactive.";
    } 
  } else {
    TLOG(TLVL_WORK_STEPS) << "No recording config object specified. Recording feature is inactive."; 
  }
  
  inherited::conf(conf);
}

// Special record command that writes to files from memory aligned LBs
template<class ReadoutType, class LatencyBufferType>
void 
ZeroCopyRecordingRequestHandlerModel<ReadoutType, LatencyBufferType>::record(const nlohmann::json& cmdargs)
{
  if (inherited::m_recording.load()) {
    ers::error(
      CommandError(ERS_HERE, inherited::m_sourceid, "A recording is still running, no new recording was started!"));
    return;
  }

// FIXME: Recording parameters to be clarified!
  int recording_time_sec = 0;
  if (cmdargs.contains("duration")) {
    recording_time_sec = cmdargs["duration"];
  } else {
    ers::warning(
      CommandError(ERS_HERE, inherited::m_sourceid, "A recording command with missing duration field received!"));
  }
  if (recording_time_sec == 0) {
    ers::warning(
      CommandError(ERS_HERE, inherited::m_sourceid, "Recording for 0 seconds requested. Recording command is ignored!"));
    return;
  }

  inherited::m_recording_thread.set_work(
    [&](int duration) {
      size_t chunk_size = inherited::m_stream_buffer_size;
      size_t alignment_size = inherited::m_latency_buffer->get_alignment_size();
      TLOG() << "Start recording for " << duration << " second(s)" << std::endl;
      inherited::m_recording.exchange(true);
      auto start_of_recording = std::chrono::high_resolution_clock::now();
      auto current_time = start_of_recording;
      inherited::m_next_timestamp_to_record = 0;

      const char* current_write_pointer = nullptr;
      const char* start_of_buffer_pointer =
        reinterpret_cast<const char*>(inherited::m_latency_buffer->start_of_buffer()); // NOLINT
      const char* current_end_pointer;
      const char* end_of_buffer_pointer = reinterpret_cast<const char*>(inherited::m_latency_buffer->end_of_buffer()); // NOLINT

      size_t bytes_written = 0;
      size_t failed_writes = 0;

      while (std::chrono::duration_cast<std::chrono::seconds>(current_time - start_of_recording).count() < duration) {
        if (!inherited::m_cleanup_requested || (inherited::m_next_timestamp_to_record == 0)) {
          size_t considered_chunks_in_loop = 0;

          // Wait for potential running cleanup to finish first
          {
            std::unique_lock<std::mutex> lock(inherited::m_cv_mutex);
            inherited::m_cv.wait(lock, [&] { return !inherited::m_cleanup_requested; });
          }
          inherited::m_cv.notify_all();

          // Some frames have to be skipped to start copying from an aligned piece of memory
          // These frames cannot be written without O_DIRECT as this would mess up the alignment of the write pointer
          // into the target file
          if (inherited::m_next_timestamp_to_record == 0) {
            auto begin = inherited::m_latency_buffer->begin();
            if (begin == inherited::m_latency_buffer->end()) {
              // There are no elements in the buffer, update time and try again
              current_time = std::chrono::high_resolution_clock::now();
              continue;
            }
            inherited::m_next_timestamp_to_record = begin->get_timestamp();
            size_t skipped_frames = 0;
            while (reinterpret_cast<std::uintptr_t>(&(*begin)) % alignment_size) { // NOLINT
              ++begin;
              skipped_frames++;
              if (!begin.good()) {
                // We reached the end of the buffer without finding an aligned element
                // Reset the next timestamp to record and try again
                current_time = std::chrono::high_resolution_clock::now();
                inherited::m_next_timestamp_to_record = 0;
                continue;
              }
            }
            TLOG() << "Skipped " << skipped_frames << " frames";
            current_write_pointer = reinterpret_cast<const char*>(&(*begin)); // NOLINT
          }

          current_end_pointer = reinterpret_cast<const char*>(inherited::m_latency_buffer->back()); // NOLINT

          // Break the loop from time to time to update the timestamp and check if we should stop recording
          while (considered_chunks_in_loop < 100) {
            auto iptr = reinterpret_cast<std::uintptr_t>(current_write_pointer); // NOLINT
            if (iptr % alignment_size) {
              // This should never happen
              TLOG() << "Error: Write pointer is not aligned";
            }
            bool failed_write = false;
            if (current_write_pointer + chunk_size < current_end_pointer) {
              // We can write a whole chunk to file
              failed_write |= !::write(m_fd, current_write_pointer, chunk_size);
              if (!failed_write) {
                bytes_written += chunk_size;
              }
              current_write_pointer += chunk_size;
            } else if (current_end_pointer < current_write_pointer) {
              if (current_write_pointer + chunk_size < end_of_buffer_pointer) {
                // Write whole chunk to file
                failed_write |= !::write(m_fd, current_write_pointer, chunk_size);
                if (!failed_write) {
                  bytes_written += chunk_size;
                }
                current_write_pointer += chunk_size;
              } else {
                // Write the last bit of the buffer without using O_DIRECT as it possibly doesn't fulfill the
                // alignment requirement
                fcntl(m_fd, F_SETFL, O_CREAT | O_WRONLY);
                failed_write |= !::write(m_fd, current_write_pointer, end_of_buffer_pointer - current_write_pointer);
                fcntl(m_fd, F_SETFL, m_oflag);
                if (!failed_write) {
                  bytes_written += end_of_buffer_pointer - current_write_pointer;
                }
                current_write_pointer = start_of_buffer_pointer;
              }
            }

            if (current_write_pointer == end_of_buffer_pointer) {
              current_write_pointer = start_of_buffer_pointer;
            }

            if (failed_write) {
              ++failed_writes;
              ers::warning(CannotWriteToFile(ERS_HERE, inherited::m_output_file));
            }
            considered_chunks_in_loop++;
            // This expression is "a bit" complicated as it finds the last frame that was written to file completely
            inherited::m_next_timestamp_to_record =
              reinterpret_cast<const ReadoutType*>( // NOLINT
                start_of_buffer_pointer +
                (((current_write_pointer - start_of_buffer_pointer) / ReadoutType::fixed_payload_size) *
                 ReadoutType::fixed_payload_size))
                ->get_timestamp();
          }
        }
        current_time = std::chrono::high_resolution_clock::now();
      }

      // Complete writing the last frame to file
      if (current_write_pointer != nullptr) {
        const char* last_started_frame =
          start_of_buffer_pointer +
          (((current_write_pointer - start_of_buffer_pointer) / ReadoutType::fixed_payload_size) *
           ReadoutType::fixed_payload_size);
        if (last_started_frame != current_write_pointer) {
          fcntl(m_fd, F_SETFL, O_CREAT | O_WRONLY);
          if (!::write(m_fd,
                       current_write_pointer,
                       (last_started_frame + ReadoutType::fixed_payload_size) - current_write_pointer)) {
            ers::warning(CannotWriteToFile(ERS_HERE, inherited::m_output_file));
          } else {
            bytes_written += (last_started_frame + ReadoutType::fixed_payload_size) - current_write_pointer;
          }
        }
      }
      ::close(m_fd);

      inherited::m_next_timestamp_to_record = std::numeric_limits<uint64_t>::max(); // NOLINT (build/unsigned)

      TLOG() << "Stopped recording, wrote " << bytes_written << " bytes. Failed write count: " << failed_writes;
      inherited::m_recording.exchange(false);
    }, recording_time_sec);
}

} // namespace datahandlinglibs
} // namespace dunedaq
