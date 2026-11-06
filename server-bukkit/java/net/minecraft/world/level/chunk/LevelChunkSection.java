// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.chunk;

import net.minecraft.core.Holder;
import net.minecraft.world.level.biome.Biome;
import net.minecraft.world.level.block.Block;
import net.minecraft.world.level.block.state.BlockState;
import net.minecraft.world.level.material.FluidState;
import java.util.function.Predicate;
import net.minecraft.network.FriendlyByteBuf;
import net.minecraft.world.level.biome.BiomeResolver;

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
    private PalettedContainerRO<Holder<Biome>> savedBiomes;
    private int savedEdits;
    private volatile int savedStateEdits;

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
        markBiomesSaved();
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
            markBiomesSaved();
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

    private int biomeEdits() {
        return biomes instanceof PalettedContainer<?> container ? container.edits() : 0;
    }

    public boolean statesEdited() {
        PalettedContainer<BlockState> held = states;
        return held != null && held.edits() != savedStateEdits;
    }

    public void markStatesSaved() {
        PalettedContainer<BlockState> held = states;
        savedStateEdits = held == null ? 0 : held.edits();
    }

    public synchronized void markBiomesSaved() {
        savedBiomes = biomes;
        savedEdits = biomeEdits();
    }

    public synchronized void inheritBiomeState(LevelChunkSection other) {
        savedBiomes = other.savedBiomes;
        savedEdits = other.savedEdits;
    }

    public synchronized boolean biomesChanged(LevelChunkSection base) {
        if (biomes == null) {
            return false;
        }
        PalettedContainerRO<Holder<Biome>> reference = base == null ? null : base.savedBiomes;
        return biomes != reference || (base != null && biomeEdits() != base.savedEdits);
    }

    public Holder<Biome> getNoiseBiome(int quartX, int quartY, int quartZ) {
        return getBiomes().get(quartX, quartY, quartZ);
    }
    private LevelChunkSection(LevelChunkSection source) {
        this.states = source.getStates().copy();
        this.biomes = source.getBiomes().copy();
        recalcBlockCounts();
    }
    public void acquire() {

    }
    public void release() {

    }
    public BlockState setBlockState(int sectionX, int sectionY, int sectionZ, BlockState state, boolean checkThreading) {
        return setBlockState(sectionX, sectionY, sectionZ, state);
    }
    public boolean hasFluid() {
        return maybeHas(state -> !state.getFluidState().isEmpty());
    }
    public boolean isRandomlyTickingBlocks() {
        return tickingBlockCount > 0;
    }
    public boolean isRandomlyTickingFluids() {
        return maybeHas(state -> state.getFluidState().is(net.minecraft.world.level.material.Fluids.LAVA));
    }
    public void read(FriendlyByteBuf buffer) {
        buffer.readShort();
        buffer.readShort();
        getStates().read(buffer);
        readBiomes(buffer);
        recalcBlockCounts();
    }
    public void readBiomes(FriendlyByteBuf buffer) {
        PalettedContainer<Holder<Biome>> fresh = (PalettedContainer<Holder<Biome>>) getBiomes().recreate();
        fresh.read(buffer);
        biomes = fresh;
    }
    public void write(FriendlyByteBuf buffer) {
        int solid = 0;
        int fluids = 0;
        for (int y = 0; y < 16; y++) {
            for (int z = 0; z < 16; z++) {
                for (int x = 0; x < 16; x++) {
                    BlockState state = getBlockState(x, y, z);
                    if (!state.isAir()) {
                        solid++;
                    }
                    if (!state.getFluidState().isEmpty()) {
                        fluids++;
                    }
                }
            }
        }
        buffer.writeShort(solid);
        buffer.writeShort(fluids);
        getStates().write(buffer);
        getBiomes().write(buffer);
    }
    public int getSerializedSize() {
        return 4 + getStates().getSerializedSize() + getBiomes().getSerializedSize();
    }
    public boolean maybeHas(Predicate<BlockState> predicate) {
        if (hasOnlyAir()) {
            return predicate.test(net.minecraft.world.level.block.Blocks.AIR.defaultBlockState());
        }
        for (int y = 0; y < 16; y++) {
            for (int z = 0; z < 16; z++) {
                for (int x = 0; x < 16; x++) {
                    if (predicate.test(getBlockState(x, y, z))) {
                        return true;
                    }
                }
            }
        }
        return false;
    }
    public void fillBiomesFromNoise(BiomeResolver biomeResolver, int quartMinX, int quartMinY, int quartMinZ) {
        PalettedContainer<Holder<Biome>> fresh = getBiomes().recreate();
        for (int x = 0; x < 4; x++) {
            for (int y = 0; y < 4; y++) {
                for (int z = 0; z < 4; z++) {
                    fresh.getAndSetUnchecked(x, y, z, biomeResolver.getNoiseBiome(quartMinX + x, quartMinY + y, quartMinZ + z));
                }
            }
        }
        this.biomes = fresh;
    }
    public LevelChunkSection copy() {
        return new LevelChunkSection(this);
    }
}
