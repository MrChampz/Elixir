#include "epch.h"
#include "LunaSVGIconLoader.h"

#include <lunasvg.h>

namespace Elixir
{
    namespace
    {
        class LunaSVGIconContent final : public IconContent
        {
          public:
            explicit LunaSVGIconContent(std::unique_ptr<lunasvg::Document> document)
              : m_Document(std::move(document)) {}

            SIconMetrics GetMetrics() const override
            {
                return {
                    .Size = {
                        std::max(m_Document->width(), 1.0f),
                        std::max(m_Document->height(), 1.0f),
                    }
                };
            }

            SIconBitmap Rasterize(const SIconRasterRequest& request) const override
            {
                auto bitmap = m_Document->renderToBitmap(
                    static_cast<int>(request.PixelSize.x),
                    static_cast<int>(request.PixelSize.y)
                );
                if (bitmap.isNull()) return {};

                bitmap.convertToRGBA();

                SIconBitmap result;
                result.Size = request.PixelSize;
                result.Pixels.resize(static_cast<size_t>(result.Size.x) * result.Size.y * 4);

                // Keep only coverage; the GUI renderer applies the icon color.
                const uint8_t* source = bitmap.data();
                for (uint32_t y = 0; y < result.Size.y; ++y)
                {
                    for (uint32_t x = 0; x < result.Size.x; ++x)
                    {
                        const size_t destinationIndex =
                            (static_cast<size_t>(y) * result.Size.x + x) * 4;
                        const size_t sourceIndex =
                            static_cast<size_t>(y) * bitmap.stride() + x * 4;
                        result.Pixels[destinationIndex] = 255;
                        result.Pixels[destinationIndex + 1] = 255;
                        result.Pixels[destinationIndex + 2] = 255;
                        result.Pixels[destinationIndex + 3] = source[sourceIndex + 3];
                    }
                }

                return result;
            }

          private:
            std::unique_ptr<lunasvg::Document> m_Document;
        };
    }

    Ref<IconContent> LunaSVGIconLoader::Load(const SIconSource& source) const
    {
        auto document = lunasvg::Document::loadFromFile(source.Path.string());
        if (!document)
        {
            EE_CORE_ERROR("Cannot load SVG icon! [Path={0}]", source.Path.string())
            return nullptr;
        }

        return CreateRef<LunaSVGIconContent>(std::move(document));
    }
}
