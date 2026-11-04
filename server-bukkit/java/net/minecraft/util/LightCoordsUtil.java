// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.util;

import net.minecraft.core.BlockPos;
import net.minecraft.world.level.BlockAndLightGetter;
import net.minecraft.world.level.LightLayer;
import net.minecraft.world.level.block.state.BlockState;

public class LightCoordsUtil {
    public static final int FULL_BRIGHT = 15728880;
    public static final int FULL_SKY = 15728640;
    private static final int kMaxSmoothLightLevel = 240;
    private static final int kSkyMask = 0xFF0000;

    public static int pack(int block, int sky) {
        return block << 4 | sky << 20;
    }

    public static int block(int packed) {
        return packed >> 4 & 15;
    }

    public static int sky(int packed) {
        return packed >> 20 & 15;
    }

    public static int withBlock(int coords, int block) {
        return coords & kSkyMask | block << 4;
    }

    public static int smoothPack(int block, int sky) {
        return block & 255 | (sky & 255) << 16;
    }

    public static int smoothBlock(int packed) {
        return packed & 255;
    }

    public static int smoothSky(int packed) {
        return packed >> 16 & 255;
    }

    public static int addSmoothBlockEmission(int lightCoords, float blockLightEmission) {
        int emitted = (int) (Mth.clamp(blockLightEmission, 0.0F, 1.0F) * kMaxSmoothLightLevel);
        return smoothPack(Math.min(smoothBlock(lightCoords) + emitted, kMaxSmoothLightLevel), smoothSky(lightCoords));
    }

    public static int max(int coords1, int coords2) {
        return pack(Math.max(block(coords1), block(coords2)), Math.max(sky(coords1), sky(coords2)));
    }

    public static int lightCoordsWithEmission(int lightCoords, int emission) {
        return emission == 0 ? lightCoords : pack(Math.max(block(lightCoords), emission), Math.max(sky(lightCoords), emission));
    }

    private static int fillNeighbor(int neighbor, int center) {
        if (neighbor == 0) {
            return center;
        }
        return sky(neighbor) == 0 ? neighbor | center & kSkyMask : neighbor;
    }

    public static int smoothBlend(int neighbor1, int neighbor2, int neighbor3, int center) {
        if (sky(center) > 2 || block(center) > 2) {
            neighbor1 = fillNeighbor(neighbor1, center);
            neighbor2 = fillNeighbor(neighbor2, center);
            neighbor3 = fillNeighbor(neighbor3, center);
        }
        return neighbor1 + neighbor2 + neighbor3 + center >> 2 & 0xFF00FF;
    }

    public static int smoothWeightedBlend(int coords1, int coords2, int coords3, int coords4, float weight1, float weight2, float weight3,
        float weight4) {
        int sky = (int) (smoothSky(coords1) * weight1 + smoothSky(coords2) * weight2 + smoothSky(coords3) * weight3 + smoothSky(coords4) * weight4);
        int block = (int) (smoothBlock(coords1) * weight1 + smoothBlock(coords2) * weight2 + smoothBlock(coords3) * weight3
            + smoothBlock(coords4) * weight4);
        return smoothPack(block, sky);
    }

    public static int getLightCoords(BlockAndLightGetter level, BlockPos pos) {
        return getLightCoords(BrightnessGetter.DEFAULT, level, level.getBlockState(pos), pos);
    }

    public static int getLightCoords(BrightnessGetter brightnessGetter, BlockAndLightGetter level, BlockState state, BlockPos pos) {
        if (state.emissiveRendering()) {
            return FULL_BRIGHT;
        }
        int packed = brightnessGetter.packedBrightness(level, pos);
        int emission = state.getLightEmission();
        return block(packed) < emission ? withBlock(packed, emission) : packed;
    }

    @FunctionalInterface
    public interface BrightnessGetter {
        BrightnessGetter DEFAULT = (level, pos) -> pack(level.getBrightness(LightLayer.BLOCK, pos), level.getBrightness(LightLayer.SKY, pos));

        int packedBrightness(BlockAndLightGetter level, BlockPos pos);
    }
}
