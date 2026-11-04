// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.entity.raid;

import com.mojang.serialization.Codec;
import java.util.ArrayList;
import java.util.List;
import java.util.OptionalInt;
import net.minecraft.core.BlockPos;
import net.minecraft.nbt.CompoundTag;
import net.minecraft.server.level.ServerLevel;
import net.minecraft.server.level.ServerPlayer;
import net.minecraft.world.level.ChunkPos;
import net.minecraft.world.level.saveddata.SavedData;
import net.minecraft.world.level.saveddata.SavedDataType;

/** The raids of one world, read from the native raid system. */
public class Raids extends SavedData {
    public static final Codec<Raids> CODEC = org.basaltmc.bukkit.facade.LazyCodecs.codec(net.minecraft.world.entity.raid.Raids.class, "CODEC");
    public static final SavedDataType<Raids> TYPE = null;

    private final org.bukkit.World world;

    public Raids() {
        this.world = org.bukkit.Bukkit.getWorlds().get(0);
    }

    public Raids(org.bukkit.World world) {
        this.world = world;
    }

    public Raid get(int raidId) {
        for (org.bukkit.Raid each : world.getRaids()) {
            if (each.getId() == raidId) {
                return new Raid(each);
            }
        }
        return null;
    }
    public OptionalInt getId(Raid raid) {
        return raid.nativeId() < 0 ? OptionalInt.empty() : OptionalInt.of(raid.nativeId());
    }
    public void tick(ServerLevel level) {
    }
    public static boolean canJoinRaid(Raider raider) {
        return raider.isAlive() && raider.canJoinRaid();
    }
    public Raid createOrExtendRaid(ServerPlayer player, BlockPos raidPosition) {
        int id = org.basaltmc.bukkit.WorldEnvNatives.raidControl(world.getName(), -1, "create", player.getId(), raidPosition.getX(), raidPosition.getY(), raidPosition.getZ(), 0);
        return id < 0 ? null : get(id);
    }
    public static Raids load(CompoundTag tag) {
        return new Raids();
    }
    public Raid getNearbyRaid(BlockPos pos, int maxDistSqr) {
        org.bukkit.Raid found = world.locateNearestRaid(new org.bukkit.Location(world, pos.getX(), pos.getY(), pos.getZ()),
            (int) Math.ceil(Math.sqrt(maxDistSqr)));
        return found == null ? null : new Raid(found);
    }
    public List<BlockPos> getRaidCentersInChunk(ChunkPos chunkPos) {
        List<BlockPos> out = new ArrayList<>();
        for (org.bukkit.Raid each : world.getRaids()) {
            org.bukkit.Location at = each.getLocation();
            if ((at.getBlockX() >> 4) == chunkPos.x() && (at.getBlockZ() >> 4) == chunkPos.z()) {
                out.add(new BlockPos(at.getBlockX(), at.getBlockY(), at.getBlockZ()));
            }
        }
        return out;
    }
}
