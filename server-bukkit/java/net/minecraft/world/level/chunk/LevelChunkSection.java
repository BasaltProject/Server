package net.minecraft.world.level.chunk;

import net.minecraft.core.Holder;
import net.minecraft.world.level.biome.Biome;
import net.minecraft.world.level.block.Block;
import net.minecraft.world.level.block.state.BlockState;
import net.minecraft.world.level.material.FluidState;

public class LevelChunkSection {
    public static final int BIOME_CONTAINER_BITS = 2;
    private short nonEmptyBlockCount;
    private short fluidCount;
    private short tickingBlockCount;
    private short tickingFluidCount;
    private PalettedContainer<BlockState> states;
    private PalettedContainerRO<Holder<Biome>> biomes;
    private int[] lazyIds;
    private PalettedContainerFactory lazyFactory;
    private int[] lazyBiomes;
    private int lazyBiomeOffset;

    public LevelChunkSection(PalettedContainer<BlockState> states, PalettedContainerRO<Holder<Biome>> biomes) {
        this.states = states;
        this.biomes = biomes;
        recalcBlockCounts();
    }

    public LevelChunkSection(PalettedContainer<BlockState> states, PalettedContainer<Holder<Biome>> biomes) {
        this(states, (PalettedContainerRO<Holder<Biome>>) biomes);
    }

    public LevelChunkSection(PalettedContainerFactory factory, int[] ids, int uniform,
                             PalettedContainerRO<Holder<Biome>> readyBiomes, int[] biomeCells, int biomeOffset) {
        this.lazyFactory = factory;
        this.biomes = readyBiomes;
        this.lazyBiomes = biomeCells;
        this.lazyBiomeOffset = biomeOffset;
        if (uniform >= 0) {
            BlockState only = Block.stateById(uniform);
            this.states = new PalettedContainer<>(only, factory.blockStatesStrategy());
            this.nonEmptyBlockCount = (short) (only.isAir() ? 0 : 4096);
            this.tickingBlockCount = (short) (only.isRandomlyTicking() ? 4096 : 0);
        } else {
            this.lazyIds = ids;
            this.nonEmptyBlockCount = (short) (anyBlock(ids) ? 1 : 0);
        }
    }

    private static boolean anyBlock(int[] ids) {
        int last = -1;
        boolean air = true;
        for (int id : ids) {
            if (id != last) {
                last = id;
                air = Block.stateById(id).isAir();
            }
            if (!air) {
                return true;
            }
        }
        return false;
    }

    private synchronized void materialize() {
        if (states == null) {
            states = org.basaltmc.bukkit.facade.Chunks.states(lazyFactory, lazyIds, 0);
            lazyIds = null;
            recalcBlockCounts();
        }
    }

    private synchronized void materializeBiomes() {
        if (biomes == null) {
            biomes = org.basaltmc.bukkit.facade.Chunks.biomes(lazyFactory, lazyBiomes, lazyBiomeOffset);
            lazyBiomes = null;
        }
    }

    public LevelChunkSection(PalettedContainerFactory factory) {
        this.states = factory.createForBlockStates();
        this.biomes = factory.createForBiomes();
    }

    public BlockState getBlockState(int x, int y, int z) {
        return getStates().get(x, y, z);
    }

    public FluidState getFluidState(int x, int y, int z) {
        return getStates().get(x, y, z).getFluidState();
    }

    public BlockState setBlockState(int x, int y, int z, BlockState state) {
        BlockState previous = getStates().getAndSet(x, y, z, state);
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
        materialize();
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
        if (states == null) {
            materialize();
        }
        return states;
    }

    public PalettedContainerRO<Holder<Biome>> getBiomes() {
        if (biomes == null) {
            materializeBiomes();
        }
        return biomes;
    }

    public Holder<Biome> getNoiseBiome(int quartX, int quartY, int quartZ) {
        return getBiomes().get(quartX, quartY, quartZ);
    }
}
