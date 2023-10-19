#include <ice/postcard.hxx>
#include <memory>
#include <cassert>
#include <span>

#if !defined(ICE_POSTCARD_SIMD_ENABLED)
#define ICE_POSTCARD_SIMD_ENABLED 0
//#define ICE_POSTCARD_SIMD_AVX_ENABLED 0 TODO: We might want to also make use of avx?
#else
#undef ICE_POSTCARD_SIMD_ENABLED
#define ICE_POSTCARD_SIMD_ENABLED 1
#endif

#if ICE_POSTCARD_SIMD_ENABLED
#include <intrin.h>
#endif
namespace ice::postcard
{

    class DefaultAllocator : public ice::postcard::Allocator
    {
        auto allocate(ice::postcard::usize size) noexcept -> ice::postcard::Memory
        {
            return { malloc(size), size };
        }

        void deallocate(ice::postcard::Memory memory) noexcept
        {
            free(memory.location);
        }
    };

    auto ice::postcard::Allocator::get_default() noexcept -> ice::postcard::Allocator&
    {
        static ice::postcard::DefaultAllocator malloc_global;
        return malloc_global;
    }

    Attachment::Attachment() noexcept
        : Attachment{ ice::postcard::Memory{}, ice::postcard::Allocator::get_default() }
    {
    }

    Attachment::Attachment(ice::postcard::Data data) noexcept
        : Attachment{ { }, ice::postcard::Allocator::get_default() }
    {
        if (data.size > 0)
        {
            _data = _allocator->allocate(data.size);
            std::memcpy(_data.location, data.location, data.size);
        }
    }

    Attachment::Attachment(ice::postcard::Memory memory, ice::postcard::Allocator& alloc) noexcept
        : _data{ memory }
        , _allocator{ &alloc }
    {
    }

    Attachment::~Attachment() noexcept
    {
        assert(_allocator != nullptr);
        _allocator->deallocate(_data);
    }

    namespace detail
    {

        auto write_postcard_data(
            ice::postcard::Memory target,
            ice::postcard::Data source,
            ice::postcard::u8 channel_count,
            ice::postcard::u8& out_last_written_channel
        ) noexcept -> ice::postcard::usize;

        auto read_postcard_data(
            ice::postcard::Memory target,
            ice::postcard::Data source,
            ice::postcard::u8 channel_count,
            ice::postcard::u8& out_last_read_channel
        ) noexcept -> ice::postcard::usize;

        namespace simd
        {

            auto write_postcard_data(
                ice::postcard::Memory target,
                ice::postcard::Data source,
                ice::postcard::u8 channel_count,
                ice::postcard::u8& out_last_written_channel
            ) noexcept -> ice::postcard::usize;

            auto read_postcard_data(
                ice::postcard::Memory& target,
                ice::postcard::Data source,
                ice::postcard::u8 channel_count,
                ice::postcard::u8& out_last_read_channel
            ) noexcept -> ice::postcard::usize;

            static constexpr bool Constant_EnabledSIMD = ICE_POSTCARD_SIMD_ENABLED == 1;

            // TODO: Might be usefull later
            //static constexpr bool Constant_EnabledAVX = false;
            //static constexpr ice::u8 Constant_ComponentCount = Constant_EnabledAVX ? 32 : 16;

            static constexpr ice::postcard::u8 Constant_ComponentCopyMask[32]{
                0x00, 0x00, 0x00, 0x00,
                0x00, 0x00, 0x00, 0x00,
                0x01, 0x01, 0x01, 0x01,
                0x01, 0x01, 0x01, 0x01,
                0x02, 0x02, 0x02, 0x02,
                0x02, 0x02, 0x02, 0x02,
                0x03, 0x03, 0x03, 0x03,
                0x03, 0x03, 0x03, 0x03,
            };

