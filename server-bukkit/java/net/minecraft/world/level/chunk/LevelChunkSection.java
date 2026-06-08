package net.minecraft.world.level.chunk;

import net.minecraft.core.Holder;
import net.minecraft.world.level.biome.Biome;
import net.minecraft.world.level.block.state.BlockState;
import net.minecraft.world.level.material.FluidState;

public class LevelChunkSection {
    public static final int BIOME_CONTAINER_BITS = 2;
    private short nonEmptyBlockCount;
    private short fluidCount;
    private short tickingBlockCount;
    private short tickingFluidCount;
    private final PalettedContainer<BlockState> states;
    private PalettedContainerRO<Holder<Biome>> biomes;

    public LevelChunkSection(PalettedContainer<BlockState> states, PalettedContainerRO<Holder<Biome>> biomes) {
        this.states = states;
        this.biomes = biomes;
        recalcBlockCounts();
    }

    public LevelChunkSection(PalettedContainer<BlockState> states, PalettedContainer<Holder<Biome>> biomes) {
        this(states, (PalettedContainerRO<Holder<Biome>>) biomes);
    }

    public LevelChunkSection(PalettedContainerFactory factory) {
        this.states = factory.createForBlockStates();
        this.biomes = factory.createForBiomes();
    }

    public BlockState getBlockState(int x, int y, int z) {
        return states.get(x, y, z);
    }

    public FluidState getFluidState(int x, int y, int z) {
        return states.get(x, y, z).getFluidState();
    }

    public BlockState setBlockState(int x, int y, int z, BlockState state) {
        BlockState previous = states.getAndSet(x, y, z, state);
        if (!previous.isAir()) {
            nonEmptyBlockCount--;
            if (previous.isRandomlyTicking()) {
                tickingBlockCount--;
            }
        }
        if (!state.isAir()) {
            nonEmptyBlockCount++;
            if (state.isRandomlyTicking()) {
                tickingBlockCount++;
            }
        }
        return previous;
    }

    public boolean hasOnlyAir() {
        return nonEmptyBlockCount == 0;
    }

    public boolean isRandomlyTicking() {
        return tickingBlockCount > 0 || tickingFluidCount > 0;
    }

    public void recalcBlockCounts() {
        int[] counts = new int[2];
        states.count((state, count) -> {
            if (!state.isAir()) {
                counts[0] += count;
                if (state.isRandomlyTicking()) {
                    counts[1] += count;
                }
            }
        });
        nonEmptyBlockCount = (short) counts[0];
        tickingBlockCount = (short) counts[1];
    }

    public PalettedContainer<BlockState> getStates() {
        return states;
    }

    public PalettedContainerRO<Holder<Biome>> getBiomes() {
        return biomes;
    }

    public Holder<Biome> getNoiseBiome(int quartX, int quartY, int quartZ) {
        return biomes.get(quartX, quartY, quartZ);
    }
}
