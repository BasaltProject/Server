// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.biome;

import com.mojang.serialization.MapCodec;
import java.util.List;
import net.minecraft.core.Holder;
import net.minecraft.core.HolderGetter;
import net.minecraft.core.HolderSet;
import net.minecraft.resources.ResourceKey;
import net.minecraft.world.level.levelgen.GenerationStep;
import net.minecraft.world.level.levelgen.carver.WorldCarver;
import net.minecraft.world.level.levelgen.feature.Feature;
import net.minecraft.world.level.levelgen.placement.PlacedFeature;
public class BiomeGenerationSettings {
    protected BiomeGenerationSettings() {
    }
    public static final BiomeGenerationSettings EMPTY = null;
    public static final MapCodec<BiomeGenerationSettings> CODEC = org.basaltmc.bukkit.facade.LazyCodecs.mapCodec(net.minecraft.world.level.biome.BiomeGenerationSettings.class, "CODEC");
    private BiomeGenerationSettings(HolderSet<WorldCarver> carvers, List<HolderSet<PlacedFeature>> features) {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.level.biome.BiomeGenerationSettings#<init>");
    }
    public Iterable<Holder<WorldCarver>> getCarvers() {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.level.biome.BiomeGenerationSettings#getCarvers");
    }
    public List<Feature> getBoneMealFeatures() {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.level.biome.BiomeGenerationSettings#getBoneMealFeatures");
    }
    public List<HolderSet<PlacedFeature>> features() {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.level.biome.BiomeGenerationSettings#features");
    }
    public boolean hasFeature(PlacedFeature feature) {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.level.biome.BiomeGenerationSettings#hasFeature");
    }
    public static class PlainBuilder {
        public PlainBuilder addFeature(GenerationStep.Decoration step, Holder<PlacedFeature> feature) {
            throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.level.biome.BiomeGenerationSettings.PlainBuilder#addFeature");
        }
        public PlainBuilder addFeature(int index, Holder<PlacedFeature> feature) {
            throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.level.biome.BiomeGenerationSettings.PlainBuilder#addFeature");
        }
        public PlainBuilder addCarver(Holder<WorldCarver> carver) {
            throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.level.biome.BiomeGenerationSettings.PlainBuilder#addCarver");
        }
        public BiomeGenerationSettings build() {
            throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.level.biome.BiomeGenerationSettings.PlainBuilder#build");
        }
    }
    public static class Builder extends PlainBuilder {
        public Builder(HolderGetter<PlacedFeature> placedFeatures, HolderGetter<WorldCarver> worldCarvers) {
            throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.level.biome.BiomeGenerationSettings.Builder#<init>");
        }
        public Builder addFeature(GenerationStep.Decoration step, ResourceKey<PlacedFeature> feature) {
            throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.level.biome.BiomeGenerationSettings.Builder#addFeature");
        }
        public Builder addCarver(ResourceKey<WorldCarver> carver) {
            throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.level.biome.BiomeGenerationSettings.Builder#addCarver");
        }
    }
}
