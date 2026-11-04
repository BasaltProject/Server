// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.levelgen;

import java.util.OptionalLong;
import org.basaltmc.bukkit.Unsupported;
import com.mojang.serialization.MapCodec;
import java.util.Optional;
public class WorldOptions {
    protected WorldOptions() {
    }
    public WorldOptions withSeed(OptionalLong seed) {
        throw Unsupported.hit("net.minecraft.world.level.levelgen.WorldOptions.withSeed");
    }
    public static final MapCodec<WorldOptions> CODEC = org.basaltmc.bukkit.facade.LazyCodecs.mapCodec(net.minecraft.world.level.levelgen.WorldOptions.class, "CODEC");
    public static final WorldOptions DEMO_OPTIONS = null;
    public WorldOptions(long seed, boolean generateStructures, boolean generateBonusChest) {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.level.levelgen.WorldOptions#<init>");
    }
    public static WorldOptions defaultWithRandomSeed() {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.level.levelgen.WorldOptions#defaultWithRandomSeed");
    }
    public static WorldOptions testWorldWithRandomSeed() {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.level.levelgen.WorldOptions#testWorldWithRandomSeed");
    }
    private WorldOptions(long seed, boolean generateStructures, boolean generateBonusChest, Optional<String> legacyCustomOptions) {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.level.levelgen.WorldOptions#<init>");
    }
    public long seed() {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.level.levelgen.WorldOptions#seed");
    }
    public boolean generateStructures() {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.level.levelgen.WorldOptions#generateStructures");
    }
    public boolean generateBonusChest() {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.level.levelgen.WorldOptions#generateBonusChest");
    }
    public boolean isOldCustomizedWorld() {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.level.levelgen.WorldOptions#isOldCustomizedWorld");
    }
    public WorldOptions withBonusChest(boolean generateBonusChest) {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.level.levelgen.WorldOptions#withBonusChest");
    }
    public WorldOptions withStructures(boolean generateStructures) {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.level.levelgen.WorldOptions#withStructures");
    }
    public static OptionalLong parseSeed(String seedString) {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.level.levelgen.WorldOptions#parseSeed");
    }
    public static long randomSeed() {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.level.levelgen.WorldOptions#randomSeed");
    }
}
