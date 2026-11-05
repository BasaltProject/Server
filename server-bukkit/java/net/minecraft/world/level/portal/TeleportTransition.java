// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.portal;

import java.util.Set;
import net.minecraft.core.BlockPos;
import net.minecraft.network.protocol.game.ClientboundLevelEventPacket;
import net.minecraft.server.level.ServerLevel;
import net.minecraft.server.level.ServerPlayer;
import net.minecraft.world.entity.Entity;
import net.minecraft.world.entity.Relative;
import net.minecraft.world.level.storage.LevelData;
import net.minecraft.world.phys.Vec3;

public record TeleportTransition(ServerLevel newLevel, Vec3 position, Vec3 deltaMovement, float yRot, float xRot,
    boolean missingRespawnBlock, boolean asPassenger, Set<Relative> relatives, PostTeleportTransition postTeleportTransition) {
    public static final PostTeleportTransition DO_NOTHING = entity -> {
    };
    public static final PostTeleportTransition PLAY_PORTAL_SOUND = TeleportTransition::playPortalSound;
    public static final PostTeleportTransition PLACE_PORTAL_TICKET = TeleportTransition::placePortalTicket;

    public TeleportTransition(ServerLevel newLevel, Vec3 pos, Vec3 speed, float yRot, float xRot, PostTeleportTransition postTeleportTransition) {
        this(newLevel, pos, speed, yRot, xRot, Set.of(), postTeleportTransition);
    }

    public TeleportTransition(ServerLevel newLevel, Vec3 pos, Vec3 speed, float yRot, float xRot, Set<Relative> relatives,
        PostTeleportTransition postTeleportTransition) {
        this(newLevel, pos, speed, yRot, xRot, false, false, relatives, postTeleportTransition);
    }

    private static void playPortalSound(Entity entity) {
        if (entity instanceof ServerPlayer player) {
            player.connection.send(new ClientboundLevelEventPacket(1032, BlockPos.ZERO, 0, false));
        }
    }

    private static void placePortalTicket(Entity entity) {
        entity.placePortalTicket(BlockPos.containing(entity.position()));
    }

    public static TeleportTransition createDefault(ServerPlayer player, PostTeleportTransition postTeleportTransition) {
        return atSharedSpawn(player, false, postTeleportTransition);
    }

    public static TeleportTransition missingRespawnBlock(ServerPlayer player, PostTeleportTransition postTeleportTransition) {
        return atSharedSpawn(player, true, postTeleportTransition);
    }

    private static TeleportTransition atSharedSpawn(ServerPlayer player, boolean missingBlock, PostTeleportTransition post) {
        ServerLevel target = player.level().getServer().findRespawnDimension();
        LevelData.RespawnData respawn = target.getRespawnData();
        Vec3 pos = Vec3.atBottomCenterOf(player.adjustSpawnLocation(target, respawn.pos()));
        return new TeleportTransition(target, pos, Vec3.ZERO, respawn.yaw(), respawn.pitch(), missingBlock, false, Set.of(), post);
    }

    public TeleportTransition withRotation(float yRot, float xRot) {
        return new TeleportTransition(newLevel, position, deltaMovement, yRot, xRot, missingRespawnBlock, asPassenger, relatives, postTeleportTransition);
    }

    public TeleportTransition withPosition(Vec3 position) {
        return new TeleportTransition(newLevel, position, deltaMovement, yRot, xRot, missingRespawnBlock, asPassenger, relatives, postTeleportTransition);
    }

    public TeleportTransition transitionAsPassenger() {
        return new TeleportTransition(newLevel, position, deltaMovement, yRot, xRot, missingRespawnBlock, true, relatives, postTeleportTransition);
    }

    @FunctionalInterface
    public interface PostTeleportTransition {
        void onTransition(Entity entity);

        default PostTeleportTransition then(PostTeleportTransition next) {
            return entity -> {
                onTransition(entity);
                next.onTransition(entity);
            };
        }
    }
}