            // TODO: Fix issue with reading 4 channel images with data
            //static constexpr ice::u8 Constant_SSE_ComponentRead4Mask[16]{
            //    0x00, 0x01, 0x02, 0x04, // 1'st byte
            //    0x05, 0x06, 0x08, 0x09, // 1/2'st byte
            //    0x05, 0x06, 0x08, 0x09, // 1/2'st byte
            //    0x0a, 0x0c, 0x0d, 0x0e, // 2'st byte
            //    // Represents alpha channels, we don't use
            //    //0x03, 0x07, 0x0b, 0x0f,
            //};

            static constexpr ice::postcard::u8 Constant_BitSelectorMask[32]{
                0x01, 0x02, 0x04, 0x08,
                0x10, 0x20, 0x40, 0x80,
                0x01, 0x02, 0x04, 0x08,
                0x10, 0x20, 0x40, 0x80,
                0x01, 0x02, 0x04, 0x08,
                0x10, 0x20, 0x40, 0x80,
                0x01, 0x02, 0x04, 0x08,
                0x10, 0x20, 0x40, 0x80,
            };

            static constexpr ice::postcard::u8 Constant_LSBChannelClearMask[32]{
                0xfe, 0xfe, 0xfe, 0xfe,
                0xfe, 0xfe, 0xfe, 0xfe,
                0xfe, 0xfe, 0xfe, 0xfe,
                0xfe, 0xfe, 0xfe, 0xfe,
                0xfe, 0xfe, 0xfe, 0xfe,
                0xfe, 0xfe, 0xfe, 0xfe,
                0xfe, 0xfe, 0xfe, 0xfe,
                0xfe, 0xfe, 0xfe, 0xfe,
            };

            static constexpr ice::postcard::u8 Constant_LSBChannelKeepMask[32]{
                0x01, 0x01, 0x01, 0x01,
                0x01, 0x01, 0x01, 0x01,
                0x01, 0x01, 0x01, 0x01,
                0x01, 0x01, 0x01, 0x01,
                0x01, 0x01, 0x01, 0x01,
                0x01, 0x01, 0x01, 0x01,
                0x01, 0x01, 0x01, 0x01,
                0x01, 0x01, 0x01, 0x01,
            };

        } // namespace simd

    } // namespace detail

    static constexpr ice::postcard::u8 Constant_UsedChannels[]{ 0, 1, 2 }; // 0=r, 1=g, 2=b, 3=a
    static constexpr ice::postcard::u32 Constant_ChannelsUsedPerByte = sizeof(ice::postcard::u8) * 8;

    struct PostcardHeader
    {
        static constexpr ice::postcard::u32 Constant_Magic = 'ISPC';
        ice::postcard::u32 magic = Constant_Magic;
        ice::postcard::u16 unused;
        ice::postcard::u16 revision;
        ice::postcard::u32 attachment_size;
    };

    auto capacity(ice::postcard::Image const& image) noexcept -> ice::postcard::usize
    {
        ice::postcard::usize const total_available_bytes = ((image.width * image.height * std::size(Constant_UsedChannels)) / Constant_ChannelsUsedPerByte);
        return total_available_bytes - sizeof(PostcardHeader);
    }

    auto write(
        ice::postcard::Image& image,
        ice::postcard::PostcardInfo const& info,
        ice::postcard::Attachment const& attachment
    ) noexcept -> ice::postcard::Result
    {
        return write(image, info, ice::postcard::Data{ attachment._data.location, attachment._data.size });
    }

