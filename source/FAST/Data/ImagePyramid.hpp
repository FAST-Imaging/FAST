#pragma once

#include <FAST/Data/SpatialDataObject.hpp>
#include <FAST/Data/Access/Access.hpp>
#include <FAST/Data/Access/ImagePyramidAccess.hpp>
#include <set>

// Forward declare

namespace fast {

class Image;

/**
 * Data object for storing large images as tiled image pyramids.
 * Storage uses virtual memory enabling the images to be larger than
 * the available RAM.
 */
class FAST_EXPORT ImagePyramid : public SpatialDataObject {
    FAST_OBJECT(ImagePyramid)
    public:
        void create(int width, int height, int channels, int levels = -1);
        void create(openslide_t* fileHandle, std::vector<ImagePyramidLevel> levels);
        int getNrOfLevels();
        int getLevelWidth(int level);
        int getLevelHeight(int level);
        int getLevelTileWidth(int level);
        int getLevelTileHeight(int level);
        int getLevelTilesX(int level);
        int getLevelTilesY(int level);
    int getFullWidth();
        int getFullHeight();
        int getNrOfChannels() const;
        void setSpacing(Vector3f spacing);
        Vector3f getSpacing() const;
        ImagePyramidAccess::pointer getAccess(accessType type);
        std::unordered_set<std::string> getDirtyPatches();
        bool isDirtyPatch(const std::string& tileID);
        void setDirtyPatch(int level, int patchIdX, int patchIdY);
        void clearDirtyPatches(std::set<std::string> patches);
        void free(ExecutionDevice::pointer device) override;
        void freeAll() override;
        ~ImagePyramid();
    private:
        ImagePyramid();
        std::vector<ImagePyramidLevel> m_levels;

        openslide_t* m_fileHandle = nullptr;

        int m_channels;
        bool m_initialized;

        std::unordered_set<std::string> m_dirtyPatches;
        static int m_counter;
        std::mutex m_dirtyPatchMutex;
        Vector3f m_spacing = Vector3f::Ones();
};

}
