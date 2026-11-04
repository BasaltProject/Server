// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.levelgen;

import com.mojang.serialization.Codec;
import java.util.BitSet;
import java.util.Optional;
import net.minecraft.world.level.LevelHeightAccessor;
import net.minecraft.world.level.biome.BiomeResolver;
import net.minecraft.world.level.chunk.ChunkAccess;
import net.minecraft.world.level.chunk.ProtoChunk;
import net.minecraft.world.level.chunk.status.ChunkStatus;
public class BelowZeroRetrogen {
    protected BelowZeroRetrogen() {
    }
    public static final Codec<BelowZeroRetrogen> CODEC = org.basaltmc.bukkit.facade.LazyCodecs.codec(net.minecraft.world.level.levelgen.BelowZeroRetrogen.class, "CODEC");
    public static final LevelHeightAccessor UPGRADE_HEIGHT_ACCESSOR = null;
    private BelowZeroRetrogen(ChunkStatus targetStatus, Optional<BitSet> missingBedrock) {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.level.levelgen.BelowZeroRetrogen#<init>");
    }
    public static void replaceOldBedrock(ProtoChunk chunk) {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.level.levelgen.BelowZeroRetrogen#replaceOldBedrock");
    }
    public void applyBedrockMask(ProtoChunk chunk) {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.level.levelgen.BelowZeroRetrogen#applyBedrockMask");
    }
    public ChunkStatus targetStatus() {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.level.levelgen.BelowZeroRetrogen#targetStatus");
    }
    public boolean hasBedrockHoles() {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.level.levelgen.BelowZeroRetrogen#hasBedrockHoles");
    }
    public boolean hasBedrockHole(int x, int z) {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.level.levelgen.BelowZeroRetrogen#hasBedrockHole");
    }
    public static BiomeResolver getBiomeResolver(BiomeResolver biomeResolver, ChunkAccess protoChunk) {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.level.levelgen.BelowZeroRetrogen#getBiomeResolver");
    }
}