    auto write(
        ice::postcard::Image& image,
        ice::postcard::PostcardInfo const& info,
        ice::postcard::Data const& attachment_data
    ) noexcept -> ice::postcard::Result
    {
        using ice::postcard::Result;
        assert(info.attachment_size == 0 || info.attachment_size == attachment_data.size);

        if (capacity(image) < attachment_data.size)
        {
            return Result::ErrorWrite_AttachmentTooBig;
        }

        PostcardHeader const header{
            .revision = info.revision,
            .attachment_size = ice::postcard::u32(attachment_data.size)
        };

        ice::postcard::u8 last_channel_written = 0;
        ice::postcard::usize const offset = detail::write_postcard_data(
            image.data,
            { &header, sizeof(header) },
            image.channels,
            last_channel_written
        );
        detail::write_postcard_data(
            { reinterpret_cast<ice::postcard::u8*>(image.data.location) + offset, image.data.size - offset },
            attachment_data,
            image.channels,
            last_channel_written
        );
        return Result::Success;
    }

    auto read_info(
        ice::postcard::Image const& image,
        ice::postcard::PostcardInfo& out_info
    ) noexcept -> ice::postcard::Result
    {
        using ice::postcard::Result;

        PostcardHeader header{ };
        ice::postcard::u8 last_channel_read = 0;
        detail::read_postcard_data(
            { &header, sizeof(header) },
            { image.data.location, image.data.size },
            image.channels,
            last_channel_read
        );

        if (header.magic != PostcardHeader::Constant_Magic)
        {
            return Result::ErrorRead_AttachmentNotFound;
        }

        out_info.revision = header.revision;
        out_info.attachment_size = header.attachment_size;
        return Result::Success;
    }

    auto read(
        ice::postcard::Image const& image,
        ice::postcard::PostcardInfo& out_info,
        ice::postcard::Attachment& out_attachment,
        ice::postcard::Allocator& allocator /*= ice::postcard::Allocator::get_default()*/
    ) noexcept -> ice::postcard::Result
    {
        Result const result = read(image, out_info, out_attachment._data, allocator);
        if (result == Result::Success)
        {
            out_attachment._allocator = &allocator;
        }
        return result;
    }

