// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.levelgen.blending;

import com.mojang.serialization.Codec;
import java.util.Optional;
import java.util.Set;
import net.minecraft.core.CompositeDirection;
import net.minecraft.core.Holder;
import net.minecraft.server.level.WorldGenRegion;
import net.minecraft.world.level.LevelHeightAccessor;
import net.minecraft.world.level.WorldGenLevel;
import net.minecraft.world.level.biome.Biome;
public class BlendingData {
    protected BlendingData() {
    }
    protected static final int CELL_WIDTH = 4;
    protected static final int CELL_HEIGHT = 8;
    protected static final int CELL_RATIO = 2;
    protected static final float NO_VALUE = 0;
    private BlendingData(int minSection, int maxSection, Optional<float[]> heights) {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.level.levelgen.blending.BlendingData#<init>");
    }
    public static BlendingData unpack(Packed packed) {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.level.levelgen.blending.BlendingData#unpack");
    }
    public Packed pack() {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.level.levelgen.blending.BlendingData#pack");
    }
    public static BlendingData getOrUpdateBlendingData(WorldGenRegion region, int chunkX, int chunkZ) {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.level.levelgen.blending.BlendingData#getOrUpdateBlendingData");
    }
    public static Set<CompositeDirection.Direction8> sideByGenerationAge(WorldGenLevel region, int chunkX, int chunkZ, boolean wantedOldGen) {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.level.levelgen.blending.BlendingData#sideByGenerationAge");
    }
    protected float getHeight(int cellX, int cellY, int cellZ) {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.level.levelgen.blending.BlendingData#getHeight");
    }
    protected float getDensity(int cellX, int cellY, int cellZ) {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.level.levelgen.blending.BlendingData#getDensity");
    }
    protected void iterateBiomes(int minCellX, int quartY, int minCellZ, BiomeConsumer biomeConsumer) {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.level.levelgen.blending.BlendingData#iterateBiomes");
    }
    protected void iterateHeights(int minCellX, int minCellZ, HeightConsumer heightConsumer) {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.level.levelgen.blending.BlendingData#iterateHeights");
    }
    protected void iterateDensities(int minCellX, int minCellZ, int fromCellY, int toCellY, DensityConsumer densityConsumer) {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.level.levelgen.blending.BlendingData#iterateDensities");
    }
    public LevelHeightAccessor getAreaWithOldGeneration() {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.level.levelgen.blending.BlendingData#getAreaWithOldGeneration");
    }
    public static record Packed(int minSection, int maxSection, Optional<float[]> heights) {
        public static final Codec<Packed> CODEC = org.basaltmc.bukkit.facade.LazyCodecs.codec(net.minecraft.world.level.levelgen.blending.BlendingData.Packed.class, "CODEC");
    }
    protected interface BiomeConsumer {
        void consume(int cellX, int cellZ, Holder<Biome> biome);
    }
    protected interface DensityConsumer {
        void consume(int cellX, int cellY, int cellZ, float density);
    }
    protected interface HeightConsumer {
        void consume(int cellX, int cellZ, float height);
    }
}
