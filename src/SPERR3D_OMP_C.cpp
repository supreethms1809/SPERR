#include "SPERR3D_OMP_C.h"

#include <algorithm>  // std::all_of()
#include <cassert>
#include <cstring>
#include <numeric>  // std::accumulate()

#ifdef USE_OMP
#include <omp.h>
#endif

void SPERR3D_OMP_C::set_num_threads(size_t n)
{
  if (n > 0)
    m_num_threads = n;
}

void SPERR3D_OMP_C::toggle_conditioning(sperr::Conditioner::settings_type b4)
{
  m_conditioning_settings = b4;
}

#ifdef QZ_TERM
void SPERR3D_OMP_C::set_qz_level(int32_t q)
{
  m_qz_lev = q;
}
void SPERR3D_OMP_C::set_tolerance(double t)
{
  m_tol = t;
}
auto SPERR3D_OMP_C::get_outlier_stats() const -> std::pair<size_t, size_t>
{
  using pair = std::pair<size_t, size_t>;
  pair sum{0, 0};
  auto op = [](const pair& a, const pair& b) -> pair {
    return {a.first + b.first, a.second + b.second};
  };
  return std::accumulate(m_outlier_stats.begin(), m_outlier_stats.end(), sum, op);
}
#else
auto SPERR3D_OMP_C::set_bpp(double bpp) -> RTNType
{
  auto eq0 = [](auto v) { return v == 0; };

  if (bpp < 0.0 || bpp > 64.0)
    return RTNType::InvalidParam;
  // If the volume and chunk dimension hasn't been set, return error.
  else if (std::any_of(m_dims.begin(), m_dims.end(), eq0) ||
           std::any_of(m_chunk_dims.begin(), m_chunk_dims.end(), eq0))
    return RTNType::SetBPPBeforeDims;
  else {
    // Need to account in the size of the header.
    const auto total_vals_f = static_cast<double>(m_dims[0] * m_dims[1] * m_dims[2]);
    double total_bits = bpp * total_vals_f;
    const auto chunks = sperr::chunk_volume(m_dims, m_chunk_dims);
    const auto header_bits = (m_header_magic + chunks.size() * 4) * 8;
    total_bits -= header_bits;
    m_bpp = total_bits / total_vals_f;

    return RTNType::Good;
  }
}
#endif

template <typename T>
auto SPERR3D_OMP_C::copy_data(const T* vol,
                              size_t len,
                              sperr::dims_type vol_dims,
                              sperr::dims_type chunk_dims) -> RTNType
{
  if (len != vol_dims[0] * vol_dims[1] * vol_dims[2])
    return RTNType::WrongDims;
  else
    m_dims = vol_dims;

  // The preferred chunk size has to be between 1 and m_dims.
  for (size_t i = 0; i < m_chunk_dims.size(); i++)
    m_chunk_dims[i] = std::min(std::max(size_t{1}, chunk_dims[i]), vol_dims[i]);

  // Block the volume into smaller chunks
  const auto chunks = sperr::chunk_volume(m_dims, m_chunk_dims);
  const auto num_chunks = chunks.size();
  m_chunk_buffers.resize(num_chunks);

#pragma omp parallel for num_threads(m_num_threads)
  for (size_t i = 0; i < num_chunks; i++) {
    m_chunk_buffers[i] = sperr::gather_chunk<T, double>(vol, m_dims, chunks[i]);
  }

  return RTNType::Good;
}
template auto SPERR3D_OMP_C::copy_data(const float*, size_t, sperr::dims_type, sperr::dims_type)
    -> RTNType;
template auto SPERR3D_OMP_C::copy_data(const double*, size_t, sperr::dims_type, sperr::dims_type)
    -> RTNType;

