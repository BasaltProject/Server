// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.levelgen;

import java.util.OptionalLong;
import com.mojang.serialization.MapCodec;
import java.util.Optional;
public class WorldOptions {
    private final long seed;
    private final boolean generateStructures;
    private final boolean generateBonusChest;
    private final Optional<String> legacyCustomOptions;
    protected WorldOptions() {
        this(0L, true, false, Optional.empty());
    }
    public WorldOptions withSeed(OptionalLong seed) {
        return new WorldOptions(seed.orElse(randomSeed()), generateStructures, generateBonusChest, legacyCustomOptions);
    }
    public static final MapCodec<WorldOptions> CODEC = org.basaltmc.bukkit.facade.LazyCodecs.mapCodec(net.minecraft.world.level.levelgen.WorldOptions.class, "CODEC");
    public static final WorldOptions DEMO_OPTIONS = new WorldOptions("North Carolina".hashCode(), true, true);
    public WorldOptions(long seed, boolean generateStructures, boolean generateBonusChest) {
        this(seed, generateStructures, generateBonusChest, Optional.empty());
    }
    public static WorldOptions defaultWithRandomSeed() {
        return new WorldOptions(randomSeed(), true, false);
    }
    public static WorldOptions testWorldWithRandomSeed() {
        return new WorldOptions(randomSeed(), false, false);
    }
    private WorldOptions(long seed, boolean generateStructures, boolean generateBonusChest, Optional<String> legacyCustomOptions) {
        this.seed = seed;
        this.generateStructures = generateStructures;
        this.generateBonusChest = generateBonusChest;
        this.legacyCustomOptions = legacyCustomOptions;
    }
    public long seed() {
        return seed;
    }
    public boolean generateStructures() {
        return generateStructures;
    }
    public boolean generateBonusChest() {
        return generateBonusChest;
    }
    public boolean isOldCustomizedWorld() {
        return legacyCustomOptions.isPresent();
    }
    public WorldOptions withBonusChest(boolean generateBonusChest) {
        return new WorldOptions(seed, generateStructures, generateBonusChest, legacyCustomOptions);
    }
    public WorldOptions withStructures(boolean generateStructures) {
        return new WorldOptions(seed, generateStructures, generateBonusChest, legacyCustomOptions);
    }
    public static OptionalLong parseSeed(String seedString) {
        String trimmed = seedString.trim();
        if (trimmed.isEmpty()) {
            return OptionalLong.empty();
        }
        try {
            return OptionalLong.of(Long.parseLong(trimmed));
        } catch (NumberFormatException e) {
            return OptionalLong.of(trimmed.hashCode());
        }
    }
    public static long randomSeed() {
        return net.minecraft.util.RandomSource.create().nextLong();
    }
}
