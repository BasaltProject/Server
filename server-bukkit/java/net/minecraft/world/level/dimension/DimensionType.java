// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.dimension;
import net.minecraft.core.BlockPos;
import com.mojang.serialization.Codec;
import com.mojang.serialization.MapCodec;
import java.nio.file.Path;
import java.util.Optional;
import net.minecraft.core.Holder;
import net.minecraft.core.HolderSet;
import net.minecraft.network.RegistryFriendlyByteBuf;
import net.minecraft.network.codec.StreamCodec;
import net.minecraft.resources.ResourceKey;
import net.minecraft.util.StringRepresentable;
import net.minecraft.util.valueproviders.IntProvider;
import net.minecraft.world.attribute.EnvironmentAttributeMap;
import net.minecraft.world.clock.WorldClock;
import net.minecraft.world.level.CardinalLighting;
import net.minecraft.world.level.Level;
import net.minecraft.world.level.block.Block;
import net.minecraft.world.timeline.Timeline;
public record DimensionType(boolean hasFixedTime, boolean hasSkyLight, boolean hasCeiling, boolean hasEnderDragonFight, double coordinateScale, int minY, int height, int logicalHeight, HolderSet<Block> infiniburn, float ambientLight, MonsterSettings monsterSettings, Skybox skybox, CardinalLighting.Type cardinalLightType, EnvironmentAttributeMap attributes, HolderSet<Timeline> timelines, Optional<Holder<WorldClock>> defaultClock) {
    public static final int BITS_FOR_Y = BlockPos.PACKED_Y_LENGTH;
    public static final int MIN_HEIGHT = 16;
    public static final int Y_SIZE = (1 << BITS_FOR_Y) - 32;
    public static final int MAX_Y = (Y_SIZE >> 1) - 1;
    public static final int MIN_Y = MAX_Y - Y_SIZE + 1;
    public static final int WAY_ABOVE_MAX_Y = MAX_Y << 4;
    public static final int WAY_BELOW_MIN_Y = MIN_Y << 4;
    public static final Codec<DimensionType> DIRECT_CODEC = org.basaltmc.bukkit.facade.LazyCodecs.codec(net.minecraft.world.level.dimension.DimensionType.class, "DIRECT_CODEC");
    public static final Codec<DimensionType> NETWORK_CODEC = org.basaltmc.bukkit.facade.LazyCodecs.codec(net.minecraft.world.level.dimension.DimensionType.class, "NETWORK_CODEC");
    public static final StreamCodec<RegistryFriendlyByteBuf, Holder<DimensionType>> STREAM_CODEC = org.basaltmc.bukkit.facade.LazyCodecs.streamCodec(net.minecraft.world.level.dimension.DimensionType.class, "STREAM_CODEC");
    public static final float[] MOON_BRIGHTNESS_PER_PHASE = new float[] {1.0F, 0.75F, 0.5F, 0.25F, 0.0F, 0.25F, 0.5F, 0.75F};
    public static final Codec<Holder<DimensionType>> CODEC = org.basaltmc.bukkit.facade.LazyCodecs.codec(net.minecraft.world.level.dimension.DimensionType.class, "CODEC");
    public static double getTeleportationScale(DimensionType lastDimensionType, DimensionType newDimensionType) {
        return lastDimensionType.coordinateScale() / newDimensionType.coordinateScale();
    }
    public static Path getStorageFolder(ResourceKey<Level> name, Path baseFolder) {
        return name.identifier().resolveAgainst(baseFolder.resolve("dimensions"));
    }
    public IntProvider monsterSpawnLightTest() {
        return monsterSettings.monsterSpawnLightTest();
    }
    public int monsterSpawnBlockLightLimit() {
        return monsterSettings.monsterSpawnBlockLightLimit();
    }
    public boolean hasEndFlashes() {
        return skybox == Skybox.END;

    }
    public static record MonsterSettings(IntProvider monsterSpawnLightTest, int monsterSpawnBlockLightLimit) {
        public static final MapCodec<MonsterSettings> CODEC = org.basaltmc.bukkit.facade.LazyCodecs.mapCodec(net.minecraft.world.level.dimension.DimensionType.MonsterSettings.class, "CODEC");
    }
    public static enum Skybox implements StringRepresentable {
        NONE, OVERWORLD, END;
        public static final Codec<Skybox> CODEC = org.basaltmc.bukkit.facade.LazyCodecs.codec(net.minecraft.world.level.dimension.DimensionType.Skybox.class, "CODEC");
        public String getSerializedName() {
            return name().toLowerCase(java.util.Locale.ROOT);
        }
    }
}
