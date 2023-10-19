#pragma once
#include <inttypes.h>

namespace ice::postcard
{

    using u8 = uint8_t;
    using u16 = uint16_t;
    using u32 = uint32_t;
    using u64 = uint64_t;
    using usize = size_t;

    struct Data
    {
        void const* location;
        ice::postcard::usize size;
    };

    struct Memory
    {
        void* location;
        ice::postcard::usize size;
    };

    struct Allocator
    {
        virtual auto allocate(ice::postcard::usize size) noexcept -> ice::postcard::Memory = 0;
        virtual void deallocate(ice::postcard::Memory memory) noexcept = 0;

        static auto get_default() noexcept -> ice::postcard::Allocator&;
    };

    struct Image
    {
        ice::postcard::u32 width;
        ice::postcard::u32 height;
        ice::postcard::u8 channels;
        ice::postcard::Memory data;
    };

    struct PostcardInfo
    {
        ice::postcard::u16 revision = 0;
        ice::postcard::u32 attachment_size = 0;
    };

    struct Attachment
    {
        Attachment() noexcept;
        Attachment(ice::postcard::Data data) noexcept;
        Attachment(ice::postcard::Memory memory, ice::postcard::Allocator& allocator) noexcept;
        ~Attachment() noexcept;

        Attachment(Attachment&& other) noexcept = default;
        auto operator=(Attachment&& other) noexcept -> Attachment & = default;
        Attachment(Attachment const& other) noexcept = delete;
        auto operator=(Attachment const& other) noexcept -> Attachment & = delete;

        ice::postcard::Allocator* _allocator;
        ice::postcard::Memory _data;
    };

    enum class Result : ice::postcard::u8
    {
        Success,
        ErrorRead_AttachmentNotFound,
        ErrorWrite_AttachmentTooBig,
    };

    auto capacity(ice::postcard::Image const& image) noexcept -> ice::postcard::usize;

    auto write(
        ice::postcard::Image& image,
        ice::postcard::PostcardInfo const& info,
        ice::postcard::Attachment const& attachment
    ) noexcept -> ice::postcard::Result;

    auto write(
        ice::postcard::Image& image,
        ice::postcard::PostcardInfo const& info,
        ice::postcard::Data const& attachment_data
    ) noexcept -> ice::postcard::Result;

    auto read_info(
        ice::postcard::Image const& image,
        ice::postcard::PostcardInfo& out_info
    ) noexcept -> ice::postcard::Result;

    auto read(
        ice::postcard::Image const& image,
        ice::postcard::PostcardInfo& out_info,
        ice::postcard::Attachment& out_attachment,
        ice::postcard::Allocator& allocator = ice::postcard::Allocator::get_default()
    ) noexcept -> ice::postcard::Result;

    auto read(
        ice::postcard::Image const& image,
        ice::postcard::PostcardInfo& out_info,
        ice::postcard::Memory& out_attachment_data,
        ice::postcard::Allocator& allocator = ice::postcard::Allocator::get_default()
    ) noexcept -> ice::postcard::Result;

} // namespace ice::postcard
