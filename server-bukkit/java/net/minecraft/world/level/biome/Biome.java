// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.biome;

import net.minecraft.world.level.LightLayer;
import net.minecraft.world.level.block.state.BlockState;
import net.minecraft.resources.Identifier;
import com.mojang.serialization.Codec;
import com.mojang.serialization.MapCodec;
import net.minecraft.core.BlockPos;
import net.minecraft.core.Holder;
import net.minecraft.core.HolderSet;
import net.minecraft.util.StringRepresentable;
import net.minecraft.world.attribute.EnvironmentAttribute;
import net.minecraft.world.attribute.EnvironmentAttributeMap;
import net.minecraft.world.attribute.modifier.AttributeModifier;
import net.minecraft.world.level.LevelReader;
import net.minecraft.world.level.levelgen.synth.Noise;

public class Biome {
    private final Identifier key;

    public Biome(Identifier key) {
        this.key = key;
    }

    private volatile java.util.Map<String, Object> facts;

    private java.util.Map<String, Object> facts() {
        java.util.Map<String, Object> found = facts;
        if (found == null) {
            byte[] bytes = org.basaltmc.bukkit.LeftoverNatives.registryData("worldgen/biome", key.toString());
            found = bytes == null ? java.util.Map.of() : org.basaltmc.bukkit.item.Nbt.read(bytes);
            facts = found;
        }
        return found;
    }

    public Identifier biomeKey() {
        return key;
    }

