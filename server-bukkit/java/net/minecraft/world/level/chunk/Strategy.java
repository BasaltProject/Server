package net.minecraft.world.level.chunk;

import net.minecraft.core.IdMap;
import net.minecraft.util.Mth;

public abstract class Strategy<T> {
    private static final Palette.Factory kSingle = SingleValuePalette::create;
    private static final Palette.Factory kLinear = LinearPalette::create;
    private static final Palette.Factory kHash = HashMapPalette::create;
    private static final Configuration kZero = new Configuration.Simple(kSingle, 0);
    private static final Configuration kOne = new Configuration.Simple(kLinear, 1);
    private static final Configuration kTwo = new Configuration.Simple(kLinear, 2);
    private static final Configuration kThree = new Configuration.Simple(kLinear, 3);
    private static final Configuration kFour = new Configuration.Simple(kLinear, 4);
    private static final Configuration kFive = new Configuration.Simple(kHash, 5);
    private static final Configuration kSix = new Configuration.Simple(kHash, 6);
    private static final Configuration kSeven = new Configuration.Simple(kHash, 7);
    private static final Configuration kEight = new Configuration.Simple(kHash, 8);

    private final IdMap<T> globalMap;
    private final GlobalPalette<T> globalPalette;
    protected final int globalPaletteBitsInMemory;
    private final int bitsPerAxis;
    private final int entryCount;

    private Strategy(IdMap<T> globalMap, int bitsPerAxis) {
        this.globalMap = globalMap;
        this.globalPalette = new GlobalPalette<>(globalMap);
        this.globalPaletteBitsInMemory = bitsFor(globalMap.size());
        this.bitsPerAxis = bitsPerAxis;
        this.entryCount = 1 << bitsPerAxis * 3;
    }

    public static <T> Strategy<T> createForBlockStates(IdMap<T> registry) {
        return new Strategy<T>(registry, 4) {
            @Override
            public Configuration getConfigurationForBitCount(int entryBits) {
                return switch (entryBits) {
                    case 0 -> kZero;
                    case 1, 2, 3, 4 -> kFour;
                    case 5 -> kFive;
                    case 6 -> kSix;
                    case 7 -> kSeven;
                    case 8 -> kEight;
                    default -> new Configuration.Global(globalPaletteBitsInMemory, entryBits);
                };
            }
        };
    }

    public static <T> Strategy<T> createForBiomes(IdMap<T> registry) {
        return new Strategy<T>(registry, 2) {
            @Override
            public Configuration getConfigurationForBitCount(int entryBits) {
                return switch (entryBits) {
                    case 0 -> kZero;
                    case 1 -> kOne;
                    case 2 -> kTwo;
                    case 3 -> kThree;
                    default -> new Configuration.Global(globalPaletteBitsInMemory, entryBits);
                };
            }
        };
    }

    public int entryCount() {
        return entryCount;
    }

    public int getIndex(int x, int y, int z) {
        return (y << bitsPerAxis | z) << bitsPerAxis | x;
    }

    public IdMap<T> globalMap() {
        return globalMap;
    }

    public GlobalPalette<T> globalPalette() {
        return globalPalette;
    }

    public abstract Configuration getConfigurationForBitCount(int entryBits);

    public Configuration getConfigurationForPaletteSize(int paletteSize) {
        return getConfigurationForBitCount(bitsFor(paletteSize));
    }

    private static int bitsFor(int count) {
        return Mth.ceillog2(count);
    }
}