    auto read(
        ice::postcard::Image const& image,
        ice::postcard::PostcardInfo& out_info,
        ice::postcard::Memory& out_attachment_data,
        ice::postcard::Allocator& allocator /*= ice::postcard::Allocator::get_default()*/
    ) noexcept -> ice::postcard::Result
    {
        using ice::postcard::Result;

        PostcardHeader header{ };
        ice::postcard::u8 last_channel_read = 0;
        ice::postcard::usize const offset = detail::read_postcard_data(
            { &header, sizeof(header) },
            { image.data.location, image.data.size },
            image.channels,
            last_channel_read
        );

        if (header.magic != PostcardHeader::Constant_Magic)
        {
            return Result::ErrorRead_AttachmentNotFound;
        }

        ice::postcard::Memory result = allocator.allocate(header.attachment_size);
        detail::read_postcard_data(
            result,
            { reinterpret_cast<ice::postcard::u8 const*>(image.data.location) + offset,  image.data.size - offset },
            image.channels,
            last_channel_read
        );

        out_info.revision = header.revision;
        out_info.attachment_size = header.attachment_size;
        out_attachment_data = result;
        return Result::Success;
    }

#if ICE_POSTCARD_SIMD_ENABLED
    auto detail::simd::write_postcard_data(
        ice::postcard::Memory target,
        ice::postcard::Data source,
        ice::u8 channel_count,
        ice::u8& out_last_written_channel
    ) noexcept -> ice::usize
    {
        // Used to calculate final offset after all data is written.
        ice::u16 const* const source_words = reinterpret_cast<ice::u16 const*>(source.location);
        ice::u8* destination = reinterpret_cast<ice::u8*>(target.location);
        ice::u8 const* const start = destination;

        static __m128i const sse_mask_copy = _mm_loadu_si128(reinterpret_cast<__m128i const*>(detail::simd::Constant_ComponentCopyMask));
        static __m128i const sse_mask_select = _mm_loadu_si128(reinterpret_cast<__m128i const*>(detail::simd::Constant_BitSelectorMask));
        static __m128i const sse_mask_lsb_clear = _mm_loadu_si128(reinterpret_cast<__m128i const*>(detail::simd::Constant_LSBChannelClearMask));
        static __m128i const sse_mask_lsb_keep = _mm_loadu_si128(reinterpret_cast<__m128i const*>(detail::simd::Constant_LSBChannelKeepMask));

        ice::usize const simd_sse_words = source.size / 2;
        for (ice::usize word_idx = 0; word_idx < simd_sse_words; word_idx += 1)
        {
            // Load the given word.
            __m128i v = _mm_set1_epi16(*(source_words + word_idx));
            // Duplicate both bytes into lower and higher half of the register for 8 entries each (8 bits sxtracted).
            v = _mm_shuffle_epi8(v, sse_mask_copy);
            // Keep only the required bit for each 8bit entry and check if it is set
            v = _mm_and_si128(v, sse_mask_select);
            v = _mm_cmpeq_epi8(v, sse_mask_select);
            // Keep only the LSB bit after the check
            v = _mm_and_si128(v, sse_mask_lsb_keep);

            if (channel_count == 3)
            {
                // Load 16 bytes into the register from the image.
                __m128i imgchan = _mm_loadu_si128(reinterpret_cast<__m128i const*>(destination));
                // Clear the LSB bit so we can set it with out own data.
                imgchan = _mm_and_si128(imgchan, sse_mask_lsb_clear);
                // Add the prevously caclulated data
                imgchan = _mm_add_epi8(imgchan, v);
                // Store back the result into the buffer
                _mm_storeu_si128(reinterpret_cast<__m128i*>(destination), imgchan);

                // Move to the next destination
                destination += 16;
            }
            else
            {
                ice::u8 temp[16];
                _mm_storeu_si128((__m128i*)temp, v);

                for (ice::u8 new_lsb : temp)
                {
                    if (out_last_written_channel == 3)
                    {
                        destination += 1;
                        out_last_written_channel = 0;
                    }
                    destination[0] = (destination[0] & detail::simd::Constant_LSBChannelClearMask[0]) | new_lsb;
                    out_last_written_channel += 1;
                    destination += 1;
                }
            }
        }
        return reinterpret_cast<ice::u8 const*>(destination) - reinterpret_cast<ice::u8 const*>(start);
    }

