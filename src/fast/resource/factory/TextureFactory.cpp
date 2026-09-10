#include "fast/resource/factory/TextureFactory.h"
#include "fast/resource/type/Texture.h"
#include "spdlog/spdlog.h"

namespace Fast {

// NEI custom inventory icons use 32x32 display-list coordinates even when
// their bundled artwork is HD. Legacy generated resources omit this metadata.
static void ApplyNeiIconScale(Texture& texture, const Ship::ResourceInitData& initData) {
    if (initData.Path.find("textures/icon_item_custom/") != std::string::npos &&
        texture.Type == TextureType::RGBA32bpp && texture.Width > 32 && texture.Height > 32 &&
        texture.Width % 32 == 0 && texture.Height % 32 == 0 &&
        texture.HByteScale == 1.0f && texture.VPixelScale == 1.0f) {
        texture.HByteScale = static_cast<float>(texture.Width) / 32.0f;
        texture.VPixelScale = static_cast<float>(texture.Height) / 32.0f;
    }
}

std::shared_ptr<Ship::IResource>
ResourceFactoryBinaryTextureV0::ReadResource(std::shared_ptr<Ship::File> file,
                                             std::shared_ptr<Ship::ResourceInitData> initData) {
    if (!FileHasValidFormatAndReader(file, initData)) {
        return nullptr;
    }

    auto texture = std::make_shared<Texture>(initData);
    auto reader = std::get<std::shared_ptr<Ship::BinaryReader>>(file->Reader);

    texture->Type = (TextureType)reader->ReadUInt32();
    texture->Width = reader->ReadUInt32();
    texture->Height = reader->ReadUInt32();
    texture->ImageDataSize = reader->ReadUInt32();
    texture->mImageBuffer = file->Buffer;
    texture->ImageData = reinterpret_cast<uint8_t*>(file->Buffer->data() + reader->GetBaseAddress());

    ApplyNeiIconScale(*texture, *initData);

    return texture;
}

std::shared_ptr<Ship::IResource>
ResourceFactoryBinaryTextureV1::ReadResource(std::shared_ptr<Ship::File> file,
                                             std::shared_ptr<Ship::ResourceInitData> initData) {
    if (!FileHasValidFormatAndReader(file, initData)) {
        return nullptr;
    }

    auto texture = std::make_shared<Texture>(initData);
    auto reader = std::get<std::shared_ptr<Ship::BinaryReader>>(file->Reader);

    texture->Type = (TextureType)reader->ReadUInt32();
    texture->Width = reader->ReadUInt32();
    texture->Height = reader->ReadUInt32();
    texture->Flags = reader->ReadUInt32();
    texture->HByteScale = reader->ReadFloat();
    texture->VPixelScale = reader->ReadFloat();
    texture->ImageDataSize = reader->ReadUInt32();
    texture->mImageBuffer = file->Buffer;
    texture->ImageData = reinterpret_cast<uint8_t*>(file->Buffer->data() + reader->GetBaseAddress());

    ApplyNeiIconScale(*texture, *initData);

    return texture;
}
} // namespace Fast
