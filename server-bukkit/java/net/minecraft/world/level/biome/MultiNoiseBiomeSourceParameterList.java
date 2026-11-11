// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.biome;
import com.mojang.serialization.Codec;
import java.util.Map;
import java.util.function.Function;
import java.util.stream.Stream;
import net.minecraft.core.Holder;
import net.minecraft.core.HolderGetter;
import net.minecraft.resources.Identifier;
import net.minecraft.resources.ResourceKey;
import com.google.common.collect.ImmutableList;
import com.mojang.serialization.DataResult;
import com.mojang.serialization.codecs.RecordCodecBuilder;
import java.util.stream.Collectors;
import net.minecraft.resources.RegistryOps;
import net.minecraft.core.registries.Registries;
import java.util.List;
import java.util.Optional;
import com.mojang.datafixers.util.Pair;
public class MultiNoiseBiomeSourceParameterList {
    public static final Codec<MultiNoiseBiomeSourceParameterList> DIRECT_CODEC = RecordCodecBuilder.create(i -> i.group(
        Preset.CODEC.fieldOf("preset").forGetter(list -> list.preset),
        RegistryOps.retrieveGetter(Registries.BIOME)).apply(i, MultiNoiseBiomeSourceParameterList::new));
    public static final Codec<Holder<MultiNoiseBiomeSourceParameterList>> CODEC = org.basaltmc.bukkit.facade.LazyCodecs.codec(net.minecraft.world.level.biome.MultiNoiseBiomeSourceParameterList.class, "CODEC");
    private final Preset preset;
    private final Climate.ParameterList<Holder<Biome>> parameters;
    public MultiNoiseBiomeSourceParameterList(Preset preset, HolderGetter<Biome> biomes) {
        this.preset = preset;
        this.parameters = preset.provider.apply(biomes::getOrThrow);
    }
    public Climate.ParameterList<Holder<Biome>> parameters() {
        return parameters;
    }
    public static Map<Preset, Climate.ParameterList<ResourceKey<Biome>>> knownPresets() {
        return Preset.kByName.values().stream().collect(Collectors.toMap(preset -> preset, preset -> preset.provider().apply(key -> key)));
    }
    public static record Preset(Identifier id, SourceProvider provider) {
        public static final Preset NETHER = new Preset(Identifier.withDefaultNamespace("nether"), new SourceProvider() {
            @Override
            public <T> Climate.ParameterList<T> apply(Function<ResourceKey<Biome>, T> lookup) {
                return new Climate.ParameterList<>(List.of(
                    Pair.of(Climate.parameters(0.0F, 0.0F, 0.0F, 0.0F, 0.0F, 0.0F, 0.0F), lookup.apply(Biomes.NETHER_WASTES)),
                    Pair.of(Climate.parameters(0.0F, -0.5F, 0.0F, 0.0F, 0.0F, 0.0F, 0.0F), lookup.apply(Biomes.SOUL_SAND_VALLEY)),
                    Pair.of(Climate.parameters(0.4F, 0.0F, 0.0F, 0.0F, 0.0F, 0.0F, 0.0F), lookup.apply(Biomes.CRIMSON_FOREST)),
                    Pair.of(Climate.parameters(0.0F, 0.5F, 0.0F, 0.0F, 0.0F, 0.0F, 0.375F), lookup.apply(Biomes.WARPED_FOREST)),
                    Pair.of(Climate.parameters(-0.5F, 0.0F, 0.0F, 0.0F, 0.0F, 0.0F, 0.175F), lookup.apply(Biomes.BASALT_DELTAS))));
            }
        });
        public static final Preset OVERWORLD = new Preset(Identifier.withDefaultNamespace("overworld"), new SourceProvider() {
            @Override
            public <T> Climate.ParameterList<T> apply(Function<ResourceKey<Biome>, T> lookup) {
                ImmutableList.Builder<Pair<Climate.ParameterPoint, T>> built = ImmutableList.builder();
                new OverworldBiomeBuilder().addBiomes(point -> built.add(point.mapSecond(lookup)));
                return new Climate.ParameterList<>(built.build());
            }
        });
        private static final Map<Identifier, Preset> kByName = Stream.of(NETHER, OVERWORLD).collect(Collectors.toMap(Preset::id, preset -> preset));
        public static final Codec<Preset> CODEC = Identifier.CODEC.flatXmap(
            name -> Optional.ofNullable(kByName.get(name)).map(DataResult::success).orElseGet(() -> DataResult.error(() -> "Unknown preset: " + name)),
            preset -> DataResult.success(preset.id));
        public Stream<ResourceKey<Biome>> usedBiomes() {
            return provider.apply(key -> key).values().stream().map(Pair::getSecond).distinct();
        }
        @FunctionalInterface
        interface SourceProvider {
            <T> Climate.ParameterList<T> apply(Function<ResourceKey<Biome>, T> lookup);
        }
    }
}