    @Override
    public String toString() {
        return key.toString();
    }
    public static final Codec<Biome> DIRECT_CODEC = org.basaltmc.bukkit.facade.LazyCodecs.codec(net.minecraft.world.level.biome.Biome.class, "DIRECT_CODEC");
    public static final Codec<Biome> NETWORK_CODEC = org.basaltmc.bukkit.facade.LazyCodecs.codec(net.minecraft.world.level.biome.Biome.class, "NETWORK_CODEC");
    public static final Codec<Holder<Biome>> CODEC = org.basaltmc.bukkit.facade.LazyCodecs.codec(net.minecraft.world.level.biome.Biome.class, "CODEC");
    public static final Codec<HolderSet<Biome>> LIST_CODEC = org.basaltmc.bukkit.facade.LazyCodecs.codec(net.minecraft.world.level.biome.Biome.class, "LIST_CODEC");
    public static final Noise FROZEN_TEMPERATURE_NOISE = null;
    public static final Noise BIOME_INFO_NOISE = null;
    private Biome(ClimateSettings climateSettings, EnvironmentAttributeMap attributes, BiomeSpecialEffects specialEffects, BiomeGenerationSettings generationSettings) {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.level.biome.Biome#<init>");
    }
    public boolean hasPrecipitation() {
        Object value = facts().get("has_precipitation");
        return value == null || ((Number) value).intValue() != 0;
    }
    public Precipitation getPrecipitationAt(BlockPos pos, int seaLevel) {
        return !hasPrecipitation() ? Precipitation.NONE : coldEnoughToSnow(pos, seaLevel) ? Precipitation.SNOW : Precipitation.RAIN;
    }
        private static boolean coldAt(LevelReader level, BlockPos pos) {
        return level instanceof net.minecraft.server.level.ServerLevel server
            && org.basaltmc.bukkit.WorldEnvNatives.temperature(server.getWorld().getName(), pos.getX(), pos.getY(), pos.getZ()) < 0.15;
    }
    public boolean shouldFreeze(LevelReader level, BlockPos pos) {
        return shouldFreeze(level, pos, true);
    }
    public boolean shouldFreeze(LevelReader level, BlockPos pos, boolean checkNeighbors) {
        if (!coldAt(level, pos) || pos.getY() < level.getMinY() || pos.getY() > level.getMaxY()
            || level.getBrightness(LightLayer.BLOCK, pos) >= 10) {
            return false;
        }
        BlockState state = level.getBlockState(pos);
        if (!state.is(net.minecraft.world.level.block.Blocks.WATER) || state.getFluidState().getAmount() != 8) {
            return false;
        }
        if (!checkNeighbors) {
            return true;
        }
        for (net.minecraft.core.Direction side : net.minecraft.core.Direction.Plane.HORIZONTAL) {
            if (!level.getBlockState(pos.relative(side)).is(net.minecraft.world.level.block.Blocks.WATER)) {
                return true;
            }
        }
        return false;
    }
    public boolean coldEnoughToSnow(BlockPos pos, int seaLevel) {
        return hasPrecipitation() && getBaseTemperature() < 0.15F;
    }
    public boolean warmEnoughToRain(BlockPos pos, int seaLevel) {
        return hasPrecipitation() && getBaseTemperature() >= 0.15F;
    }
    public boolean shouldMeltFrozenOceanIcebergSlightly(BlockPos pos, int seaLevel) {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.level.biome.Biome#shouldMeltFrozenOceanIcebergSlightly");
    }
    public boolean shouldSnow(LevelReader level, BlockPos pos) {
        return coldAt(level, pos) && pos.getY() >= level.getMinY() && pos.getY() <= level.getMaxY()
            && level.getBrightness(LightLayer.BLOCK, pos) < 10 && level.getBlockState(pos).isAir()
            && net.minecraft.world.level.block.Blocks.SNOW.defaultBlockState().canSurvive(level, pos);
    }
    public BiomeGenerationSettings getGenerationSettings() {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.level.biome.Biome#getGenerationSettings");
    }
    public int getGrassColor(double x, double z) {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.level.biome.Biome#getGrassColor");
    }
    public int getFoliageColor() {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.level.biome.Biome#getFoliageColor");
    }
    public int getDryFoliageColor() {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.level.biome.Biome#getDryFoliageColor");
    }
    public float getBaseTemperature() {
        Object value = facts().get("temperature");
        return value instanceof Number number ? number.floatValue() : 0.8F;
    }
    public EnvironmentAttributeMap getAttributes() {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.level.biome.Biome#getAttributes");
    }
    public BiomeSpecialEffects getSpecialEffects() {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.level.biome.Biome#getSpecialEffects");
    }
    public int getWaterColor() {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.level.biome.Biome#getWaterColor");
    }
    public static enum Precipitation implements StringRepresentable {
        NONE, RAIN, SNOW;
        public static final Codec<Precipitation> CODEC = StringRepresentable.fromEnum(Precipitation::values);
        public String getSerializedName() {
            return name().toLowerCase(java.util.Locale.ROOT);
        }
    }
    public static enum TemperatureModifier implements StringRepresentable {
        NONE, FROZEN;
        public static final Codec<TemperatureModifier> CODEC = StringRepresentable.fromEnum(TemperatureModifier::values);
        public float modifyTemperature(BlockPos pos, float baseTemperature) {
            return baseTemperature;
        }
        public String getName() {
            return name().toLowerCase(java.util.Locale.ROOT);
        }
        public String getSerializedName() {
            return name().toLowerCase(java.util.Locale.ROOT);
        }
    }
    public static class BiomeBuilder {
        public BiomeBuilder() {
            throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.level.biome.Biome.BiomeBuilder#<init>");
        }
        public BiomeBuilder hasPrecipitation(boolean hasPrecipitation) {
            throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.level.biome.Biome.BiomeBuilder#hasPrecipitation");
        }
        public BiomeBuilder temperature(float temperature) {
            throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.level.biome.Biome.BiomeBuilder#temperature");
        }
        public BiomeBuilder downfall(float downfall) {
            throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.level.biome.Biome.BiomeBuilder#downfall");
        }
        public BiomeBuilder putAttributes(EnvironmentAttributeMap attributes) {
            throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.level.biome.Biome.BiomeBuilder#putAttributes");
        }
        public BiomeBuilder putAttributes(EnvironmentAttributeMap.Builder attributes) {
            throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.level.biome.Biome.BiomeBuilder#putAttributes");
        }
        public <Value> BiomeBuilder setAttribute(EnvironmentAttribute<Value> attribute, Value value) {
            throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.level.biome.Biome.BiomeBuilder#setAttribute");
        }
        public <Value, Parameter> BiomeBuilder modifyAttribute(EnvironmentAttribute<Value> attribute, AttributeModifier<Value, Parameter> modifier, Parameter value) {
            throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.level.biome.Biome.BiomeBuilder#modifyAttribute");
        }
        public BiomeBuilder specialEffects(BiomeSpecialEffects specialEffects) {
            throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.level.biome.Biome.BiomeBuilder#specialEffects");
        }
        public BiomeBuilder mobSpawnSettings(MobSpawnSettings mobSpawnSettings) {
            throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.level.biome.Biome.BiomeBuilder#mobSpawnSettings");
        }
        public BiomeBuilder generationSettings(BiomeGenerationSettings generationSettings) {
            throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.level.biome.Biome.BiomeBuilder#generationSettings");
        }
        public BiomeBuilder temperatureAdjustment(TemperatureModifier temperatureModifier) {
            throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.level.biome.Biome.BiomeBuilder#temperatureAdjustment");
        }
        public Biome build() {
            throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.level.biome.Biome.BiomeBuilder#build");
        }
        public String toString() {
            return super.toString();
        }
    }
    static record ClimateSettings(boolean hasPrecipitation, float temperature, TemperatureModifier temperatureModifier, float downfall) {
        public static final MapCodec<ClimateSettings> CODEC = org.basaltmc.bukkit.facade.LazyCodecs.mapCodec(net.minecraft.world.level.biome.Biome.ClimateSettings.class, "CODEC");
    }
}