    auto detail::simd::read_postcard_data(
        ice::postcard::Memory& target,
        ice::postcard::Data source,
        ice::u8 channel_count,
        ice::u8& out_last_read_channel
    ) noexcept -> ice::usize
    {
        // Used to calculate final offset after all data is written.
        ice::u8 const* const source_words = reinterpret_cast<ice::u8 const*>(source.location);
        ice::u8* destination = reinterpret_cast<ice::u8*>(target.location);
        ice::u8 const* const start = destination;

        static __m128i const sse_mask_copy = _mm_loadu_si128(reinterpret_cast<__m128i const*>(detail::simd::Constant_ComponentCopyMask));
        static __m128i const sse_mask_select = _mm_loadu_si128(reinterpret_cast<__m128i const*>(detail::simd::Constant_BitSelectorMask));
        static __m128i const sse_mask_lsb_clear = _mm_loadu_si128(reinterpret_cast<__m128i const*>(detail::simd::Constant_LSBChannelClearMask));
        static __m128i const sse_mask_lsb_keep = _mm_loadu_si128(reinterpret_cast<__m128i const*>(detail::simd::Constant_LSBChannelKeepMask));

        ice::usize bytes_to_read = target.size;
        ice::usize byte_offset = 0;
        // If we don't have at least 3 bytes to read (case when channel_count == 4)
        //   we leave this function and allow the rest to be read with the non-simd version.
        while (bytes_to_read >= 3)
        {
            // Load 32 bytes into registers from the image.
            __m128i v0 = _mm_loadu_si128(reinterpret_cast<__m128i const*>(source_words + byte_offset));
            __m128i v1 = _mm_loadu_si128(reinterpret_cast<__m128i const*>(source_words + byte_offset + 16));

            // Keep the last bit only
            v0 = _mm_and_si128(v0, sse_mask_lsb_keep);
            v1 = _mm_and_si128(v1, sse_mask_lsb_keep);

            // Compare the bits with keep mask, which will set also the msb we want to use in the next operation.
            v0 = _mm_cmpeq_epi8(v0, sse_mask_lsb_keep);
            v1 = _mm_cmpeq_epi8(v1, sse_mask_lsb_keep);

            // Creates a 'mask' out of the bits we have loaded. Which results in the final two bytes we wanted to read.
            ice::u16 const temp0 = _mm_movemask_epi8(v0);
            ice::u16 const temp1 = _mm_movemask_epi8(v1);

            ice::u8 const* temp0u8 = reinterpret_cast<ice::u8 const*>(&temp0);
            ice::u8 const* temp1u8 = reinterpret_cast<ice::u8 const*>(&temp1);
            if (channel_count == 4)
            {
                assert(false);
                // TODO: Fix reading data with simd using 4 channels
                //destination[0] = temp0u8[0];
                //destination[1] = (0xf0 & temp0u8[1]) | (temp1u8[0] & 0x0f);
                //destination[2] = temp1u8[1];
                //destination[0] = (temp0 & 0x00'07) | ((temp0 & 0x00'70) >> 1) | ((temp0 & 0x03'00) >> 2);
                //destination[1] = ((temp0 & 0x04'00) >> 10) | ((temp0 & 0x70'00) >> 11) | ((temp1 & 0x00'07) << 4) | ((temp1 & 0x00'10) << 3);
                //destination[2] = ((temp1 & 0x00'60) >> 5) | ((temp1 & 0x07'00) >> 6) | ((temp1 & 0x70'00) >> 8);
                //destination += 3;
                //bytes_to_read -= 3;
            }
            else // if (channel_count == 3)
            {
                destination[0] = temp0u8[0];
                destination[1] = temp0u8[1];
                destination[2] = temp1u8[0];
                destination[3] = temp1u8[1];
                destination += 4;
                bytes_to_read -= 4;
            }

            byte_offset += 32;
        }

        target.location = destination;
        target.size -= ice::usize(destination - start);
        return byte_offset;
    }
#endif // #if ICE_POSTCARD_SIMD_ENABLED

