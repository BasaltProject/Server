// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.biome;

import com.mojang.serialization.MapCodec;
import java.util.ArrayList;
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
    public static final BiomeGenerationSettings EMPTY = new BiomeGenerationSettings(HolderSet.empty(), List.of());
    public static final MapCodec<BiomeGenerationSettings> CODEC = org.basaltmc.bukkit.facade.LazyCodecs.mapCodec(BiomeGenerationSettings.class, "CODEC");
    private final HolderSet<WorldCarver> carvers;
    private final List<HolderSet<PlacedFeature>> features;

    protected BiomeGenerationSettings() {
        this(HolderSet.empty(), List.of());
    }

    private BiomeGenerationSettings(HolderSet<WorldCarver> carvers, List<HolderSet<PlacedFeature>> features) {
        this.carvers = carvers;
        this.features = features;
    }

    @SuppressWarnings("unchecked")
    public Iterable<Holder<WorldCarver>> getCarvers() {
        return (Iterable<Holder<WorldCarver>>) (Iterable<?>) carvers;
    }

    public List<Feature> getBoneMealFeatures() {
        return List.of();
    }

    public List<HolderSet<PlacedFeature>> features() {
        return features;
    }

    public boolean hasFeature(PlacedFeature feature) {
        for (HolderSet<PlacedFeature> step : features) {
            for (Holder<PlacedFeature> candidate : step) {
                if (candidate.value() == feature) {
                    return true;
                }
            }
        }
        return false;
    }

    public static class PlainBuilder {
        private final List<Holder<WorldCarver>> carvers = new ArrayList<>();
        private final List<List<Holder<PlacedFeature>>> features = new ArrayList<>();

        public PlainBuilder addFeature(GenerationStep.Decoration step, Holder<PlacedFeature> feature) {
            return addFeature(step.ordinal(), feature);
        }

        public PlainBuilder addFeature(int index, Holder<PlacedFeature> feature) {
            while (features.size() <= index) {
                features.add(new ArrayList<>());
            }
            features.get(index).add(feature);
            return this;
        }

        public PlainBuilder addCarver(Holder<WorldCarver> carver) {
            carvers.add(carver);
            return this;
        }

        public BiomeGenerationSettings build() {
            List<HolderSet<PlacedFeature>> steps = new ArrayList<>();
            for (List<Holder<PlacedFeature>> step : features) {
                steps.add(HolderSet.direct(step));
            }
            return new BiomeGenerationSettings(HolderSet.direct(carvers), List.copyOf(steps));
        }
    }

    public static class Builder extends PlainBuilder {
        private final HolderGetter<PlacedFeature> placedFeatures;
        private final HolderGetter<WorldCarver> worldCarvers;

        public Builder(HolderGetter<PlacedFeature> placedFeatures, HolderGetter<WorldCarver> worldCarvers) {
            this.placedFeatures = placedFeatures;
            this.worldCarvers = worldCarvers;
        }

        public Builder addFeature(GenerationStep.Decoration step, ResourceKey<PlacedFeature> feature) {
            addFeature(step.ordinal(), placedFeatures.getOrThrow(feature));
            return this;
        }

        public Builder addCarver(ResourceKey<WorldCarver> carver) {
            addCarver(worldCarvers.getOrThrow(carver));
            return this;
        }
    }
}
