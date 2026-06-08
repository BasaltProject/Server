package net.minecraft.world.level.chunk;

import com.mojang.serialization.Codec;
import net.minecraft.core.Holder;
import net.minecraft.core.Registry;
import net.minecraft.core.RegistryAccess;
import net.minecraft.core.registries.Registries;
import net.minecraft.world.level.biome.Biome;
import net.minecraft.world.level.block.Block;
import net.minecraft.world.level.block.Blocks;
import net.minecraft.world.level.block.state.BlockState;

public record PalettedContainerFactory(Strategy<BlockState> blockStatesStrategy, BlockState defaultBlockState,
                                       Codec<PalettedContainer<BlockState>> blockStatesContainerCodec,
                                       Strategy<Holder<Biome>> biomeStrategy, Holder<Biome> defaultBiome,
                                       Codec<PalettedContainerRO<Holder<Biome>>> biomeContainerCodec) {
    private static final java.util.Map<Registry<Biome>, PalettedContainerFactory> kCache =
        new java.util.WeakHashMap<>();

    public static synchronized PalettedContainerFactory create(RegistryAccess registries) {
        Registry<Biome> biomes = registries.lookupOrThrow(Registries.BIOME);
        PalettedContainerFactory found = kCache.get(biomes);
        if (found == null) {
            Strategy<BlockState> blockStrategy = Strategy.createForBlockStates(Block.BLOCK_STATE_REGISTRY);
            Holder<Biome> plains = biomes.get(net.minecraft.resources.Identifier.parse("minecraft:plains"))
                .orElseThrow();
            found = new PalettedContainerFactory(blockStrategy, Blocks.AIR.defaultBlockState(), null,
                Strategy.createForBiomes(biomes.asHolderIdMap()), plains, null);
            kCache.put(biomes, found);
        }
        return found;
    }

    public PalettedContainer<BlockState> createForBlockStates() {
        return new PalettedContainer<>(defaultBlockState, blockStatesStrategy);
    }

    public PalettedContainer<Holder<Biome>> createForBiomes() {
        return new PalettedContainer<>(defaultBiome, biomeStrategy);
    }
}
