// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.entity.npc.villager;
import net.minecraft.world.level.biome.Biomes;
import com.mojang.serialization.Codec;
import net.minecraft.core.Holder;
import net.minecraft.core.Registry;
import net.minecraft.network.RegistryFriendlyByteBuf;
import net.minecraft.network.codec.StreamCodec;
import net.minecraft.resources.ResourceKey;
import net.minecraft.world.level.biome.Biome;
import net.minecraft.core.registries.Registries;
public final class VillagerType {
    private static final java.util.Map<ResourceKey<Biome>, ResourceKey<VillagerType>> BY_BIOME = new java.util.HashMap<>();
    @SafeVarargs
    private static void assign(ResourceKey<VillagerType> kind, ResourceKey<Biome>... biomes) {
        for (ResourceKey<Biome> each : biomes) {
            BY_BIOME.put(each, kind);
        }
    }
    private static VillagerType enlist(Registry<VillagerType> registry, ResourceKey<VillagerType> name) {
        return Registry.register(registry, name, new VillagerType());
    }
    public static final ResourceKey<VillagerType> DESERT = ResourceKey.create(Registries.VILLAGER_TYPE, net.minecraft.resources.Identifier.withDefaultNamespace("desert"));
    public static final ResourceKey<VillagerType> JUNGLE = ResourceKey.create(Registries.VILLAGER_TYPE, net.minecraft.resources.Identifier.withDefaultNamespace("jungle"));
    public static final ResourceKey<VillagerType> PLAINS = ResourceKey.create(Registries.VILLAGER_TYPE, net.minecraft.resources.Identifier.withDefaultNamespace("plains"));
    public static final ResourceKey<VillagerType> SAVANNA = ResourceKey.create(Registries.VILLAGER_TYPE, net.minecraft.resources.Identifier.withDefaultNamespace("savanna"));
    public static final ResourceKey<VillagerType> SNOW = ResourceKey.create(Registries.VILLAGER_TYPE, net.minecraft.resources.Identifier.withDefaultNamespace("snow"));
    public static final ResourceKey<VillagerType> SWAMP = ResourceKey.create(Registries.VILLAGER_TYPE, net.minecraft.resources.Identifier.withDefaultNamespace("swamp"));
    public static final ResourceKey<VillagerType> TAIGA = ResourceKey.create(Registries.VILLAGER_TYPE, net.minecraft.resources.Identifier.withDefaultNamespace("taiga"));
    public static final Codec<Holder<VillagerType>> CODEC = org.basaltmc.bukkit.facade.LazyCodecs.codec(net.minecraft.world.entity.npc.villager.VillagerType.class, "CODEC");
    public static final StreamCodec<RegistryFriendlyByteBuf, Holder<VillagerType>> STREAM_CODEC = org.basaltmc.bukkit.facade.LazyCodecs.streamCodec(net.minecraft.world.entity.npc.villager.VillagerType.class, "STREAM_CODEC");
    public static VillagerType bootstrap(Registry<VillagerType> registry) {
        enlist(registry, DESERT);
        enlist(registry, JUNGLE);
        enlist(registry, PLAINS);
        enlist(registry, SAVANNA);
        enlist(registry, SNOW);
        enlist(registry, SWAMP);
        return enlist(registry, TAIGA);
    }
    public static ResourceKey<VillagerType> byBiome(Holder<Biome> biome) {
        return biome.unwrapKey().map(BY_BIOME::get).orElse(VillagerData.DEFAULT_TYPE);


    }
    static {
        assign(VillagerType.DESERT, Biomes.BADLANDS, Biomes.DESERT, Biomes.ERODED_BADLANDS, Biomes.WOODED_BADLANDS);
        assign(VillagerType.JUNGLE, Biomes.BAMBOO_JUNGLE, Biomes.JUNGLE, Biomes.SPARSE_JUNGLE);
        assign(VillagerType.SAVANNA, Biomes.SAVANNA_PLATEAU, Biomes.SAVANNA, Biomes.WINDSWEPT_SAVANNA);
        assign(VillagerType.SNOW, Biomes.DEEP_FROZEN_OCEAN, Biomes.FROZEN_OCEAN, Biomes.FROZEN_RIVER, Biomes.ICE_SPIKES, Biomes.SNOWY_BEACH,
            Biomes.SNOWY_TAIGA, Biomes.SNOWY_PLAINS, Biomes.GROVE, Biomes.SNOWY_SLOPES, Biomes.FROZEN_PEAKS, Biomes.JAGGED_PEAKS);
        assign(VillagerType.SWAMP, Biomes.SWAMP, Biomes.MANGROVE_SWAMP);
        assign(VillagerType.TAIGA, Biomes.OLD_GROWTH_SPRUCE_TAIGA, Biomes.OLD_GROWTH_PINE_TAIGA, Biomes.WINDSWEPT_GRAVELLY_HILLS,
            Biomes.WINDSWEPT_HILLS, Biomes.TAIGA, Biomes.WINDSWEPT_FOREST);
    }
}