    auto detail::write_postcard_data(
        ice::postcard::Memory target,
        ice::postcard::Data source,
        ice::postcard::u8 channel_count,
        ice::postcard::u8& out_last_written_channel
    ) noexcept -> ice::postcard::usize
    {
        ice::postcard::usize offset = 0;
        if constexpr (detail::simd::Constant_EnabledSIMD)
        {
            ice::postcard::usize const remainder_bytes = source.size & 0x0000'0003;
            ice::postcard::usize const simd_write_size = source.size - remainder_bytes;
            if (simd_write_size > 0)
            {
                source.size = simd_write_size;
                offset = detail::simd::write_postcard_data(target, source, channel_count, out_last_written_channel);
                target.location = reinterpret_cast<ice::postcard::u8*>(target.location) + offset;
                target.size -= offset;
                source.location = reinterpret_cast<ice::postcard::u8 const*>(source.location) + simd_write_size;
                source.size = remainder_bytes;
            }
        }

        if (source.size > 0)
        {
            // Temporary buffer holding 8 bytes extracted from each bit from an single source byte.
            ice::postcard::u8 temp[8];

            // Used to calculate final offset after all data is written.
            ice::postcard::u8* destination = reinterpret_cast<ice::postcard::u8*>(target.location);
            ice::postcard::u8 const* const start = destination;

            for (ice::postcard::u8 byte : std::span{ reinterpret_cast<ice::postcard::u8 const*>(source.location), source.size })
            {
                temp[0] = (byte & 0b0000'0001) >> 0;
                temp[1] = (byte & 0b0000'0010) >> 1;
                temp[2] = (byte & 0b0000'0100) >> 2;
                temp[3] = (byte & 0b0000'1000) >> 3;
                temp[4] = (byte & 0b0001'0000) >> 4;
                temp[5] = (byte & 0b0010'0000) >> 5;
                temp[6] = (byte & 0b0100'0000) >> 6;
                temp[7] = (byte & 0b1000'0000) >> 7;

                // For images with 4 channels we skip alpha when embedding data.
                if (channel_count == 4)
                {
                    for (ice::postcard::u8 new_lsb : temp)
                    {
                        if (out_last_written_channel == 3)
                        {
                            destination += 1;
                            out_last_written_channel = 0;
                        }
                        destination[0] = (destination[0] & detail::simd::Constant_LSBChannelClearMask[0]) | new_lsb;
                        out_last_written_channel += 1;
                        destination += 1;
                    }
                }
                else
                {
                    for (ice::postcard::u8 new_lsb : temp)
                    {
                        destination[0] = (destination[0] & detail::simd::Constant_LSBChannelClearMask[0]) | new_lsb;
                        destination += 1;
                    }
                }
            }
            offset += ice::postcard::usize(destination - start);
        }
        return offset;
    }

    auto detail::read_postcard_data(
        ice::postcard::Memory target,
        ice::postcard::Data source,
        ice::postcard::u8 channel_count,
        ice::postcard::u8& out_last_read_channel
    ) noexcept -> ice::postcard::usize
    {
        ice::postcard::usize offset = 0;
        if constexpr (detail::simd::Constant_EnabledSIMD)
        {
            ice::postcard::usize const total_source_size = source.size;
            ice::postcard::usize const remainder_bytes = target.size & 0x0000'0003;
            ice::postcard::usize const simd_read_size = target.size - remainder_bytes;

            // TODO: Images with 4 channels, where 'alpha' is ignored do not support simd read, we might want to fix that at some point.
            if (simd_read_size > 0 && channel_count == 3)
            {
                assert(total_source_size > (simd_read_size + remainder_bytes) * 8);
                offset = detail::simd::read_postcard_data(target, source, channel_count, out_last_read_channel);
                source.location = reinterpret_cast<ice::postcard::u8 const*>(source.location) + offset;
                source.size -= offset;
            }
        }

        if (target.size > 0)
        {
            // Used to calculate final offset after all data is written.
            ice::postcard::u8* destination = reinterpret_cast<ice::postcard::u8*>(target.location);
            ice::postcard::u8 const* source_bytes = reinterpret_cast<ice::postcard::u8 const*>(source.location);
            ice::postcard::u8 const* const source_bytes_start = source_bytes;

            for (ice::postcard::u32 idx = 0; idx < target.size; idx += 1)
            {
                // Temporary byte holding a single byte extracted from 8 attachment byte LSB value.
                ice::postcard::u8 temp = 0;

                if (channel_count == 4)
                {
                    for (ice::postcard::u8 bytes_read = 0; bytes_read < 8; bytes_read += 1, source_bytes += 1, out_last_read_channel += 1)
                    {
                        if (out_last_read_channel == 3)
                        {
                            source_bytes += 1;
                            out_last_read_channel = 0;
                        }

                        temp |= (source_bytes[0] & detail::simd::Constant_LSBChannelKeepMask[0]) << bytes_read;
                    }
                }
                else
                {
                    for (ice::postcard::u8 source_byte : std::span{ source_bytes, 8 })
                    {
                        temp <<= 1;
                        temp |= (source_byte & detail::simd::Constant_LSBChannelKeepMask[0]);
                    }

                    source_bytes += 8;
                }
                destination[idx] = temp;
            }

            offset += ice::postcard::usize(source_bytes - source_bytes_start);
        }
        return offset;
    }

} // namespace ice::postcard