auto SPERR3D_OMP_C::compress() -> RTNType
{
  // Need to make sure that the chunks are ready!
  auto chunks = sperr::chunk_volume(m_dims, m_chunk_dims);
  const auto num_chunks = chunks.size();
  if (m_chunk_buffers.size() != num_chunks)
    return RTNType::Error;
  if (std::any_of(m_chunk_buffers.begin(), m_chunk_buffers.end(),
                  [](auto& v) { return v.empty(); }))
    return RTNType::Error;

  // Let's prepare some data structures for compression!
  auto compressors = std::vector<sperr::SPERR3D_Compressor>(m_num_threads);
  auto chunk_rtn = std::vector<RTNType>(num_chunks, RTNType::Good);
  m_encoded_streams.resize(num_chunks);
  std::for_each(m_encoded_streams.begin(), m_encoded_streams.end(), [](auto& v) { v.clear(); });

#ifdef QZ_TERM
  m_outlier_stats.assign(num_chunks, {0, 0});
#endif

// Each thread uses a compressor instance to work on a chunk.
//
#pragma omp parallel for num_threads(m_num_threads)
  for (size_t i = 0; i < num_chunks; i++) {
#ifdef USE_OMP
    auto& compressor = compressors[omp_get_thread_num()];
#else
    auto& compressor = compressors[0];
#endif

    // The following few operations have no chance to fail.
    compressor.take_data(std::move(m_chunk_buffers[i]), {chunks[i][1], chunks[i][3], chunks[i][5]});

    compressor.toggle_conditioning(m_conditioning_settings);

#ifdef QZ_TERM
    compressor.set_qz_level(m_qz_lev);
    compressor.set_tolerance(m_tol);
#else
    compressor.set_bpp(m_bpp);
#endif

    // Action items
    chunk_rtn[i] = compressor.compress();
    m_encoded_streams[i] = compressor.release_encoded_bitstream();

#ifdef QZ_TERM
    m_outlier_stats[i] = compressor.get_outlier_stats();
#endif
  }

  auto fail =
      std::find_if(chunk_rtn.begin(), chunk_rtn.end(), [](auto r) { return r != RTNType::Good; });
  if (fail != chunk_rtn.end())
    return (*fail);

  if (std::any_of(m_encoded_streams.begin(), m_encoded_streams.end(),
                  [](auto& s) { return s.empty(); }))
    return RTNType::EmptyStream;

  return RTNType::Good;
}

auto SPERR3D_OMP_C::get_encoded_bitstream() const -> std::vector<uint8_t>
{
  auto buf = std::vector<uint8_t>();
  auto header = m_generate_header();
  if (header.empty())
    return buf;

  auto total_size =
      std::accumulate(m_encoded_streams.begin(), m_encoded_streams.end(), header.size(),
                      [](size_t a, const auto& b) { return a + b.size(); });
  buf.resize(total_size, 0);

  std::copy(header.begin(), header.end(), buf.begin());
  auto itr = buf.begin() + header.size();
  for (const auto& s : m_encoded_streams) {
    std::copy(s.begin(), s.end(), itr);
    itr += s.size();
  }

  return buf;
}

auto SPERR3D_OMP_C::m_generate_header() const -> sperr::vec8_type
{
  // The header would contain the following information
  //  -- a version number                     (1 byte)
  //  -- 8 booleans                           (1 byte)
  //  -- volume and chunk dimensions          (4 x 6 = 24 bytes)
  //  -- length of bitstream for each chunk   (4 x num_chunks)

  auto chunks = sperr::chunk_volume(m_dims, m_chunk_dims);
  const auto num_chunks = chunks.size();
  if (num_chunks != m_encoded_streams.size())
    return std::vector<uint8_t>();
  const auto header_size = m_header_magic + num_chunks * 4;
  auto header = std::vector<uint8_t>(header_size);

  // Version number
  header[0] = static_cast<uint8_t>(SPERR_VERSION_MAJOR);
  size_t loc = 1;

  // 8 booleans:
  // bool[0]  : if ZSTD is used
  // bool[1]  : if this bitstream is for 3D (true) or 2D (false) data.
  // bool[2]  : if this bitstream is in QZ_TERM mode (true) or fixed-size mode (false).
  // bool[3-7]: undefined
  //
  auto b8 = std::array<bool, 8>{false, false, false, false, false, false, false, false};

#ifdef USE_ZSTD
  b8[0] = true;
#endif

  b8[1] = true;

#ifdef QZ_TERM
  b8[2] = true;
#endif

  header[loc] = sperr::pack_8_booleans(b8);
  loc += 1;

  // Volume and chunk dimensions
  uint32_t vcdim[6] = {
      static_cast<uint32_t>(m_dims[0]),       static_cast<uint32_t>(m_dims[1]),
      static_cast<uint32_t>(m_dims[2]),       static_cast<uint32_t>(m_chunk_dims[0]),
      static_cast<uint32_t>(m_chunk_dims[1]), static_cast<uint32_t>(m_chunk_dims[2])};
  std::memcpy(&header[loc], vcdim, sizeof(vcdim));
  loc += sizeof(vcdim);

  // Length of bitstream for each chunk
  // Note that we use uint32_t to keep the length, and we need to make sure
  // that no chunk size is bigger than that.
  for (const auto& stream : m_encoded_streams) {
    assert(stream.size() <= uint64_t{std::numeric_limits<uint32_t>::max()});
    uint32_t len = stream.size();
    std::memcpy(&header[loc], &len, sizeof(len));
    loc += sizeof(len);
  }
  assert(loc == header_size);

  return header;
}